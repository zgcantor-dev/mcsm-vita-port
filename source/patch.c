/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  patch.c
 * @brief Patching some of the .so internal functions or bridging them to native
 *        for better compatibility.
 */

#include <kubridge.h>
#include <so_util/so_util.h>

#include <psp2/kernel/clib.h>

#include <stdlib.h>

#include "utils/glutil.h"
#include "utils/logger.h"

extern so_module so_mod;


static so_hook fmod_get_userdata_hook;
static uintptr_t fmod_text_base;

static void *get_fmod_userdata_iface_ptr(void) {
    if (!fmod_text_base)
        return NULL;

    // libfmod.so@0x000CD954
    //   ldr r3, [pc, #0xec] ; [0x000CDA48]
    //   ldr r3, [pc, r3]    ; [0x000CD968 + r3] -> global slot
    //   ldr r3, [r3]        ; FMOD globals
    //   ldr r0, [r3, #0x68] ; userdata iface (may be NULL during startup)
    uint32_t got_off = *(uint32_t *)(fmod_text_base + 0x000CDA48);
    void **global_slot = (void **)(fmod_text_base + 0x000CD968 + got_off);
    if (!global_slot)
        return NULL;

    void *globals_ref = *global_slot;
    if (!globals_ref)
        return NULL;

    void *globals = *(void **)globals_ref;
    if (!globals)
        return NULL;

    return *(void **)((uintptr_t)globals + 0x68);
}

static int fmod_get_userdata_patched(void *instance, void **userdata) {
    if (!instance || !userdata)
        return 31;

    // libfmod.so@0x000CD978 does `ldr r3, [r0]` after loading r0 from
    // the FMOD globals singleton at offset +0x68. Some startup paths call
    // this entrypoint before that field is initialized, causing a null
    // dereference data abort.
    // Mirror the library's own failure path and return FMOD_ERR_INTERNAL (28)
    // instead of letting it crash.
    void *internal_iface = get_fmod_userdata_iface_ptr();
    if (!internal_iface) {
        *userdata = NULL;
        return 28;
    }

    return SO_CONTINUE(int, fmod_get_userdata_hook, instance, userdata);
}

void so_patch_fmod(so_module *mod) {
    if (!mod)
        return;

    fmod_text_base = mod->text_base;

    // JNI_OnLoad+0xA0C in libfmod.so: validates instance/userdata then dereferences instance.
    // Guard null instance to avoid data abort at ldr r3, [r0].
    fmod_get_userdata_hook = hook_addr(mod->text_base + 0x000CD940, (uintptr_t)&fmod_get_userdata_patched);
}
static so_hook begin_static_vertices_hook;
static so_hook begin_static_indices_hook;
static so_hook vertex_buffer_lock_hook;
static so_hook vertex_buffer_platform_lock_hook;
static so_hook vertex_buffer_platform_unlock_hook;
// RenderUtility::Initialize() can request count=0xFFFF and then write
// 16-byte records, requiring up to 1 MiB. Keep a 1 MiB emergency buffer
// for bad pointers returned by BeginStaticVertices.
static uint8_t emergency_static_vertices[0x100000] __attribute__((aligned(16)));
static uint8_t emergency_static_indices[0x20000] __attribute__((aligned(16)));
static uint8_t *dynamic_static_vertices;
static size_t dynamic_static_vertices_size;
static uint8_t *dynamic_static_indices;
static size_t dynamic_static_indices_size;

#define STATIC_VERTEX_RECORD_SIZE 16u
#define STATIC_INDEX_RECORD_SIZE sizeof(uint16_t)
#define MAX_REASONABLE_STATIC_RECORDS 0x100000u

#define T3VB_BUFFER_ID_OFFSET 0x20
#define T3VB_COUNT_OFFSET 0xC0
#define T3VB_STRIDE_OFFSET 0xC4
#define T3VB_MAPPED_PTR_OFFSET 0xD0
#define T3VB_LOCK_DEPTH_OFFSET 0xE0

typedef struct vb_cpu_fallback_entry {
    struct vb_cpu_fallback_entry *next;
    void *vb;
    void *mapped;
    size_t size;
} vb_cpu_fallback_entry;

static vb_cpu_fallback_entry *g_vb_cpu_fallbacks;

static uint32_t vb_read_u32(void *vb, size_t off) {
    return *(uint32_t *)((uintptr_t)vb + off);
}

static void vb_write_u32(void *vb, size_t off, uint32_t value) {
    *(uint32_t *)((uintptr_t)vb + off) = value;
}

static void *vb_read_ptr(void *vb, size_t off) {
    return *(void **)((uintptr_t)vb + off);
}

static void vb_write_ptr(void *vb, size_t off, void *value) {
    *(void **)((uintptr_t)vb + off) = value;
}

static vb_cpu_fallback_entry *find_vb_cpu_fallback(void *vb) {
    vb_cpu_fallback_entry *entry = g_vb_cpu_fallbacks;
    while (entry) {
        if (entry->vb == vb)
            return entry;
        entry = entry->next;
    }
    return NULL;
}

static vb_cpu_fallback_entry *get_vb_cpu_fallback(void *vb) {
    vb_cpu_fallback_entry *entry = find_vb_cpu_fallback(vb);
    if (entry)
        return entry;

    entry = (vb_cpu_fallback_entry *)calloc(1, sizeof(*entry));
    if (!entry)
        return NULL;

    entry->vb = vb;
    entry->next = g_vb_cpu_fallbacks;
    g_vb_cpu_fallbacks = entry;
    return entry;
}

static size_t get_vertex_buffer_size_bytes(void *vb) {
    uint32_t count = vb_read_u32(vb, T3VB_COUNT_OFFSET);
    uint32_t stride = vb_read_u32(vb, T3VB_STRIDE_OFFSET);
    size_t size = (size_t)count * (size_t)stride;

    if (size == 0)
        size = 0x100000;

    return size;
}

static void *ensure_vertex_buffer_cpu_mapping(void *vb, const char *reason_tag) {
    void *mapped = vb_read_ptr(vb, T3VB_MAPPED_PTR_OFFSET);
    if (mapped)
        return mapped;

    size_t required = get_vertex_buffer_size_bytes(vb);
    vb_cpu_fallback_entry *entry = get_vb_cpu_fallback(vb);
    if (!entry) {
        l_error("%s could not allocate fallback tracking entry for vb=%p", reason_tag, vb);
        return NULL;
    }

    if (entry->size < required || !entry->mapped) {
        void *resized = realloc(entry->mapped, required);
        if (!resized) {
            l_error("%s fallback realloc failed for vb=%p size=%u", reason_tag, vb, (unsigned)required);
            return NULL;
        }

        entry->mapped = resized;
        entry->size = required;
        sceClibMemset(entry->mapped, 0, required);
        l_warn("%s forcing CPU vertex-buffer mapping fallback for vb=%p size=%u", reason_tag, vb, (unsigned)required);
    }

    vb_write_ptr(vb, T3VB_MAPPED_PTR_OFFSET, entry->mapped);
    return entry->mapped;
}

static int vertex_buffer_platform_lock_patched(void *vb, int read_only) {
    int ok = SO_CONTINUE(int, vertex_buffer_platform_lock_hook, vb, read_only);
    void *mapped = vb_read_ptr(vb, T3VB_MAPPED_PTR_OFFSET);

    if (!mapped)
        mapped = ensure_vertex_buffer_cpu_mapping(vb, "T3VertexBuffer::PlatformLock");

    uint32_t lock_depth = vb_read_u32(vb, T3VB_LOCK_DEPTH_OFFSET);
    if (mapped && lock_depth == 0) {
        lock_depth = 1;
        vb_write_u32(vb, T3VB_LOCK_DEPTH_OFFSET, lock_depth);
        l_warn("T3VertexBuffer::PlatformLock repaired lock depth for CPU fallback vb=%p", vb);
    }

    if (!mapped || lock_depth == 0) {
        l_error("T3VertexBuffer::PlatformLock failed: vb=%p ro=%d mapped=%p lockDepth=%u", vb, read_only, mapped, lock_depth);
        return 0;
    }

    return ok ? ok : 1;
}

static int vertex_buffer_lock_patched(void *vb, int read_only) {
    int ok = SO_CONTINUE(int, vertex_buffer_lock_hook, vb, read_only);
    void *mapped = vb_read_ptr(vb, T3VB_MAPPED_PTR_OFFSET);

    if (!mapped)
        mapped = ensure_vertex_buffer_cpu_mapping(vb, "T3VertexBuffer::Lock");

    if (!mapped) {
        l_error("T3VertexBuffer::Lock produced NULL mapped pointer: vb=%p ro=%d", vb, read_only);
        return 0;
    }

    return ok ? ok : 1;
}

static int vertex_buffer_platform_unlock_patched(void *vb) {
    vb_cpu_fallback_entry *entry = find_vb_cpu_fallback(vb);
    void *mapped = vb_read_ptr(vb, T3VB_MAPPED_PTR_OFFSET);

    if (!entry || !mapped || mapped != entry->mapped)
        return SO_CONTINUE(int, vertex_buffer_platform_unlock_hook, vb);

    uint32_t buffer_id = vb_read_u32(vb, T3VB_BUFFER_ID_OFFSET);
    size_t upload_size = get_vertex_buffer_size_bytes(vb);
    if (buffer_id && upload_size > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)upload_size, mapped);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    uint32_t lock_depth = vb_read_u32(vb, T3VB_LOCK_DEPTH_OFFSET);
    if (lock_depth > 0)
        lock_depth--;

    vb_write_u32(vb, T3VB_LOCK_DEPTH_OFFSET, lock_depth);
    vb_write_ptr(vb, T3VB_MAPPED_PTR_OFFSET, NULL);

    l_warn("T3VertexBuffer::PlatformUnlock used CPU fallback upload for vb=%p size=%u lockDepth=%u", vb, (unsigned)upload_size, lock_depth);
    return lock_depth == 0;
}

static void *get_dynamic_fallback(uint8_t **storage, size_t *storage_size, size_t required, const char *tag) {
    if (required == 0)
        return NULL;

    if (*storage_size >= required && *storage)
        return *storage;

    void *resized = realloc(*storage, required);
    if (!resized) {
        l_error("%s fallback realloc failed for %u bytes", tag, (unsigned)required);
        return NULL;
    }

    *storage = (uint8_t *)resized;
    *storage_size = required;
    sceClibMemset(*storage, 0, required);
    l_warn("%s using dynamic fallback buffer of %u bytes", tag, (unsigned)required);
    return *storage;
}

static size_t estimate_required_bytes(int count, size_t record_size, size_t emergency_size) {
    if (count < 0)
        return emergency_size;

    size_t records = (size_t)count + 1;
    if (records > MAX_REASONABLE_STATIC_RECORDS)
        records = MAX_REASONABLE_STATIC_RECORDS;

    if (record_size > 0 && records > (SIZE_MAX / record_size))
        return emergency_size;

    size_t required = records * record_size;
    if (required < emergency_size)
        return emergency_size;

    return required;
}

static void *begin_static_vertices_patched(void *vertex_buffer, int format, int count) {
    void *result = SO_CONTINUE(void *, begin_static_vertices_hook, vertex_buffer, format, count);
    uintptr_t result_addr = (uintptr_t)result;

    // libGameEngine writes into this buffer without null-checking. In practice
    // RenderUtility::Initialize() can request very large ranges (e.g. count=0x3FFE),
    // so fallback storage needs to be large enough for those writes.
    //
    // Some failure paths may return small sentinel-like pointers (e.g. 0xFFFF)
    // instead of a valid heap address; treat them as invalid too.
    if (!result || result_addr == UINTPTR_MAX || result_addr < 0x00100000) {
        // The observed initialization paths write 16 bytes per vertex and may
        // touch one extra record. Keep the estimate conservative.
        size_t required = estimate_required_bytes(count, STATIC_VERTEX_RECORD_SIZE, sizeof(emergency_static_vertices));

        if (required <= sizeof(emergency_static_vertices)) {
            sceClibMemset(emergency_static_vertices, 0, sizeof(emergency_static_vertices));
            l_warn("BeginStaticVertices returned bad ptr=0x%08X for format=%d count=%d; using %u-byte emergency scratch buffer.",
                   (unsigned)result_addr, format, count, (unsigned)sizeof(emergency_static_vertices));
            return emergency_static_vertices;
        }

        void *dynamic_fallback = get_dynamic_fallback(&dynamic_static_vertices,
                                                      &dynamic_static_vertices_size,
                                                      required,
                                                      "BeginStaticVertices");
        if (dynamic_fallback)
            return dynamic_fallback;

        l_error("BeginStaticVertices returned bad ptr=0x%08X for format=%d count=%d; estimated %u bytes exceeds emergency buffer and dynamic alloc failed, using emergency scratch buffer.",
                (unsigned)result_addr, format, count, (unsigned)required);
        return emergency_static_vertices;
    }

    return result;
}

static void *begin_static_indices_patched(void *index_buffer, int count) {
    void *result = SO_CONTINUE(void *, begin_static_indices_hook, index_buffer, count);
    uintptr_t result_addr = (uintptr_t)result;

    if (!result || result_addr == UINTPTR_MAX || result_addr < 0x00100000) {
        size_t required = estimate_required_bytes(count, STATIC_INDEX_RECORD_SIZE, sizeof(emergency_static_indices));

        if (required <= sizeof(emergency_static_indices)) {
            sceClibMemset(emergency_static_indices, 0, sizeof(emergency_static_indices));
            l_warn("BeginStaticIndices returned bad ptr=0x%08X for count=%d; using %u-byte emergency CPU index buffer.",
                   (unsigned)result_addr, count, (unsigned)sizeof(emergency_static_indices));
            return emergency_static_indices;
        }

        void *dynamic_fallback = get_dynamic_fallback(&dynamic_static_indices,
                                                      &dynamic_static_indices_size,
                                                      required,
                                                      "BeginStaticIndices");
        if (dynamic_fallback)
            return dynamic_fallback;

        l_error("BeginStaticIndices returned bad ptr=0x%08X for count=%d; estimated %u bytes and no fallback available, using emergency CPU index buffer.",
                (unsigned)result_addr, count, (unsigned)required);
        return emergency_static_indices;
    }

    return result;
}

void so_patch(void) {
    // _ZN14T3VertexBuffer12PlatformLockEb
    vertex_buffer_platform_lock_hook = hook_addr(so_mod.text_base + 0x0057B998, (uintptr_t)&vertex_buffer_platform_lock_patched);

    // _ZN14T3VertexBuffer4LockEb
    vertex_buffer_lock_hook = hook_addr(so_mod.text_base + 0x00B073C8, (uintptr_t)&vertex_buffer_lock_patched);

    // _ZN14T3VertexBuffer14PlatformUnlockEv
    vertex_buffer_platform_unlock_hook = hook_addr(so_mod.text_base + 0x0057BA74, (uintptr_t)&vertex_buffer_platform_unlock_patched);

    // _ZN14RenderGeometry19BeginStaticVerticesER14T3VertexBuffer18RenderVertexFormati
    // vaddr from libGameEngine.so disassembly: 0x00AAC190.
    begin_static_vertices_hook = hook_addr(so_mod.text_base + 0x00AAC190, (uintptr_t)&begin_static_vertices_patched);

    // _ZN14RenderGeometry18BeginStaticIndicesER13T3IndexBufferi
    // vaddr from libGameEngine.so disassembly: 0x00AAC1B4.
    begin_static_indices_hook = hook_addr(so_mod.text_base + 0x00AAC1B4, (uintptr_t)&begin_static_indices_patched);
}
