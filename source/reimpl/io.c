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
#include <sys/statvfs.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdarg.h>
#include <psp2/kernel/threadmgr.h>

#define TELLTALE_PKG_NAME "com.telltalegames.minecraft100"
#define TELLTALE_OBB_VERSION "40137"
#define TELLTALE_OBB_DIR "/Android/obb/" TELLTALE_PKG_NAME "/"
#define VITA_OBB_DIR "ux0:" TELLTALE_OBB_DIR

#define TELLTALE_MAIN_OBB_VERSIONED "main." TELLTALE_OBB_VERSION "." TELLTALE_PKG_NAME ".obb"
#define TELLTALE_PATCH_OBB_VERSIONED "patch." TELLTALE_OBB_VERSION "." TELLTALE_PKG_NAME ".obb"

#define OBB_SCAN_MAX_DEPTH 5
#define OBB_SCAN_ENTRY_BUDGET 4096

#define ANDROID_OBB_REL_DIR "Android/obb/" TELLTALE_PKG_NAME "/"
#define ANDROID_DATA_FILES_REL_DIR "Android/data/" TELLTALE_PKG_NAME "/files/"

static const char *g_android_obb_prefixes[] = {
    "/storage/emulated/0/" ANDROID_OBB_REL_DIR,
    "/sdcard/" ANDROID_OBB_REL_DIR,
    "/mnt/sdcard/" ANDROID_OBB_REL_DIR,
    "/data/media/0/" ANDROID_OBB_REL_DIR,
    NULL,
};

static const char *g_android_data_prefixes[] = {
    "/storage/emulated/0/" ANDROID_DATA_FILES_REL_DIR,
    "/sdcard/" ANDROID_DATA_FILES_REL_DIR,
    "/mnt/sdcard/" ANDROID_DATA_FILES_REL_DIR,
    "/data/data/" TELLTALE_PKG_NAME "/files/",
    "/data/user/0/" TELLTALE_PKG_NAME "/files/",
    NULL,
};

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


static volatile int g_main_obb_open_logged = 0;
static volatile int g_main_obb_open_done_logged = 0;
static volatile int g_patch_obb_open_logged = 0;
static volatile int g_patch_obb_open_done_logged = 0;
static volatile int g_main_obb_parse_done_logged = 0;

typedef struct obb_path_cache_entry {
    int initialized;
    int found;
    char path[PATH_MAX];
} obb_path_cache_entry;

static obb_path_cache_entry g_main_obb_cache;
static obb_path_cache_entry g_patch_obb_cache;

static void log_obb_stage_before_open(const char *kind, const char *filename) {
    if (!kind || !filename)
        return;

    if (strcmp(kind, "main") == 0) {
        if (!g_main_obb_open_logged) {
            g_main_obb_open_logged = 1;
            l_info("main: before main obb open (%s)", filename);
        }
    } else if (strcmp(kind, "patch") == 0) {
        if (!g_patch_obb_open_logged) {
            g_patch_obb_open_logged = 1;
            l_info("main: before patch obb open (%s)", filename);
        }
    }
}

static void log_obb_stage_after_open(const char *kind, const char *resolved_path, int ok) {
    if (!kind)
        return;

    if (strcmp(kind, "main") == 0) {
        if (!g_main_obb_open_done_logged) {
            g_main_obb_open_done_logged = 1;
            l_info("main: after main obb open (%s, ok=%d)", resolved_path ? resolved_path : "(null)", ok);
            l_info("main: before main obb parse");
        }
    } else if (strcmp(kind, "patch") == 0) {
        if (!g_patch_obb_open_done_logged) {
            g_patch_obb_open_done_logged = 1;
            l_info("main: after patch obb open (%s, ok=%d)", resolved_path ? resolved_path : "(null)", ok);
        }
    }
}


static const char *detect_telltale_obb_basename(const char *src) {
    if (!src) {
        return NULL;
    }

    const char *basename = NULL;

    const char *filename = strrchr(src, '/');
    filename = filename ? filename + 1 : src;

    if (strcmp(src, OBB_PATH) == 0 || strstr(src, TELLTALE_MAIN_OBB_VERSIONED) != NULL || strstr(src, "main.com.telltalegames.minecraft100.obb") != NULL || strstr(src, "main.obb") != NULL ||
        (strncmp(filename, "main.", 5) == 0 && strstr(filename, ".com.telltalegames.minecraft100.obb") != NULL)) {
        basename = "main";
    } else if (strcmp(src, PATCH_OBB_PATH) == 0 || strstr(src, TELLTALE_PATCH_OBB_VERSIONED) != NULL || strstr(src, "patch.com.telltalegames.minecraft100.obb") != NULL || strstr(src, "patch.obb") != NULL ||
               (strncmp(filename, "patch.", 6) == 0 && strstr(filename, ".com.telltalegames.minecraft100.obb") != NULL)) {
        basename = "patch";
    } else {
        const char *obb_pos = strstr(src, TELLTALE_OBB_DIR);
        if (!obb_pos)
            return NULL;

        const char *candidate = obb_pos + strlen(TELLTALE_OBB_DIR);
        if (strncmp(candidate, "main", 4) == 0) {
            basename = "main";
        } else if (strncmp(candidate, "patch", 5) == 0) {
            basename = "patch";
        }
    }

    return basename;
}

static int path_exists_local(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0;
}

static int try_build_obb_candidate(char *dst, size_t dst_size, const char *dir, const char *basename) {
    if (!dst || dst_size == 0 || !dir || !basename)
        return 0;

    int written = snprintf(dst, dst_size, "%s%s", dir, basename);
    if (written <= 0 || (size_t)written >= dst_size)
        return 0;

    return path_exists_local(dst);
}

static int try_build_obb_candidates(char *dst, size_t dst_size, const char *dir, const char *obb_kind) {
    if (!dst || dst_size == 0 || !dir || !obb_kind)
        return 0;

    if (strcmp(obb_kind, "main") == 0) {
        if (try_build_obb_candidate(dst, dst_size, dir, TELLTALE_MAIN_OBB_VERSIONED))
            return 1;
        if (try_build_obb_candidate(dst, dst_size, dir, "main.com.telltalegames.minecraft100.obb"))
            return 1;
        if (try_build_obb_candidate(dst, dst_size, dir, "main.obb"))
            return 1;
    } else if (strcmp(obb_kind, "patch") == 0) {
        if (try_build_obb_candidate(dst, dst_size, dir, TELLTALE_PATCH_OBB_VERSIONED))
            return 1;
        if (try_build_obb_candidate(dst, dst_size, dir, "patch.com.telltalegames.minecraft100.obb"))
            return 1;
        if (try_build_obb_candidate(dst, dst_size, dir, "patch.obb"))
            return 1;
    }

    return 0;
}

static int scan_dir_for_file(const char *dir_path, const char *basename, int depth, int *entry_budget, char *dst, size_t dst_size) {
    unsigned int entry_idx = 0;
    if (!dir_path || !basename || !dst || dst_size == 0 || depth > OBB_SCAN_MAX_DEPTH)
        return 0;

    if (!entry_budget || *entry_budget <= 0)
        return 0;

    DIR *dir = opendir(dir_path);
    if (!dir)
        return 0;

    int found = 0;
    struct dirent *entry;
    while (!found && (entry = readdir(dir)) != NULL) {
        if (*entry_budget <= 0) {
            l_warn("OBB scan budget exhausted while searching for %s under %s", basename, dir_path);
            break;
        }
        (*entry_budget)--;
        entry_idx++;
        if ((entry_idx % 100) == 0)
            l_info("main obb entry %u / ? (dir=%s)", entry_idx, dir_path);
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char candidate[PATH_MAX];
        int written = snprintf(candidate, sizeof(candidate), "%s%s", dir_path, entry->d_name);
        if (written <= 0 || (size_t)written >= sizeof(candidate))
            continue;

        struct stat st;
        if (stat(candidate, &st) != 0)
            continue;

        if (S_ISREG(st.st_mode)) {
            if (strcmp(entry->d_name, basename) == 0) {
                written = snprintf(dst, dst_size, "%s", candidate);
                found = written > 0 && (size_t)written < dst_size;
            }
            continue;
        }

        if (!S_ISDIR(st.st_mode) || depth >= OBB_SCAN_MAX_DEPTH)
            continue;

        size_t candidate_len = strlen(candidate);
        if (candidate_len + 1 >= sizeof(candidate))
            continue;

        candidate[candidate_len] = '/';
        candidate[candidate_len + 1] = '\0';

        found = scan_dir_for_file(candidate, basename, depth + 1, entry_budget, dst, dst_size);
    }

    closedir(dir);
    return found;
}

static int resolve_telltale_obb_path(const char *obb_kind, char *dst, size_t dst_size) {
    if (!obb_kind || !dst || dst_size == 0)
        return 0;

    obb_path_cache_entry *cache_entry = NULL;
    if (strcmp(obb_kind, "main") == 0)
        cache_entry = &g_main_obb_cache;
    else if (strcmp(obb_kind, "patch") == 0)
        cache_entry = &g_patch_obb_cache;

    if (cache_entry && cache_entry->initialized) {
        if (!cache_entry->found)
            return 0;

        int copied = snprintf(dst, dst_size, "%s", cache_entry->path);
        return copied > 0 && (size_t)copied < dst_size;
    }

    // Primary Android-like location.
    if (try_build_obb_candidates(dst, dst_size, VITA_OBB_DIR, obb_kind))
        return 1;

    // Common Vita layouts used by ports/users.
    if (try_build_obb_candidates(dst, dst_size, DATA_PATH, obb_kind))
        return 1;

    if (try_build_obb_candidates(dst, dst_size, DATA_PATH "obb/", obb_kind))
        return 1;

    if (try_build_obb_candidates(dst, dst_size, DATA_PATH "Android/obb/" TELLTALE_PKG_NAME "/", obb_kind))
        return 1;

    // Last resort: scan under data root for these known OBB names.
    int scan_budget = OBB_SCAN_ENTRY_BUDGET;
    if (strcmp(obb_kind, "main") == 0) {
        if (scan_dir_for_file(DATA_PATH, TELLTALE_MAIN_OBB_VERSIONED, 0, &scan_budget, dst, dst_size))
            return 1;
        if (scan_dir_for_file(DATA_PATH, "main.com.telltalegames.minecraft100.obb", 0, &scan_budget, dst, dst_size))
            return 1;
        if (scan_dir_for_file(DATA_PATH, "main.obb", 0, &scan_budget, dst, dst_size))
            return 1;
    } else if (strcmp(obb_kind, "patch") == 0) {
        if (scan_dir_for_file(DATA_PATH, TELLTALE_PATCH_OBB_VERSIONED, 0, &scan_budget, dst, dst_size))
            return 1;
        if (scan_dir_for_file(DATA_PATH, "patch.com.telltalegames.minecraft100.obb", 0, &scan_budget, dst, dst_size))
            return 1;
        if (scan_dir_for_file(DATA_PATH, "patch.obb", 0, &scan_budget, dst, dst_size))
            return 1;
    }

    return 0;
}

static int default_telltale_obb_path(const char *obb_kind, char *dst, size_t dst_size) {
    if (!obb_kind || !dst || dst_size == 0)
        return 0;

    const char *versioned = NULL;
    if (strcmp(obb_kind, "main") == 0) {
        versioned = TELLTALE_MAIN_OBB_VERSIONED;
    } else if (strcmp(obb_kind, "patch") == 0) {
        versioned = TELLTALE_PATCH_OBB_VERSIONED;
    } else {
        return 0;
    }

    int written = snprintf(dst, dst_size, "%s%s", VITA_OBB_DIR, versioned);
    return written > 0 && (size_t)written < dst_size;
}

static int remap_telltale_obb_path(const char *src, char *dst, size_t dst_size) {
    if (!dst || dst_size == 0) {
        return 0;
    }

    const char *obb_kind = detect_telltale_obb_basename(src);
    if (!obb_kind)
        return 0;

    if (resolve_telltale_obb_path(obb_kind, dst, dst_size)) {
        obb_path_cache_entry *cache_entry = strcmp(obb_kind, "main") == 0 ? &g_main_obb_cache : &g_patch_obb_cache;
        cache_entry->initialized = 1;
        cache_entry->found = 1;
        snprintf(cache_entry->path, sizeof(cache_entry->path), "%s", dst);
        return 1;
    }

    obb_path_cache_entry *cache_entry = strcmp(obb_kind, "main") == 0 ? &g_main_obb_cache : &g_patch_obb_cache;
    cache_entry->initialized = 1;
    cache_entry->found = 0;

    return default_telltale_obb_path(obb_kind, dst, dst_size);
}

static int remap_android_storage_path(const char *src, char *dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0)
        return 0;

    for (int i = 0; g_android_data_prefixes[i] != NULL; i++) {
        const char *prefix = g_android_data_prefixes[i];
        size_t prefix_len = strlen(prefix);
        if (strncmp(src, prefix, prefix_len) == 0) {
            const char *relative = src + prefix_len;
            int written = snprintf(dst, dst_size, "%s%s", DATA_PATH, relative);
            if (written > 0 && (size_t)written < dst_size) {
                l_info("[path-remap] Android data path %s => %s", src, dst);
                return 1;
            }
            l_warn("[path-remap] Android data path remap overflow for %s", src);
            return 0;
        }
    }

    for (int i = 0; g_android_obb_prefixes[i] != NULL; i++) {
        const char *prefix = g_android_obb_prefixes[i];
        size_t prefix_len = strlen(prefix);
        if (strncmp(src, prefix, prefix_len) == 0) {
            const char *basename = src + prefix_len;
            int written = snprintf(dst, dst_size, "%s%s", VITA_OBB_DIR, basename);
            if (written > 0 && (size_t)written < dst_size) {
                l_info("[path-remap] Android OBB path %s => %s", src, dst);
                return 1;
            }
            l_warn("[path-remap] Android OBB path remap overflow for %s", src);
            return 0;
        }
    }

    return 0;
}

static int remap_telltale_obb_data_path(const char *src, char *dst, size_t dst_size) {
    if (!dst || dst_size == 0) {
        return 0;
    }

    const char *obb_kind = detect_telltale_obb_basename(src);
    if (!obb_kind)
        return 0;

    if (strcmp(obb_kind, "main") == 0) {
        int written = snprintf(dst, dst_size, "%s%s", DATA_PATH, TELLTALE_MAIN_OBB_VERSIONED);
        if (written > 0 && (size_t)written < dst_size)
            return 1;
        written = snprintf(dst, dst_size, "%s%s", DATA_PATH, "main.obb");
        return written > 0 && (size_t)written < dst_size;
    }

    int written = snprintf(dst, dst_size, "%s%s", DATA_PATH, TELLTALE_PATCH_OBB_VERSIONED);
    if (written > 0 && (size_t)written < dst_size)
        return 1;
    written = snprintf(dst, dst_size, "%s%s", DATA_PATH, "patch.obb");
    return written > 0 && (size_t)written < dst_size;
}

static const char *find_telltale_obb_payload_relative_path(const char *src) {
    if (!src)
        return NULL;

    static const char *obb_markers[] = {
        TELLTALE_MAIN_OBB_VERSIONED,
        TELLTALE_PATCH_OBB_VERSIONED,
        "main.com.telltalegames.minecraft100.obb",
        "patch.com.telltalegames.minecraft100.obb",
        "main.obb",
        "patch.obb",
        NULL,
    };

    for (int i = 0; obb_markers[i] != NULL; i++) {
        const char *marker = strstr(src, obb_markers[i]);
        if (!marker)
            continue;

        marker += strlen(obb_markers[i]);
        if (*marker == '/' && marker[1] != '\0')
            return marker + 1;
    }

    return NULL;
}

static int remap_telltale_extracted_data_path(const char *src, char *dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0)
        return 0;

    const char *relative = find_telltale_obb_payload_relative_path(src);
    if (!relative)
        return 0;

    int written = snprintf(dst, dst_size, "%s%s", DATA_PATH, relative);

    if (written <= 0 || (size_t)written >= dst_size)
        return 0;

    l_info("[path-remap] extracted OBB payload path %s => %s", src, dst);
    if (!g_main_obb_parse_done_logged) {
        g_main_obb_parse_done_logged = 1;
        l_info("main: after main obb parse");
    }
    return 1;
}

FILE * fopen_soloader(const char * filename, const char * mode) {
    const char *resolved_filename = filename;
    char remapped_path[PATH_MAX];
    char fallback_path[PATH_MAX];
    const char *obb_kind = detect_telltale_obb_basename(filename);
    if (obb_kind)
        log_obb_stage_before_open(obb_kind, filename);

    if (strcmp(filename, "/proc/cpuinfo") == 0) {
        resolved_filename = "app0:/cpuinfo";
    } else if (strcmp(filename, "/proc/meminfo") == 0) {
        resolved_filename = "app0:/meminfo";
    } else if (remap_telltale_extracted_data_path(filename, remapped_path, sizeof(remapped_path))) {
        resolved_filename = remapped_path;
    } else if (remap_android_storage_path(filename, remapped_path, sizeof(remapped_path))) {
        resolved_filename = remapped_path;
    } else if (remap_telltale_obb_path(filename, remapped_path, sizeof(remapped_path))) {
#ifdef USE_SCELIBC_IO
        FILE *ret = sceLibcBridge_fopen(remapped_path, mode);
#else
        FILE *ret = fopen(remapped_path, mode);
#endif
        if (!ret && remap_telltale_obb_data_path(filename, fallback_path, sizeof(fallback_path))) {
#ifdef USE_SCELIBC_IO
            ret = sceLibcBridge_fopen(fallback_path, mode);
#else
            ret = fopen(fallback_path, mode);
#endif
            resolved_filename = fallback_path;
        } else {
            resolved_filename = remapped_path;
        }

        if (ret)
            l_debug("fopen(%s => %s, %s): %p", filename, resolved_filename, mode, ret);
        else
            l_warn("fopen(%s => %s, %s): %p", filename, resolved_filename, mode, ret);

        if (obb_kind)
            log_obb_stage_after_open(obb_kind, resolved_filename, ret != NULL);

        return ret;
    }

#ifdef USE_SCELIBC_IO
    FILE* ret = sceLibcBridge_fopen(resolved_filename, mode);
#else
    FILE* ret = fopen(resolved_filename, mode);
#endif

    if (ret)
        l_debug("fopen(%s => %s, %s): %p", filename, resolved_filename, mode, ret);
    else
        l_warn("fopen(%s => %s, %s): %p", filename, resolved_filename, mode, ret);

    if (obb_kind)
        log_obb_stage_after_open(obb_kind, resolved_filename, ret != NULL);

    return ret;
}

int open_soloader(const char * path, int oflag, ...) {
    const char *resolved_path = path;
    char remapped_path[PATH_MAX];
    char fallback_path[PATH_MAX];
    const char *obb_kind = detect_telltale_obb_basename(path);
    if (obb_kind)
        log_obb_stage_before_open(obb_kind, path);

    mode_t mode = 0666;
    if (((oflag & BIONIC_O_CREAT) == BIONIC_O_CREAT) ||
        ((oflag & BIONIC_O_TMPFILE) == BIONIC_O_TMPFILE)) {
        va_list args;
        va_start(args, oflag);
        mode = (mode_t)(va_arg(args, int));
        va_end(args);
    }

    if (strcmp(path, "/proc/cpuinfo") == 0) {
        resolved_path = "app0:/cpuinfo";
    } else if (strcmp(path, "/proc/meminfo") == 0) {
        resolved_path = "app0:/meminfo";
    } else if (strcmp(path, "/dev/urandom") == 0) {
        resolved_path = "app0:/urandom";
    } else if (remap_telltale_extracted_data_path(path, remapped_path, sizeof(remapped_path))) {
        resolved_path = remapped_path;
    } else if (remap_android_storage_path(path, remapped_path, sizeof(remapped_path))) {
        resolved_path = remapped_path;
    } else if (remap_telltale_obb_path(path, remapped_path, sizeof(remapped_path))) {
        int open_flags = oflags_bionic_to_newlib(oflag);
        int fd = open(remapped_path, open_flags, mode);
        if (fd < 0 && remap_telltale_obb_data_path(path, fallback_path, sizeof(fallback_path))) {
            fd = open(fallback_path, open_flags, mode);
            resolved_path = fallback_path;
        } else {
            resolved_path = remapped_path;
        }

        if (fd >= 0)
            l_debug("open(%s => %s, %x): %i", path, resolved_path, open_flags, fd);
        else
            l_warn("open(%s => %s, %x): %i", path, resolved_path, open_flags, fd);
        if (obb_kind)
            log_obb_stage_after_open(obb_kind, resolved_path, fd >= 0);
        return fd;
    }

    oflag = oflags_bionic_to_newlib(oflag);
    int ret = open(resolved_path, oflag, mode);
    if (ret >= 0)
        l_debug("open(%s => %s, %x): %i", path, resolved_path, oflag, ret);
    else
        l_warn("open(%s => %s, %x): %i", path, resolved_path, oflag, ret);
    if (obb_kind)
        log_obb_stage_after_open(obb_kind, resolved_path, ret >= 0);
    return ret;
}

int rename_soloader(const char *oldpath, const char *newpath) {
    if (!oldpath || !newpath) {
        l_warn("rename(%p, %p): invalid arguments", oldpath, newpath);
        return -1;
    }

    const char *resolved_oldpath = oldpath;
    const char *resolved_newpath = newpath;
    char remapped_oldpath[PATH_MAX];
    char remapped_newpath[PATH_MAX];

    if (remap_android_storage_path(oldpath, remapped_oldpath, sizeof(remapped_oldpath))) {
        resolved_oldpath = remapped_oldpath;
    } else if (remap_telltale_extracted_data_path(oldpath, remapped_oldpath, sizeof(remapped_oldpath))) {
        resolved_oldpath = remapped_oldpath;
    }

    if (remap_android_storage_path(newpath, remapped_newpath, sizeof(remapped_newpath))) {
        resolved_newpath = remapped_newpath;
    } else if (remap_telltale_extracted_data_path(newpath, remapped_newpath, sizeof(remapped_newpath))) {
        resolved_newpath = remapped_newpath;
    }

    int ret = rename(resolved_oldpath, resolved_newpath);
    if (ret == 0)
        l_debug("rename(%s => %s, %s => %s): %i", oldpath, resolved_oldpath, newpath, resolved_newpath, ret);
    else
        l_warn("rename(%s => %s, %s => %s): %i", oldpath, resolved_oldpath, newpath, resolved_newpath, ret);

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


int statfs_soloader(const char *path, statfs_bionic *buf) {
    if (!path || !buf) {
        l_warn("statfs(%p, %p): invalid arguments", path, buf);
        return -1;
    }

    const char *resolved_path = path;
    char remapped_path[PATH_MAX];
    char fallback_path[PATH_MAX];

    if (remap_android_storage_path(path, remapped_path, sizeof(remapped_path))) {
        resolved_path = remapped_path;
    } else if (remap_telltale_extracted_data_path(path, remapped_path, sizeof(remapped_path))) {
        resolved_path = remapped_path;
    } else if (remap_telltale_obb_path(path, remapped_path, sizeof(remapped_path))) {
        struct statvfs stvfs_obb;
        int res_obb = statvfs(remapped_path, &stvfs_obb);
        if (res_obb != 0 && remap_telltale_obb_data_path(path, fallback_path, sizeof(fallback_path))) {
            resolved_path = fallback_path;
        } else {
            resolved_path = remapped_path;
        }
    }

    struct statvfs stvfs;
    int res = statvfs(resolved_path, &stvfs);
    if (res != 0) {
        l_warn("statfs(%s => %s): %i", path, resolved_path, res);
        return res;
    }

    memset(buf, 0, sizeof(*buf));
    buf->f_type = 0;
    buf->f_bsize = (uint32_t)stvfs.f_bsize;
    buf->f_blocks = (uint64_t)stvfs.f_blocks;
    buf->f_bfree = (uint64_t)stvfs.f_bfree;
    buf->f_bavail = (uint64_t)stvfs.f_bavail;
    buf->f_files = (uint64_t)stvfs.f_files;
    buf->f_ffree = (uint64_t)stvfs.f_ffree;
    buf->f_fsid_0 = 0;
    buf->f_fsid_1 = 0;
    buf->f_namelen = (uint32_t)stvfs.f_namemax;
    buf->f_frsize = (uint32_t)stvfs.f_frsize;
    buf->f_flags = (uint32_t)stvfs.f_flag;

    l_debug("statfs(%s => %s): 0", path, resolved_path);
    return 0;
}

int stat_soloader(const char * path, stat64_bionic * buf) {
    const char *resolved_path = path;
    char remapped_path[PATH_MAX];
    char fallback_path[PATH_MAX];

    if (strcmp(path, "/system/lib/libOpenSLES.so") == 0) {
        l_debug("stat(%s): returning 0 in case this is a check for OpenSLES support", path);
        return 0;
    }

    if (remap_android_storage_path(path, remapped_path, sizeof(remapped_path))) {
        resolved_path = remapped_path;
    } else if (remap_telltale_extracted_data_path(path, remapped_path, sizeof(remapped_path))) {
        resolved_path = remapped_path;
    } else if (remap_telltale_obb_path(path, remapped_path, sizeof(remapped_path))) {
        struct stat st;
        int res = stat(remapped_path, &st);

        if (res != 0 && remap_telltale_obb_data_path(path, fallback_path, sizeof(fallback_path))) {
            res = stat(fallback_path, &st);
            resolved_path = fallback_path;
        } else {
            resolved_path = remapped_path;
        }

        if (res == 0)
            stat_newlib_to_bionic(&st, buf);

        l_debug("stat(%s => %s): %i", path, resolved_path, res);
        return res;
    }

    struct stat st;
    int res = stat(resolved_path, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("stat(%s => %s): %i", path, resolved_path, res);
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
    DIR* ret = opendir(_pathname);
    l_debug("opendir(\"%s\"): %p", _pathname, ret);
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

ssize_t pread_soloader(int fd, void *buf, size_t count, off_t offset) {
    off_t current = lseek(fd, 0, SEEK_CUR);
    if (current < 0)
        return -1;

    if (lseek(fd, offset, SEEK_SET) < 0)
        return -1;

    ssize_t result = read(fd, buf, count);
    off_t restore = lseek(fd, current, SEEK_SET);
    if (restore < 0 && result >= 0)
        return -1;

    return result;
}

ssize_t pwrite_soloader(int fd, const void *buf, size_t count, off_t offset) {
    off_t current = lseek(fd, 0, SEEK_CUR);
    if (current < 0)
        return -1;

    if (lseek(fd, offset, SEEK_SET) < 0)
        return -1;

    ssize_t result = write(fd, buf, count);
    off_t restore = lseek(fd, current, SEEK_SET);
    if (restore < 0 && result >= 0)
        return -1;

    return result;
}
