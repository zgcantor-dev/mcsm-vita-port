#include "reimpl/asset_manager.h"
#include "utils/logger.h"

#include <pthread.h>
#include <malloc.h>
#include <cstring>
#include <cstdio>
#include <libc_bridge/libc_bridge.h>
#include <string>
#include <fcntl.h>
#include <vector>
#include <algorithm>
#include <dirent.h>

typedef struct assetManager {
    int dummy = 0; // TODO: mb we will need to store something here in future
    pthread_mutex_t mLock;
} assetManager;

typedef struct aAsset {
    char * filename;
    FILE* f;
    size_t bytesRead;
    size_t fileSize;
    bool opened = false;
} asset;

typedef struct aAssetDir {
    std::vector<std::string> entries;
    size_t index;
    std::string resolvedPath;
} assetDir;

static AAssetManager * g_AAssetManager = nullptr;

AAssetManager * AAssetManager_create() {
    if (g_AAssetManager) return g_AAssetManager;

    assetManager am;

    pthread_mutex_init(&am.mLock, nullptr);

    g_AAssetManager = (AAssetManager *) malloc(sizeof(assetManager));
    memcpy(g_AAssetManager, &am, sizeof(assetManager));

    return g_AAssetManager;
}


AAssetManager *AAssetManager_fromJava(void *env, void *assetManager) {
    const bool fallback_env = (env == nullptr);
    const bool fallback_obj = (assetManager == nullptr);

    if (!env) {
        l_error("AAssetManager_fromJava called with NULL JNIEnv; using Vita filesystem asset manager fallback");
    }

    if (!assetManager) {
        l_warn("AAssetManager_fromJava called with NULL/unknown Java AssetManager object (e.g. missing SDLActivity.mAssetMgr); using Vita filesystem fallback");
    }

    AAssetManager *mgr = AAssetManager_create();
    l_info("AAssetManager_fromJava(env=%p, assetManager=%p) -> %p (Vita optional AssetManager shim; fallback=%s%s)",
           env,
           assetManager,
           mgr,
           (fallback_env || fallback_obj) ? "yes:" : "no",
           (fallback_env || fallback_obj) ? (fallback_env && fallback_obj ? "null-env+null-assetmgr" : (fallback_env ? "null-env" : "missing-assetmgr")) : "none");
    return mgr;
}

static FILE *open_asset_path(const char *path) {
#ifdef USE_SCELIBC_IO
    return sceLibcBridge_fopen(path, "rb");
#else
    return fopen(path, "rb");
#endif
}

AAsset* AAssetManager_open(AAssetManager* mgr, const char* filename, int mode) {
    if (!filename || filename[0] == '\0') {
        l_warn("AAssetManager_open(%p, %p, %i): invalid filename", mgr, filename, mode);
        return nullptr;
    }

    std::string requested(filename);
    std::string data_root = std::string(DATA_PATH);
    std::string assets_path = data_root + std::string("assets/") + requested;
    std::string direct_path = data_root + requested;

    FILE *f = open_asset_path(assets_path.c_str());
    const char *resolved = assets_path.c_str();

    if (!f) {
        f = open_asset_path(direct_path.c_str());
        resolved = direct_path.c_str();
    }

    if (!f) {
        l_warn("AAssetManager_open(%p, \"%s\", %i): not found in %sassets/ or data root; APK assets are optional on Vita", mgr, filename, mode, DATA_PATH);
        return nullptr;
    }

    auto *a = new aAsset;
    a->filename = (char *)malloc(strlen(resolved) + 1);
    strcpy(a->filename, resolved);
    a->f = f;
    a->bytesRead = 0;

#ifdef USE_SCELIBC_IO
    sceLibcBridge_fseek(a->f, 0, SEEK_END);
    a->fileSize = sceLibcBridge_ftell(a->f);
    sceLibcBridge_fseek(a->f, 0, SEEK_SET);
#else
    fseek(a->f, 0, SEEK_END);
    a->fileSize = ftell(a->f);
    fseek(a->f, 0, SEEK_SET);
#endif
    a->opened = true;

    l_info("AAssetManager_open(%p, \"%s\", %i) -> %p [resolved=%s]", mgr, filename, mode, a, resolved);
    return (AAsset *) a;
}

void AAsset_close(AAsset* asset) {
    l_debug("AAsset_close<%p>(%p)", __builtin_return_address(0), asset);

    if (asset) {
        auto * a = (aAsset *) asset;
        free(a->filename);
        if (a->opened) {
#ifdef USE_SCELIBC_IO
            sceLibcBridge_fclose(a->f);
#else
            fclose(a->f);
#endif
        }
        delete a;
    }
}

int AAsset_read(AAsset* asset, void* buf, size_t count) {
    l_debug("AAsset_read<%p>(%p, %p, %i)", __builtin_return_address(0), asset, buf, count);

    if (!asset) {
        return -1;
    }

    auto * a = (aAsset *) asset;

    if (!a->opened) {
        return -1;
    }

#ifdef USE_SCELIBC_IO
    size_t ret = sceLibcBridge_fread(buf, 1, count, a->f);
#else
    size_t ret = fread(buf, 1, count, a->f);
#endif

    if (ret > 0) {
        a->bytesRead += ret;
        return (int) ret;
    } else {
#ifdef USE_SCELIBC_IO
        if (sceLibcBridge_feof(a->f)) {
#else
        if (feof(a->f)) {
#endif
            return 0;
        } else {
            return -1;
        }
    }
}

off_t AAsset_seek(AAsset* asset, off_t offset, int whence) {
    l_debug("AAsset_seek(%p, %d, %i)", asset, offset, whence);

    if (!asset) {
        return (off_t) -1;
    }

    auto * a = (aAsset *) asset;

    if (!a->opened) {
        return -1;
    }

#ifdef USE_SCELIBC_IO
    auto ret = (off_t) sceLibcBridge_fseek(a->f, offset, whence);
#else
    auto ret = (off_t) fseek(a->f, offset, whence);
#endif

    return ret;
}

off_t AAsset_getRemainingLength(AAsset* asset) {
    l_debug("AAsset_getRemainingLength");
    if (!asset) {
        return (off_t) -1;
    }

    auto * a = (aAsset *) asset;

    if (!a->opened) {
        return -1;
    }

    return (off_t)(a->fileSize - a->bytesRead);
}

off_t AAsset_getLength(AAsset* asset) {
    l_debug("AAsset_getLength");
    if (!asset) {
        return (off_t) -1;
    }

    auto * a = (aAsset *) asset;

    return (off_t)a->fileSize;
}

AAssetDir* AAssetManager_openDir(AAssetManager* mgr, const char* dirName) {
    if (!dirName) {
        l_warn("AAssetManager_openDir(%p, %p): invalid dirName", mgr, dirName);
        return nullptr;
    }

    std::string requested(dirName);
    std::string dataRoot(DATA_PATH);
    std::string baseAssets = dataRoot + "assets";
    std::string baseDirect = dataRoot;

    std::string assetsPath = requested.empty() ? baseAssets : (baseAssets + "/" + requested);
    std::string directPath = requested.empty() ? baseDirect : (baseDirect + requested);

    DIR *dir = opendir(assetsPath.c_str());
    std::string resolved = assetsPath;
    if (!dir) {
        dir = opendir(directPath.c_str());
        resolved = directPath;
    }

    if (!dir) {
        l_warn("AAssetManager_openDir(%p, \"%s\"): not found in %sassets/ or data root", mgr, dirName, DATA_PATH);
        return nullptr;
    }

    auto *result = new assetDir();
    result->index = 0;
    result->resolvedPath = resolved;

    while (dirent *entry = readdir(dir)) {
        if (!entry->d_name || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        result->entries.emplace_back(entry->d_name);
    }

    closedir(dir);

    std::sort(result->entries.begin(), result->entries.end());

    l_info("AAssetManager_openDir(%p, \"%s\") -> %p [resolved=%s, entries=%zu]", mgr, dirName, result, result->resolvedPath.c_str(), result->entries.size());
    return (AAssetDir *)result;
}

const char* AAssetDir_getNextFileName(AAssetDir* assetDir) {
    if (!assetDir) {
        l_warn("AAssetDir_getNextFileName(%p): invalid assetDir", assetDir);
        return nullptr;
    }

    auto *dir = (struct aAssetDir *) assetDir;
    if (dir->index >= dir->entries.size()) {
        return nullptr;
    }

    const char *name = dir->entries[dir->index].c_str();
    dir->index++;
    return name;
}

void AAssetDir_close(AAssetDir* assetDir) {
    l_debug("AAssetDir_close(%p)", assetDir);
    if (assetDir) {
        delete (struct aAssetDir *) assetDir;
    }
}

int AAsset_openFileDescriptor(AAsset* asset, off_t* outStart, off_t* outLength) {
    if (!asset) {
        l_warn("AAsset_openFileDescriptor(%p, %p, %p): asset is null", asset, outStart, outLength);
        return -1;
    }
    auto * a = (aAsset *) asset;
    if (outStart) *outStart = 0;
    if (outLength) *outLength = a->fileSize;
    if (a->opened) {
        if (a->opened) {
#ifdef USE_SCELIBC_IO
            sceLibcBridge_fclose(a->f);
#else
            fclose(a->f);
#endif
        }
        a->opened = false;
    }
    int ret = open(a->filename, O_RDONLY);
    l_debug("AAsset_openFileDescriptor(%p/\"%s\", %p, %p): ret %i", asset, a->filename, outStart, outLength, ret);
    return ret;
}
