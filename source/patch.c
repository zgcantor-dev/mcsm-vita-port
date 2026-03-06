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

#include "utils/logger.h"

extern so_module so_mod;

static so_hook begin_static_vertices_hook;
// RenderUtility::Initialize() can request count=0xFFFF and then write
// 16-byte records, requiring up to 1 MiB. Keep a 1 MiB emergency buffer
// for bad pointers returned by BeginStaticVertices.
static uint8_t emergency_static_vertices[0x100000] __attribute__((aligned(16)));

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
        size_t required = (count >= 0 && count < 0x100000) ? ((size_t)count + 1) * 16 : 0;

        if (required <= sizeof(emergency_static_vertices)) {
            sceClibMemset(emergency_static_vertices, 0, sizeof(emergency_static_vertices));
            l_warn("BeginStaticVertices returned bad ptr=0x%08X for format=%d count=%d; using %u-byte emergency scratch buffer.",
                   (unsigned)result_addr, format, count, (unsigned)sizeof(emergency_static_vertices));
            return emergency_static_vertices;
        }

        l_error("BeginStaticVertices returned bad ptr=0x%08X for format=%d count=%d; estimated %u bytes exceeds emergency buffer.",
                (unsigned)result_addr, format, count, (unsigned)required);
    }

    return result;
}

void so_patch(void) {
    // _ZN14RenderGeometry19BeginStaticVerticesER14T3VertexBuffer18RenderVertexFormati
    // vaddr from libGameEngine.so disassembly: 0x00AAC190.
    begin_static_vertices_hook = hook_addr(so_mod.text_base + 0x00AAC190, (uintptr_t)&begin_static_vertices_patched);
}
