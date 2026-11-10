// Soinnka: RPG de disparos y supervivencia, vista desde arriba, para PSP.
#include <pspkernel.h>
#include <psppower.h>
#include <string.h>
#include "r2d.h"
#include "game.h"

PSP_MODULE_INFO("Soinnka", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

static int exit_cb(int a, int b, void *c)
{
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int cb = sceKernelCreateCallback("pepel_exit", exit_cb, NULL);
    sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}

int main(int argc, char *argv[])
{
    SceUID th = sceKernelCreateThread("pepel_cb", callback_thread, 0x11, 0x1000, 0, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);
    scePowerSetClockFrequency(333, 333, 166);
    r_init();
    // la carpeta del EBOOT: argv[0] sin el nombre del archivo
    static char dir[128];
    const char *d = NULL;
    if (argc > 0 && argv && argv[0]) {
        const char *sl = strrchr(argv[0], '/');
        int n = sl ? (int)(sl - argv[0]) : 0;
        if (n > 0 && n < (int)sizeof(dir)) { memcpy(dir, argv[0], n); dir[n] = 0; d = dir; }
    }
    game_init(d);
    while (1) game_frame();
    return 0;
}
