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
void *builtin_resolve_symbol(const char *name);

#ifdef __cplusplus
}
#endif

#endif
