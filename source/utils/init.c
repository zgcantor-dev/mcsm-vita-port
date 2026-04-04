/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021-2022 Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/init.h"

#include "utils/dialog.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "utils/settings.h"

#include <reimpl/controls.h>

#include <string.h>

#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/net/net.h>
#include <psp2/sysmodule.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <fios/fios.h>

#include "android_shims.h"
#include "engine_bootstrap.h"
#include "fmod_symbols.h"

#define LOAD_ADDRESS 0x98000000
#define LOAD_ADDRESS_STEP 0x02000000


extern so_module so_mod;

#ifdef LOAD_GAMEENGINE_SO
static so_module gameengine_mod;
#endif
static so_module main_trace_mod;
static int main_trace_loaded;

static void load_so_or_fail(so_module *mod, const char *path, const char *name, uintptr_t load_address) {
    if (!file_exists(path)) {
        l_fatal("Required dependency missing: %s (%s)", name, path);
        fatal_error("Error: missing required dependency %s at %s.", name, path);
    }

    if (so_file_load(mod, path, load_address) < 0) {
        l_fatal("Could not load %s (%s)", name, path);
        fatal_error("Error: could not load %s.", path);
    }

    so_log_needed_tree(mod, 1);
    l_success("Loaded %s.", name);
}

static void load_module_or_fail(const char *path, const char *name) {
    if (!file_exists(path)) {
        l_fatal("Required dependency missing: %s (%s)", name, path);
        fatal_error("Error: missing required dependency %s at %s.", name, path);
    }

    int res = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
    if (res < 0) {
        l_fatal("Could not load %s (%s): 0x%x", name, path, res);
        fatal_error("Error: could not load %s.", path);
    }

    l_success("Loaded %s.", name);
}

static int load_module_or_warn(const char *path, const char *name) {
    if (!file_exists(path)) {
        l_warn("Optional dependency missing: %s (%s)", name, path);
        return -1;
    }

    int res = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
    if (res < 0) {
        l_warn("Could not load optional dependency %s (%s): 0x%x", name, path, res);
        return res;
    }

    l_success("Loaded %s.", name);
    return 0;
}


static void ensure_net_for_fmod() {
    int res = sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    if (res < 0)
        l_warn("Could not load NET sysmodule: 0x%x", res);

    int netstat = sceNetShowNetstat();
    if (netstat == SCE_NET_ERROR_ENOTINIT) {
        static unsigned char net_memory[141 * 1024];
        SceNetInitParam initparam = {
            .memory = net_memory,
            .size = sizeof(net_memory),
            .flags = 0,
        };

        int net_res = sceNetInit(&initparam);
        if (net_res < 0)
            l_warn("sceNetInit failed: 0x%x", net_res);
        else
            l_success("SceNet initialized for FMOD dependency.");
    }
}

static void relocate_resolve_init(so_module *mod) {
    so_relocate(mod);
    resolve_imports(mod);
    so_flush_caches(mod);
    so_initialize(mod);
}

static void relocate_resolve_patch_init(so_module *mod) {
    so_relocate(mod);
    resolve_imports(mod);
#ifdef LOAD_FMODSTUDIO_SUPRX
    if (!FMOD_Memory_Initialize) {
        l_fatal("FMOD_Memory_Initialize is unresolved after import resolution.");
        fatal_error("Error: unresolved FMOD imports. Ensure libfmodstudio.suprx is installed at %s.", FMODSTUDIO_SUPRX);
    }
#endif
    so_patch();
    so_flush_caches(mod);
    so_initialize(mod);
}

static void configure_system() {
    sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
    SceAppUtilAppEventParam eventParam;
    sceClibMemset(&eventParam, 0, sizeof(SceAppUtilAppEventParam));
    sceAppUtilReceiveAppEvent(&eventParam);
    if (eventParam.type == 0x05) {
        char buffer[2048];
        sceAppUtilAppEventParseLiveArea(&eventParam, buffer);
        if (strstr(buffer, "-config"))
            sceAppMgrLoadExec("app0:/configurator.bin", NULL, NULL);
    }

    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

#ifdef USE_SCELIBC_IO
    int fios_res = fios_init(DATA_PATH);
    if (fios_res == 0)
        l_success("FIOS initialized.");
    else
        l_warn("FIOS initialization skipped (0x%x)", fios_res);
#endif

    if (!module_loaded("kubridge")) {
        l_fatal("kubridge is not loaded.");
        fatal_error("Error: kubridge.skprx is not installed.");
    }
}

typedef enum BootStage {
    BOOT_STAGE_START = 0,
    BOOT_STAGE_BEFORE_SO_INIT,
    BOOT_STAGE_AFTER_SO_INIT,
    BOOT_STAGE_BEFORE_GL_INIT,
    BOOT_STAGE_AFTER_GL_INIT,
    BOOT_STAGE_BEFORE_GAME_ENTRY,
    BOOT_STAGE_AFTER_GAME_ENTRY,
} BootStage;

static volatile int g_boot_stage = BOOT_STAGE_START;
static volatile int g_boot_heartbeat_enabled = 1;

static int boot_heartbeat_thread(unsigned int arg_size, void *argp) {
    (void)arg_size;
    (void)argp;

    while (g_boot_heartbeat_enabled) {
        l_info("heartbeat: stage=%d", g_boot_stage);
        sceKernelDelayThread(1000 * 1000);
    }

    return sceKernelExitDeleteThread(0);
}

static void set_boot_stage(BootStage stage, const char *message) {
    g_boot_stage = stage;
    l_info("%s", message);
}

void soloader_init_all() {
    set_boot_stage(BOOT_STAGE_START, "main: start");

    SceUID heartbeat_thread = sceKernelCreateThread("boot_heartbeat",
                                                    boot_heartbeat_thread,
                                                    0x10000100,
                                                    0x10000,
                                                    0,
                                                    0,
                                                    NULL);
    if (heartbeat_thread >= 0) {
        sceKernelStartThread(heartbeat_thread, 0, NULL);
    } else {
        l_warn("main: heartbeat thread creation failed: 0x%x", heartbeat_thread);
    }

    set_boot_stage(BOOT_STAGE_BEFORE_SO_INIT, "main: before so init");
    configure_system();
    l_info("logger initialized");

#ifdef DEBUG_TRACE
    so_set_trace_enabled(1);
#endif

    android_shims_init(DATA_PATH);
    jni_init();
    l_info("Vita JNI shim initialized before SO constructors");

    uintptr_t load_address = LOAD_ADDRESS;

#ifdef LOAD_FMODSTUDIO_SUPRX
    // FMOD imports SceNet directly; load/initialize NET through sysmodule first.
    ensure_net_for_fmod();
    load_module_or_warn("vs0:sys/external/libfios2.suprx", "libfios2.suprx");
    load_module_or_warn("vs0:sys/external/libc.suprx", "libc.suprx");
    load_module_or_fail(FMODSTUDIO_SUPRX, "libfmodstudio.suprx");
#endif
#ifdef LOAD_GAMEENGINE_SO
    load_so_or_fail(&gameengine_mod, GAMEENGINE_SO, "libGameEngine.so", load_address);
    load_address += LOAD_ADDRESS_STEP;
#endif
    if (file_exists(SO_PATH)) {
        load_so_or_fail(&main_trace_mod, SO_PATH, "libmain.so", load_address);
        main_trace_loaded = 1;
        load_address += LOAD_ADDRESS_STEP;
    } else {
        l_warn("libmain.so not found at %s (continuing with libGameEngine bootstrap)", SO_PATH);
        main_trace_loaded = 0;
    }

#ifdef LOAD_GAMEENGINE_SO
    so_mod = gameengine_mod;
#endif

#ifdef LOAD_GAMEENGINE_SO
    relocate_resolve_patch_init(&gameengine_mod);
#endif
    if (main_trace_loaded)
        relocate_resolve_init(&main_trace_mod);

    set_boot_stage(BOOT_STAGE_AFTER_SO_INIT, "main: after so init");

    l_success("Engine libraries loaded + initialized.");

    set_boot_stage(BOOT_STAGE_BEFORE_GL_INIT, "main: before gl_init");
    gl_preload();
    set_boot_stage(BOOT_STAGE_AFTER_GL_INIT, "main: after gl_init");
    controls_init();

#ifdef LOAD_GAMEENGINE_SO
    set_boot_stage(BOOT_STAGE_BEFORE_GAME_ENTRY, "main: before game entry");

    int entry_ret = -1;
    if (main_trace_loaded) {
        entry_ret = start_engine_via_module(&main_trace_mod, "libmain.so");
        if (entry_ret != 0)
            l_warn("main: libmain.so entry returned %d, falling back to libGameEngine.so", entry_ret);
    }

    if (!main_trace_loaded || entry_ret != 0)
        entry_ret = start_engine_via_module(&gameengine_mod, "libGameEngine.so");

    l_info("main: engine entry returned %d", entry_ret);
    set_boot_stage(BOOT_STAGE_AFTER_GAME_ENTRY, "main: after game entry");
    g_boot_heartbeat_enabled = 0;
    return;
#endif

    _log_printf("ENGINE_OK_CONSTRUCTORS_DONE\n");
    g_boot_heartbeat_enabled = 0;
    return;
}
