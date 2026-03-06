#ifndef ANDROID_SHIMS_H
#define ANDROID_SHIMS_H

#include <stdint.h>
#include <vitaGL.h>

#include "reimpl/egl.h"

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
void *SDL_AndroidGetJNIEnv(void);
const char *SDL_AndroidGetInternalStoragePath(void);
int SDL_AndroidGetExternalStorageState(void);
int SDL_Android_Init(void);
void SDL_SetMainReady_REAL(void);
int gettid(void);

void *glMapBufferOES_soloader(GLenum target, GLenum access);
GLboolean glUnmapBufferOES_soloader(GLenum target);
void glBindBuffer_soloader(GLenum target, GLuint buffer);
void glBufferData_soloader(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
void glDeleteBuffers_soloader(GLsizei n, const GLuint *buffers);

#ifdef USE_SDL2
#include <SDL2/SDL.h>
SDL_Window *SDL_CreateWindow_logged(const char *title, int x, int y,
                                    int w, int h, Uint32 flags);
SDL_GLContext SDL_GL_CreateContext_logged(SDL_Window *window);
int SDL_GL_MakeCurrent_logged(SDL_Window *window, SDL_GLContext context);
#endif

EGLDisplay egl_shim_get_display(EGLNativeDisplayType display_id);
EGLDisplay eglGetCurrentDisplay(void);
EGLBoolean eglGetConfigs(EGLDisplay display, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLBoolean eglGetConfigAttrib(EGLDisplay display, EGLConfig config, EGLint attribute, EGLint *value);

void *resolve_builtin(const char *name);

#ifdef __cplusplus
}
#endif

#endif
