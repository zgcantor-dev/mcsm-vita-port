/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/mem.h"
#include "utils/logger.h"

#include <string.h>
#include <malloc.h>
#include <psp2/kernel/clib.h>

void *sceClibMemclr(void *dst, size_t len) {
    return sceClibMemset(dst, 0, len);
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offs) {
    l_warn("mmap(%p, %i, %i, %i, %i, %li)", addr, length, prot, flags, fd, offs);

    if (length <= 0) {
        return MAP_FAILED;
    }
    void* ret= malloc(length);
    memset(ret, 0, length);
    return ret;
}

int munmap(void *addr, size_t length) {
    if (addr) free(addr);
    return 0;
}


void *memmem_soloader(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len) {
    if (!haystack || !needle) {
        return NULL;
    }

    if (needle_len == 0) {
        return (void *)haystack;
    }

    if (haystack_len < needle_len) {
        return NULL;
    }

    const unsigned char *hay = (const unsigned char *)haystack;
    const unsigned char *nee = (const unsigned char *)needle;
    size_t last = haystack_len - needle_len;

    for (size_t i = 0; i <= last; ++i) {
        if (hay[i] == nee[0] && memcmp(hay + i, nee, needle_len) == 0) {
            return (void *)(hay + i);
        }
    }

    return NULL;
}
