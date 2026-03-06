#include "android_shims.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reimpl/sys.h"
#include "reimpl/egl.h"
#include "utils/dialog.h"
#include "utils/logger.h"

#ifdef USE_SDL2
#include <SDL2/SDL.h>
#endif

#ifdef USE_SDL2
SDL_Window *SDL_CreateWindow_logged(const char *title, int x, int y,
                                           int w, int h, Uint32 flags) {
    SDL_Window *window = SDL_CreateWindow(title, x, y, w, h, flags);
    const char *err = SDL_GetError();
    l_info("[gl] SDL_CreateWindow -> %p", window);
    if (!window) {
        l_error("[gl] SDL_CreateWindow failed: %s", err ? err : "<no error>");
        fatal_error("SDL_CreateWindow failed: %s", err ? err : "<no error>");
    }
    return window;
}

SDL_GLContext SDL_GL_CreateContext_logged(SDL_Window *window) {
    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    const char *err = SDL_GetError();
    l_info("[gl] SDL_GL_CreateContext(window=%p) -> %p", window, ctx);
    if (!ctx) {
        l_error("[gl] SDL_GL_CreateContext failed: %s", err ? err : "<no error>");
        fatal_error("SDL_GL_CreateContext failed: %s", err ? err : "<no error>");
    }
    return ctx;
}

int SDL_GL_MakeCurrent_logged(SDL_Window *window, SDL_GLContext context) {
    int ret = SDL_GL_MakeCurrent(window, context);
    const char *err = SDL_GetError();
    l_info("[gl] SDL_GL_MakeCurrent(window=%p, context=%p) -> %d", window, context, ret);
    if (ret != 0) {
        l_error("[gl] SDL_GL_MakeCurrent failed: %s", err ? err : "<no error>");
        fatal_error("SDL_GL_MakeCurrent failed: %s", err ? err : "<no error>");
    }
    return ret;
}
#endif

static const char *g_data_root = "ux0:data/com.telltalegames.minecraft100";

void android_shims_init(const char *data_root) {
    if (data_root && data_root[0] != '\0')
        g_data_root = data_root;
}

static uint64_t shim_perf_counter(void) {
    return sceKernelGetProcessTimeWide();
}

#ifndef USE_SDL2
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

typedef int SDL_bool;

#ifndef SDL_FALSE
#define SDL_FALSE 0
#endif

#ifndef SDL_TRUE
#define SDL_TRUE 1
#endif

#ifndef SDL_MAX_HINTS
#define SDL_MAX_HINTS 64
#endif

#ifndef SDL_MAX_HINT_KEY
#define SDL_MAX_HINT_KEY 64
#endif

#ifndef SDL_MAX_HINT_VAL
#define SDL_MAX_HINT_VAL 128
#endif

typedef struct {
    char key[SDL_MAX_HINT_KEY];
    char val[SDL_MAX_HINT_VAL];
    int used;
} hint_entry;

static hint_entry g_hints[SDL_MAX_HINTS];

SDL_bool SDL_SetHint(const char *name, const char *value) {
    if (!name || !*name)
        return SDL_FALSE;

    if (!value)
        value = "";

    for (int i = 0; i < SDL_MAX_HINTS; i++) {
        if (g_hints[i].used && strncmp(g_hints[i].key, name, SDL_MAX_HINT_KEY) == 0) {
            strncpy(g_hints[i].val, value, SDL_MAX_HINT_VAL - 1);
            g_hints[i].val[SDL_MAX_HINT_VAL - 1] = '\0';
            return SDL_TRUE;
        }
    }

    for (int i = 0; i < SDL_MAX_HINTS; i++) {
        if (!g_hints[i].used) {
            g_hints[i].used = 1;
            strncpy(g_hints[i].key, name, SDL_MAX_HINT_KEY - 1);
            g_hints[i].key[SDL_MAX_HINT_KEY - 1] = '\0';
            strncpy(g_hints[i].val, value, SDL_MAX_HINT_VAL - 1);
            g_hints[i].val[SDL_MAX_HINT_VAL - 1] = '\0';
            return SDL_TRUE;
        }
    }

    return SDL_TRUE;
}

const char *SDL_GetHint(const char *name) {
    if (!name || !*name)
        return NULL;

    for (int i = 0; i < SDL_MAX_HINTS; i++) {
        if (g_hints[i].used && strncmp(g_hints[i].key, name, SDL_MAX_HINT_KEY) == 0)
            return g_hints[i].val;
    }

    return NULL;
}
#endif

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

int gettid(void) {
    return (int)sceKernelGetThreadId();
}


typedef struct gl_map_fallback {
    GLenum target;
    GLuint buffer;
    void *ptr;
    GLsizeiptr size;
    GLsizeiptr capacity;
    int shadow_mapped;
    struct gl_map_fallback *next;
} gl_map_fallback;

static gl_map_fallback *g_map_fallbacks;
static GLuint g_bound_array_buffer;
static GLuint g_bound_element_array_buffer;

static GLuint *get_bound_buffer_slot(GLenum target) {
    switch (target) {
        case GL_ARRAY_BUFFER:
            return &g_bound_array_buffer;
        case GL_ELEMENT_ARRAY_BUFFER:
            return &g_bound_element_array_buffer;
        default:
            return NULL;
    }
}

static GLenum get_buffer_binding_enum(GLenum target) {
    switch (target) {
        case GL_ARRAY_BUFFER:
            return GL_ARRAY_BUFFER_BINDING;
        case GL_ELEMENT_ARRAY_BUFFER:
            return GL_ELEMENT_ARRAY_BUFFER_BINDING;
        default:
            return 0;
    }
}

static gl_map_fallback *find_map_fallback(GLenum target, GLuint buffer) {
    for (gl_map_fallback *slot = g_map_fallbacks; slot != NULL; slot = slot->next) {
        if (slot->target == target && slot->buffer == buffer)
            return slot;
    }

    if (buffer != 0)
        return NULL;

    for (gl_map_fallback *slot = g_map_fallbacks; slot != NULL; slot = slot->next) {
        if (slot->target == target)
            return slot;
    }

    return NULL;
}

static gl_map_fallback *alloc_map_fallback_slot(void) {
    gl_map_fallback *slot = (gl_map_fallback *)malloc(sizeof(*slot));
    if (!slot)
        return NULL;

    memset(slot, 0, sizeof(*slot));
    slot->next = g_map_fallbacks;
    g_map_fallbacks = slot;
    return slot;
}

static int ensure_map_fallback_capacity(gl_map_fallback *slot, GLsizeiptr size) {
    if (!slot || size <= 0)
        return 0;

    if (slot->ptr && slot->capacity >= size)
        return 1;

    void *resized = realloc(slot->ptr, (size_t)size);
    if (!resized)
        return 0;

    slot->ptr = resized;
    slot->capacity = size;
    return 1;
}

void glBindBuffer_soloader(GLenum target, GLuint buffer) {
    GLuint *bound_slot = get_bound_buffer_slot(target);
    if (bound_slot)
        *bound_slot = buffer;

    glBindBuffer(target, buffer);
}

void glBufferData_soloader(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) {
    glBufferData(target, size, data, usage);

    GLuint *bound_slot = get_bound_buffer_slot(target);
    if (!bound_slot || *bound_slot == 0)
        return;

    gl_map_fallback *slot = find_map_fallback(target, *bound_slot);
    if (!slot) {
        slot = alloc_map_fallback_slot();
        if (!slot)
            return;
        slot->target = target;
        slot->buffer = *bound_slot;
    }

    slot->size = size;
    if (slot->size > 0 && !ensure_map_fallback_capacity(slot, slot->size))
        l_warn("[gl] glBufferData_soloader could not reserve shadow map buffer: target=0x%X size=%d", target, (int)slot->size);
}

void glDeleteBuffers_soloader(GLsizei n, const GLuint *buffers) {
    if (buffers) {
        for (GLsizei i = 0; i < n; i++) {
            GLuint id = buffers[i];
            gl_map_fallback **cursor = &g_map_fallbacks;
            while (*cursor) {
                gl_map_fallback *slot = *cursor;
                if (slot->buffer == id) {
                    *cursor = slot->next;
                    free(slot->ptr);
                    free(slot);
                    continue;
                }
                cursor = &((*cursor)->next);
            }

            if (g_bound_array_buffer == id)
                g_bound_array_buffer = 0;
            if (g_bound_element_array_buffer == id)
                g_bound_element_array_buffer = 0;
        }
    }

    glDeleteBuffers(n, buffers);
}

void *glMapBufferOES_soloader(GLenum target, GLenum access) {
    GLuint *bound_slot = get_bound_buffer_slot(target);
    GLuint tracked_buffer = bound_slot ? *bound_slot : 0;

    if (tracked_buffer == 0) {
        GLenum binding_enum = get_buffer_binding_enum(target);
        if (binding_enum != 0) {
            GLint queried_buffer = 0;
            glGetIntegerv(binding_enum, &queried_buffer);
            tracked_buffer = (GLuint)queried_buffer;
            if (bound_slot)
                *bound_slot = tracked_buffer;
        }
    }

    GLsizeiptr size = 0;
    GLint queried_size = 0;
    glGetBufferParameteriv(target, GL_BUFFER_SIZE, &queried_size);
    if (queried_size > 0)
        size = queried_size;

    gl_map_fallback *slot = find_map_fallback(target, tracked_buffer);
    if (!slot) {
        slot = alloc_map_fallback_slot();
        if (!slot) {
            l_warn("[gl] glMapBufferOES fallback alloc failed for target=0x%X", target);
            return NULL;
        }
        slot->target = target;
        slot->buffer = tracked_buffer;
    }

    if (size <= 0 && slot->size > 0)
        size = slot->size;

    void *ptr = glMapBuffer(target, access);
    if (ptr) {
        l_info("[gl] glMapBufferOES target=0x%X size=%d nativeMap=1 shadowFallback=0",
               target, (int)size);
        return ptr;
    }

    slot->size = size;
    slot->shadow_mapped = 0;

    if (slot->size <= 0) {
        slot->size = (target == GL_ELEMENT_ARRAY_BUFFER) ? 0x20000 : 0x100000;
        l_warn("[gl] glMapBufferOES target=0x%X had unknown size; forcing CPU fallback size=%d",
               target, (int)slot->size);
    }

    if (!ensure_map_fallback_capacity(slot, slot->size)) {
        l_warn("[gl] glMapBufferOES fallback malloc failed: target=0x%X size=%d", target, (int)slot->size);
        return NULL;
    }

    slot->shadow_mapped = 1;
    l_warn("[gl] glMapBufferOES target=0x%X size=%d nativeMap=0 shadowFallback=1 buffer=%u",
           target, (int)slot->size, tracked_buffer);
    return slot->ptr;
}

GLboolean glUnmapBufferOES_soloader(GLenum target) {
    GLuint *bound_slot = get_bound_buffer_slot(target);
    GLuint tracked_buffer = bound_slot ? *bound_slot : 0;

    if (tracked_buffer == 0) {
        GLenum binding_enum = get_buffer_binding_enum(target);
        if (binding_enum != 0) {
            GLint queried_buffer = 0;
            glGetIntegerv(binding_enum, &queried_buffer);
            tracked_buffer = (GLuint)queried_buffer;
            if (bound_slot)
                *bound_slot = tracked_buffer;
        }
    }

    gl_map_fallback *slot = find_map_fallback(target, tracked_buffer);
    if (slot && slot->shadow_mapped) {
        if (slot->ptr && slot->size > 0)
            glBufferSubData(target, 0, slot->size, slot->ptr);
        slot->shadow_mapped = 0;
        return GL_TRUE;
    }

    return glUnmapBuffer(target);
}

GLboolean glIsVertexArrayOES_soloader(GLuint array) {
    (void)array;
    return GL_FALSE;
}

extern int __android_log_print(int prio, const char *tag, const char *fmt, ...);

static builtin_symbol g_builtin_symbols[] = {
#ifdef USE_SDL2
    { "SDL_CreateWindow", (void *)&SDL_CreateWindow_logged },
    { "SDL_Delay", (void *)&SDL_Delay },
    { "SDL_DestroyWindow", (void *)&SDL_DestroyWindow },
    { "SDL_GL_CreateContext", (void *)&SDL_GL_CreateContext_logged },
    { "SDL_GL_DeleteContext", (void *)&SDL_GL_DeleteContext },
    { "SDL_GL_ExtensionSupported", (void *)&SDL_GL_ExtensionSupported },
    { "SDL_GL_MakeCurrent", (void *)&SDL_GL_MakeCurrent_logged },
    { "SDL_GL_SetAttribute", (void *)&SDL_GL_SetAttribute },
    { "SDL_GL_SwapWindow", (void *)&SDL_GL_SwapWindow },
    { "SDL_GameControllerAddMapping", (void *)&SDL_GameControllerAddMapping },
    { "SDL_GameControllerClose", (void *)&SDL_GameControllerClose },
    { "SDL_GameControllerEventState", (void *)&SDL_GameControllerEventState },
    { "SDL_GameControllerGetAxis", (void *)&SDL_GameControllerGetAxis },
    { "SDL_GameControllerGetButton", (void *)&SDL_GameControllerGetButton },
    { "SDL_GameControllerGetJoystick", (void *)&SDL_GameControllerGetJoystick },
    { "SDL_GameControllerOpen", (void *)&SDL_GameControllerOpen },
    { "SDL_GameControllerUpdate", (void *)&SDL_GameControllerUpdate },
    { "SDL_GetDisplayBounds", (void *)&SDL_GetDisplayBounds },
    { "SDL_GetError", (void *)&SDL_GetError },
    { "SDL_GetHint", (void *)&SDL_GetHint },
    { "SDL_GetNumTouchDevices", (void *)&SDL_GetNumTouchDevices },
    { "SDL_GetPerformanceCounter", (void *)&SDL_GetPerformanceCounter },
    { "SDL_GetPerformanceFrequency", (void *)&SDL_GetPerformanceFrequency },
    { "SDL_GetTicks", (void *)&SDL_GetTicks },
    { "SDL_GetTicks64", (void *)&SDL_GetTicks64 },
    { "SDL_GetWindowID", (void *)&SDL_GetWindowID },
    { "SDL_Init", (void *)&SDL_Init },
    { "SDL_JoystickClose", (void *)&SDL_JoystickClose },
    { "SDL_JoystickEventState", (void *)&SDL_JoystickEventState },
    { "SDL_JoystickGetAttached", (void *)&SDL_JoystickGetAttached },
    { "SDL_JoystickGetAxis", (void *)&SDL_JoystickGetAxis },
    { "SDL_JoystickGetButton", (void *)&SDL_JoystickGetButton },
    { "SDL_JoystickGetHat", (void *)&SDL_JoystickGetHat },
    { "SDL_JoystickName", (void *)&SDL_JoystickName },
    { "SDL_JoystickNameForIndex", (void *)&SDL_JoystickNameForIndex },
    { "SDL_JoystickNumAxes", (void *)&SDL_JoystickNumAxes },
    { "SDL_JoystickNumButtons", (void *)&SDL_JoystickNumButtons },
    { "SDL_JoystickNumHats", (void *)&SDL_JoystickNumHats },
    { "SDL_JoystickOpen", (void *)&SDL_JoystickOpen },
    { "SDL_JoystickUpdate", (void *)&SDL_JoystickUpdate },
    { "SDL_Log", (void *)&SDL_Log },
    { "SDL_NumJoysticks", (void *)&SDL_NumJoysticks },
    { "SDL_PollEvent", (void *)&SDL_PollEvent },
    { "SDL_PushEvent", (void *)&SDL_PushEvent },
    { "SDL_Quit", (void *)&SDL_Quit },
    { "SDL_SetHint", (void *)&SDL_SetHint },
    { "SDL_ShowCursor", (void *)&SDL_ShowCursor },
#else
    { "SDL_AndroidGetJNIEnv", (void *)&SDL_AndroidGetJNIEnv },
    { "SDL_AndroidGetInternalStoragePath", (void *)&SDL_AndroidGetInternalStoragePath },
    { "SDL_AndroidGetExternalStorageState", (void *)&SDL_AndroidGetExternalStorageState },
    { "SDL_SetHint", (void *)&SDL_SetHint },
    { "SDL_GetHint", (void *)&SDL_GetHint },
#endif
    { "SDL_AndroidGetJNIEnv", (void *)&SDL_AndroidGetJNIEnv },
    { "SDL_AndroidGetInternalStoragePath", (void *)&SDL_AndroidGetInternalStoragePath },
    { "SDL_AndroidGetExternalStorageState", (void *)&SDL_AndroidGetExternalStorageState },
    { "SDL_Android_Init", (void *)&SDL_Android_Init },
    { "SDL_SetMainReady_REAL", (void *)&SDL_SetMainReady_REAL },
    { "glMapBufferOES", (void *)&glMapBufferOES_soloader },
    { "glUnmapBufferOES", (void *)&glUnmapBufferOES_soloader },
    { "glIsVertexArrayOES", (void *)&glIsVertexArrayOES_soloader },
    { "eglGetProcAddress", (void *)&eglGetProcAddress },
    { "eglGetCurrentDisplay", (void *)&eglGetCurrentDisplay },
    { "eglGetConfigs", (void *)&eglGetConfigs },
    { "eglGetConfigAttrib", (void *)&eglGetConfigAttrib },
    { "eglGetDisplay", (void *)&egl_shim_get_display },
#ifdef USE_SDL2
    { "SDL_SetMainReady", (void *)&SDL_SetMainReady },
#endif
    { "__android_log_print", (void *)&__android_log_print },
    { "clock_gettime", (void *)&clock_gettime_soloader },
    { "gettid", (void *)&gettid },
};

void *resolve_builtin(const char *name) {
    for (unsigned int i = 0; i < sizeof(g_builtin_symbols) / sizeof(g_builtin_symbols[0]); i++) {
        if (strcmp(name, g_builtin_symbols[i].name) == 0)
            return g_builtin_symbols[i].addr;
    }

    return NULL;
}
