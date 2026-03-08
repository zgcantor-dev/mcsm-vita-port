#include "engine_bootstrap.h"

#include <psp2/kernel/clib.h>

#include <string.h>
#include <stdlib.h>

#include <so_util/so_util.h>

#include "android_shims.h"
#include "utils/logger.h"
#include "utils/utils.h"

#ifndef ENGINE_ENTRYPOINT
#define ENGINE_ENTRYPOINT "EngineMain"
#endif

static char *sdl_strdup_local(const char *src) {
    size_t len = strlen(src) + 1;
    char *dst = (char *)malloc(len);
    if (dst)
        sceClibMemcpy(dst, src, len);
    return dst;
}

static void sdl_log_sink_noop(void *userdata, int category, SDL_LogPriority priority,
                              const char *message) {
    (void)userdata;
    (void)category;
    (void)priority;
    (void)message;
}

int start_engine_via_module(so_module *engine, const char *tag) {
    if (!engine) {
        l_error("[BOOT] %s: null engine module", tag ? tag : "(unknown)");
        return -1;
    }

    const char *label = tag ? tag : "engine";
    const char *data_root = android_shims_get_data_root();
    _log_printf("[BOOT] data_root=%s\n", data_root ? data_root : "(null)");

    // Avoid PSVita SDL backend default log file output (ux0:/data/SDL_Log.txt).
    SDL_LogSetOutputFunction(sdl_log_sink_noop, NULL);

    SDL_Android_Init();
    SDL_SetMainReady_REAL();

    char *argv0 = sdl_strdup_local("SDL_app");
    char *argv[] = { argv0, NULL };
    int argc = 1;

    typedef int (*SDLMainFn)(int, char **);
    SDLMainFn sdl_main = (SDLMainFn)so_symbol(engine, "SDL_main");
    if (sdl_main) {
        _log_printf("[BOOT] %s: calling SDL_main\n", label);
        int ret = sdl_main(argc, argv);
        _log_printf("[BOOT] %s: SDL_main returned %d\n", label, ret);
        free(argv0);
        return ret;
    }

    const char *entry_candidates[] = {
        "_ZN15Application_SDL3RunEv",
        ENGINE_ENTRYPOINT,
        "EngineMain",
        "GameMain",
        "AppMain",
        NULL,
    };

    for (int i = 0; entry_candidates[i]; i++) {
        void *fn = (void *)so_symbol(engine, entry_candidates[i]);
        if (!fn)
            continue;

        _log_printf("[BOOT] %s: calling engine entry: %s @ %p\n", label, entry_candidates[i], fn);
        ((void (*)(void))fn)();
        _log_printf("[BOOT] %s: engine entry returned\n", label);
        free(argv0);
        return 0;
    }

    l_warn("[BOOT] %s: constructors done but no entrypoint found", label);
    _log_printf("ENGINE_OK_CONSTRUCTORS_DONE\n");
    free(argv0);
    return 0;
}
