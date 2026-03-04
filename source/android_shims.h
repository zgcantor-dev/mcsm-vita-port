#ifndef ANDROID_SHIMS_H
#define ANDROID_SHIMS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    void *addr;
} builtin_symbol;

void android_shims_init(const char *data_root);
const char *android_shims_get_data_root(void);
#ifndef USE_SDL2
uint64_t SDL_GetPerformanceCounter(void);
uint64_t SDL_GetPerformanceFrequency(void);
uint32_t SDL_GetTicks(void);
uint64_t SDL_GetTicks64(void);
int SDL_SetHint(const char *name, const char *value);
const char *SDL_GetHint(const char *name);
#endif
int SDL_Android_Init(void);
void SDL_SetMainReady_REAL(void);
void *resolve_builtin(const char *name);

#ifdef __cplusplus
}
#endif

#endif
