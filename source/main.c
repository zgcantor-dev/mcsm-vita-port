#include "utils/init.h"

#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>

#include "reimpl/controls.h"

int _newlib_heap_size_user = 256 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod;

int main() {
    soloader_init_all();
    sceKernelDelayThread(1000 * 1000);
    sceKernelExitDeleteThread(0);
}

void controls_handler_key(int32_t keycode, ControlsAction action) {
    (void)keycode;
    (void)action;
}

void controls_handler_touch(int32_t id, float x, float y, ControlsAction action) {
    (void)id;
    (void)x;
    (void)y;
    (void)action;
}

void controls_handler_analog(ControlsStickId which, float x, float y, ControlsAction action) {
    (void)which;
    (void)x;
    (void)y;
    (void)action;
}
