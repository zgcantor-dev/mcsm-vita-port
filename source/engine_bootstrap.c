#include "engine_bootstrap.h"

#include <psp2/kernel/clib.h>

#include <string.h>
#include <stdlib.h>

#include <so_util/so_util.h>

#include "android_shims.h"
#include "utils/logger.h"
#include "utils/utils.h"

extern so_module so_mod;

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

int start_engine_via_libGameEngine(void) {
    so_module *engine = &so_mod;

    const char *data_root = android_shims_get_data_root();
    _log_printf("[BOOT] data_root=%s\n", data_root ? data_root : "(null)");

    SDL_Android_Init();
    SDL_SetMainReady_REAL();

    char *argv0 = sdl_strdup_local("SDL_app");
    char *argv[] = { argv0, NULL };
    int argc = 1;

    typedef int (*SDLMainFn)(int, char **);
    SDLMainFn sdl_main = (SDLMainFn)so_symbol(engine, "SDL_main");
    if (sdl_main) {
        _log_printf("[BOOT] calling SDL_main from engine\n");
        int ret = sdl_main(argc, argv);
        _log_printf("[BOOT] SDL_main returned %d\n", ret);
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

        _log_printf("[BOOT] calling engine entry: %s @ %p\n", entry_candidates[i], fn);
        ((void (*)(void))fn)();
        _log_printf("[BOOT] engine entry returned\n");
        free(argv0);
        return 0;
    }

    l_warn("ENGINE_OK_CONSTRUCTORS_DONE (no entrypoint found)");
    _log_printf("ENGINE_OK_CONSTRUCTORS_DONE\n");
    free(argv0);
    return 0;
}
