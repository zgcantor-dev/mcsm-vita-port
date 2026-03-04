#include "android_shims.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>

#include <stdio.h>
#include <string.h>

#include "reimpl/sys.h"

#ifdef USE_SDL2
#include <SDL2/SDL.h>
#endif

static const char *g_data_root = "ux0:data/mcsm";

void android_shims_init(const char *data_root) {
    if (data_root && data_root[0] != '\0')
        g_data_root = data_root;
}

static uint64_t shim_perf_counter(void) {
    return sceKernelGetProcessTimeWide();
}

uint64_t SDL_GetPerformanceCounter(void) {
    return shim_perf_counter();
}

uint64_t SDL_GetPerformanceFrequency(void) {
    return 1000000ULL;
}

uint32_t SDL_GetTicks(void) {
    return (uint32_t)(shim_perf_counter() / 1000ULL);
}

uint64_t SDL_GetTicks64(void) {
    return shim_perf_counter() / 1000ULL;
}

void *SDL_AndroidGetJNIEnv(void) {
    return NULL;
}

const char *SDL_AndroidGetInternalStoragePath(void) {
    return g_data_root;
}

int SDL_AndroidGetExternalStorageState(void) {
    return 1;
}

const char *android_shims_get_data_root(void) {
    return g_data_root;
}

int SDL_Android_Init(void) {
    return 0;
}

void SDL_SetMainReady_REAL(void) {
#ifdef USE_SDL2
    SDL_SetMainReady();
#endif
}

extern int __android_log_print(int prio, const char *tag, const char *fmt, ...);

static builtin_symbol g_builtin_symbols[] = {
    { "SDL_GetPerformanceCounter", (void *)&SDL_GetPerformanceCounter },
    { "SDL_GetPerformanceFrequency", (void *)&SDL_GetPerformanceFrequency },
    { "SDL_GetTicks", (void *)&SDL_GetTicks },
    { "SDL_GetTicks64", (void *)&SDL_GetTicks64 },
    { "SDL_AndroidGetJNIEnv", (void *)&SDL_AndroidGetJNIEnv },
    { "SDL_AndroidGetInternalStoragePath", (void *)&SDL_AndroidGetInternalStoragePath },
    { "SDL_AndroidGetExternalStorageState", (void *)&SDL_AndroidGetExternalStorageState },
    { "SDL_Android_Init", (void *)&SDL_Android_Init },
    { "SDL_SetMainReady_REAL", (void *)&SDL_SetMainReady_REAL },
#ifdef USE_SDL2
    { "SDL_SetMainReady", (void *)&SDL_SetMainReady },
#endif
    { "__android_log_print", (void *)&__android_log_print },
    { "clock_gettime", (void *)&clock_gettime_soloader },
};

void *resolve_builtin(const char *name) {
    for (unsigned int i = 0; i < sizeof(g_builtin_symbols) / sizeof(g_builtin_symbols[0]); i++) {
        if (strcmp(name, g_builtin_symbols[i].name) == 0)
            return g_builtin_symbols[i].addr;
    }

    return NULL;
}
