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
#include <psp2/power.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <fios/fios.h>

// Base address for the Android .so to be loaded at
#define LOAD_ADDRESS 0x98000000
#define LOAD_ADDRESS_STEP 0x02000000

extern so_module so_mod;

#ifdef LOAD_SDL2
static so_module sdl2_mod;
#endif
#ifdef LOAD_GAMEENGINE_SO
static so_module gameengine_mod;
#endif
#ifdef LOAD_FMOD
static so_module fmod_mod;
#endif
#ifdef LOAD_FMODSTUDIO
static so_module fmodstudio_mod;
#endif

static void load_so_or_fail(so_module *mod, const char *path, const char *name, uintptr_t load_address) {
    if (!file_exists(path)) {
        l_fatal("Required dependency missing: %s (%s)", name, path);
        fatal_error("Error: missing required dependency %s at %s.", name, path);
    }

    if (so_file_load(mod, path, load_address) < 0) {
        l_fatal("Could not load %s (%s)", name, path);
        fatal_error("Error: could not load %s.", path);
    }

    l_success("Loaded %s.", name);
}

void soloader_init_all() {
	// Launch `app0:configurator.bin` on `-config` init param
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

    // Set default overclock values
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

#ifdef USE_SCELIBC_IO
    if (fios_init(DATA_PATH) == 0)
        l_success("FIOS initialized.");
#endif

    if (!module_loaded("kubridge")) {
        l_fatal("kubridge is not loaded.");
        fatal_error("Error: kubridge.skprx is not installed.");
    }
    l_success("kubridge check passed.");

    uintptr_t load_address = LOAD_ADDRESS;

#ifdef LOAD_FMOD
    load_so_or_fail(&fmod_mod, FMOD_SO, "libfmod.so", load_address);
    load_address += LOAD_ADDRESS_STEP;
#endif

#ifdef LOAD_FMODSTUDIO
    load_so_or_fail(&fmodstudio_mod, FMODSTUDIO_SO, "libfmodstudio.so", load_address);
    load_address += LOAD_ADDRESS_STEP;
#endif

#ifdef LOAD_SDL2
    load_so_or_fail(&sdl2_mod, SDL2_SO, "libSDL2.so", load_address);
    load_address += LOAD_ADDRESS_STEP;
#endif

#ifdef LOAD_GAMEENGINE_SO
    load_so_or_fail(&gameengine_mod, GAMEENGINE_SO, "libGameEngine.so", load_address);
    load_address += LOAD_ADDRESS_STEP;
#endif

    load_so_or_fail(&so_mod, SO_PATH, "main game .so", load_address);

    settings_load();
    l_success("Settings loaded.");

    // Relocate and resolve dependencies first, then the main game module.
#ifdef LOAD_FMOD
    so_relocate(&fmod_mod);
    resolve_imports(&fmod_mod);
#endif
#ifdef LOAD_FMODSTUDIO
    so_relocate(&fmodstudio_mod);
    resolve_imports(&fmodstudio_mod);
#endif
#ifdef LOAD_SDL2
    so_relocate(&sdl2_mod);
    resolve_imports(&sdl2_mod);
#endif
#ifdef LOAD_GAMEENGINE_SO
    so_relocate(&gameengine_mod);
    resolve_imports(&gameengine_mod);
#endif

    so_relocate(&so_mod);
    resolve_imports(&so_mod);
    l_success("SOs relocated and imports resolved.");

    so_patch();
    l_success("SO patched.");

    // Init order matches load order so dependent constructors run first.
#ifdef LOAD_FMOD
    so_flush_caches(&fmod_mod);
    so_initialize(&fmod_mod);
#endif
#ifdef LOAD_FMODSTUDIO
    so_flush_caches(&fmodstudio_mod);
    so_initialize(&fmodstudio_mod);
#endif
#ifdef LOAD_SDL2
    so_flush_caches(&sdl2_mod);
    so_initialize(&sdl2_mod);
#endif
#ifdef LOAD_GAMEENGINE_SO
    so_flush_caches(&gameengine_mod);
    so_initialize(&gameengine_mod);
#endif

    so_flush_caches(&so_mod);
    so_initialize(&so_mod);
    l_success("SO caches flushed and modules initialized.");

    gl_preload();
    l_success("OpenGL preloaded.");

    jni_init();
    l_success("FalsoJNI initialized.");

    controls_init();
    l_success("Controls initialized.");
}
