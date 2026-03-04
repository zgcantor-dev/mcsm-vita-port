/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/io.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdarg.h>
#include <psp2/kernel/threadmgr.h>

#define TELLTALE_PKG_NAME "com.telltalegames.minecraft100"
#define TELLTALE_OBB_DIR "/Android/obb/" TELLTALE_PKG_NAME "/"

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#endif

#include "utils/logger.h"
#include "utils/utils.h"

// Includes the following inline utilities:
// int oflags_musl_to_newlib(int flags);
// dirent64_bionic * dirent_newlib_to_bionic(struct dirent* dirent_newlib);
// void stat_newlib_to_bionic(struct stat * src, stat64_bionic * dst);
#include "reimpl/bits/_struct_converters.c"

static int remap_telltale_obb_path(const char *src, char *dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0) {
        return 0;
    }

    const char *basename = NULL;

    if (strcmp(src, OBB_PATH) == 0 || strstr(src, "main.com.telltalegames.minecraft100.obb") != NULL || strstr(src, "main.obb") != NULL) {
        basename = "main.com.telltalegames.minecraft100.obb";
    } else if (strcmp(src, PATCH_OBB_PATH) == 0 || strstr(src, "patch.com.telltalegames.minecraft100.obb") != NULL || strstr(src, "patch.obb") != NULL) {
        basename = "patch.com.telltalegames.minecraft100.obb";
    } else {
        const char *obb_pos = strstr(src, TELLTALE_OBB_DIR);
        if (!obb_pos)
            return 0;

        const char *candidate = obb_pos + strlen(TELLTALE_OBB_DIR);
        if (*candidate == '\0') {
            int written = snprintf(dst, dst_size, "%s", DATA_PATH);
            return written > 0 && (size_t)written < dst_size;
        }

        if (strncmp(candidate, "main", 4) == 0) {
            basename = "main.com.telltalegames.minecraft100.obb";
        } else if (strncmp(candidate, "patch", 5) == 0) {
            basename = "patch.com.telltalegames.minecraft100.obb";
        } else {
            return 0;
        }
    }

    int written = snprintf(dst, dst_size, "%s%s", DATA_PATH, basename);
    return written > 0 && (size_t)written < dst_size;
}

static const char *resolve_android_path(const char *src, char *dst, size_t dst_size) {
    if (strcmp(src, "/proc/cpuinfo") == 0)
        return "app0:/cpuinfo";
    if (strcmp(src, "/proc/meminfo") == 0)
        return "app0:/meminfo";
    if (strcmp(src, "/dev/urandom") == 0)
        return "app0:/urandom";
    if (remap_telltale_obb_path(src, dst, dst_size))
        return dst;
    return src;
}

FILE * fopen_soloader(const char * filename, const char * mode) {
    char remapped_path[PATH_MAX];
    const char *resolved_filename = resolve_android_path(filename, remapped_path, sizeof(remapped_path));

#ifdef USE_SCELIBC_IO
    FILE* ret = sceLibcBridge_fopen(resolved_filename, mode);
#else
    FILE* ret = fopen(resolved_filename, mode);
#endif

    if (ret)
        l_debug("fopen(%s => %s, %s): %p", filename, resolved_filename, mode, ret);
    else
        l_warn("fopen(%s => %s, %s): %p", filename, resolved_filename, mode, ret);

    return ret;
}

int open_soloader(const char * path, int oflag, ...) {
    char remapped_path[PATH_MAX];
    const char *resolved_path = resolve_android_path(path, remapped_path, sizeof(remapped_path));

    mode_t mode = 0666;
    if (((oflag & BIONIC_O_CREAT) == BIONIC_O_CREAT) ||
        ((oflag & BIONIC_O_TMPFILE) == BIONIC_O_TMPFILE)) {
        va_list args;
        va_start(args, oflag);
        mode = (mode_t)(va_arg(args, int));
        va_end(args);
    }

    oflag = oflags_bionic_to_newlib(oflag);
    int ret = open(resolved_path, oflag, mode);
    if (ret >= 0)
        l_debug("open(%s => %s, %x): %i", path, resolved_path, oflag, ret);
    else
        l_warn("open(%s => %s, %x): %i", path, resolved_path, oflag, ret);
    return ret;
}

int fstat_soloader(int fd, stat64_bionic * buf) {
    struct stat st;
    int res = fstat(fd, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("fstat(%i): %i", fd, res);
    return res;
}

int stat_soloader(const char * path, stat64_bionic * buf) {
    char remapped_path[PATH_MAX];
    const char *resolved_path = resolve_android_path(path, remapped_path, sizeof(remapped_path));

    if (strcmp(path, "/system/lib/libOpenSLES.so") == 0) {
        l_debug("stat(%s): returning 0 in case this is a check for OpenSLES support", path);
        return 0;
    }

    struct stat st;
    int res = stat(resolved_path, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("stat(%s => %s): %i", path, resolved_path, res);
    return res;
}

int access_soloader(const char * path, int mode) {
    char remapped_path[PATH_MAX];
    const char *resolved_path = resolve_android_path(path, remapped_path, sizeof(remapped_path));

    int res = access(resolved_path, mode);
    l_debug("access(%s => %s, %i): %i", path, resolved_path, mode, res);
    return res;
}

int lstat_soloader(const char * path, stat64_bionic * buf) {
    char remapped_path[PATH_MAX];
    const char *resolved_path = resolve_android_path(path, remapped_path, sizeof(remapped_path));

    struct stat st;
    int res = lstat(resolved_path, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("lstat(%s => %s): %i", path, resolved_path, res);
    return res;
}

int fclose_soloader(FILE * f) {
#ifdef USE_SCELIBC_IO
    int ret = sceLibcBridge_fclose(f);
#else
    int ret = fclose(f);
#endif

    l_debug("fclose(%p): %i", f, ret);
    return ret;
}

int close_soloader(int fd) {
    int ret = close(fd);
    l_debug("close(%i): %i", fd, ret);
    return ret;
}

DIR* opendir_soloader(char* _pathname) {
    char remapped_path[PATH_MAX];
    const char *resolved_path = resolve_android_path(_pathname, remapped_path, sizeof(remapped_path));
    DIR* ret = opendir(resolved_path);
    l_debug("opendir(\"%s\" => \"%s\"): %p", _pathname, resolved_path, ret);
    return ret;
}

struct dirent64_bionic * readdir_soloader(DIR * dir) {
    static struct dirent64_bionic dirent_tmp;

    struct dirent* ret = readdir(dir);
    l_debug("readdir(%p): %p", dir, ret);

    if (ret) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(ret);
        memcpy(&dirent_tmp, entry_tmp, sizeof(dirent64_bionic));
        free(entry_tmp);
        return &dirent_tmp;
    }

    return NULL;
}

int readdir_r_soloader(DIR * dirp, dirent64_bionic * entry,
                       dirent64_bionic ** result) {
    struct dirent dirent_tmp;
    struct dirent * pdirent_tmp;

    int ret = readdir_r(dirp, &dirent_tmp, &pdirent_tmp);

    if (ret == 0) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(&dirent_tmp);
        memcpy(entry, entry_tmp, sizeof(dirent64_bionic));
        *result = (pdirent_tmp != NULL) ? entry : NULL;
        free(entry_tmp);
    }

    l_debug("readdir_r(%p, %p, %p): %i", dirp, entry, result, ret);
    return ret;
}

int closedir_soloader(DIR * dir) {
    int ret = closedir(dir);
    l_debug("closedir(%p): %i", dir, ret);
    return ret;
}

int fcntl_soloader(int fd, int cmd, ...) {
    l_warn("fcntl(%i, %i, ...): not implemented", fd, cmd);
    return 0;
}

int ioctl_soloader(int fd, int request, ...) {
    l_warn("ioctl(%i, %i, ...): not implemented", fd, request);
    return 0;
}

int fsync_soloader(int fd) {
    int ret = fsync(fd);
    l_debug("fsync(%i): %i", fd, ret);
    return ret;
}
