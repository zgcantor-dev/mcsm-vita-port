#include <stddef.h>
#include <stdint.h>

/*
 * Override vitaGL shader (de)serialization with safe stubs.
 *
 * Some titles trigger vitaGL's serialize_shader path with invalid records,
 * causing a memcpy from a NULL source pointer. Keeping these symbols in the
 * loader ensures we avoid that crash while porting.
 */
void serialize_shader(const void *record, void *out_data, uint32_t *out_size) {
    (void)record;
    (void)out_data;

    if (out_size != NULL) {
        *out_size = 0;
    }
}

void unserialize_shader(void *record, const void *serialized, uint32_t size) {
    (void)record;
    (void)serialized;
    (void)size;
}
