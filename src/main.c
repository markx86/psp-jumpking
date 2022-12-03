#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>

#include "vram.h"
#include "state.h"

PSP_MODULE_INFO("jkport", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU | THREAD_ATTR_USER);

#define BUFFER_WIDTH 512
#define BUFFER_HEIGHT SCREEN_HEIGHT

#define VIRTUAL_WIDTH 4096
#define VIRTUAL_HEIGHT 4096

#define DISPLAY_LIST_SIZE 0x20000

const GameState *__currentState;
SceCtrlData __ctrlData;


static int running;
static char displayList[DISPLAY_LIST_SIZE] __attribute__((aligned(64)));

static int exitCallback(int arg1, int arg2, void *common) {
    running = 0;
    return 0;
}

static int callbackThread(SceSize args, void *argp) {
    int callbackId = sceKernelCreateCallback("ExitCallback", &exitCallback, NULL);
    sceKernelRegisterExitCallback(callbackId);
    sceKernelSleepThreadCB();
    return 0;
}

static int setupCallbacks(void) {
    int threadId = sceKernelCreateThread("CallbackThread", &callbackThread, 0x11, 0xFA0, 0, NULL);
    if(threadId >= 0) {
        sceKernelStartThread(threadId, 0, NULL);
        return 1;
    }
    return 0;
}

static void startFrame(void) {
    sceGuStart(GU_DIRECT, displayList);
    // Clear screen with white
    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

static void endFrame(void) {
    sceGuFinish();
    // Wait for render to finish
    sceGuSync(GU_SYNC_WHAT_DONE, GU_SYNC_FINISH);
    // Wait for the next V-blank interval
    sceDisplayWaitVblankStart();
    // Swap the buffers
    sceGuSwapBuffers();
}

static void initGu(void) {
    void *drawBuffer, *dispBuffer, *depthBuffer;
    // Initialize the graphics utility
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);
    // Reserve VRAM for draw, display and depth buffers
    drawBuffer = getStaticVramBuffer(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888);
    dispBuffer = getStaticVramBuffer(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888);
    depthBuffer = getStaticVramBuffer(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_4444);
    // Set up the buffers
    sceGuDrawBuffer(GU_PSM_8888, drawBuffer, BUFFER_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, dispBuffer, BUFFER_WIDTH);
    sceGuDepthBuffer(depthBuffer, BUFFER_WIDTH);
    // Set up viewport
    sceGuOffset((VIRTUAL_WIDTH - SCREEN_WIDTH) / 2, (VIRTUAL_HEIGHT - SCREEN_HEIGHT) / 2);
    sceGuViewport(VIRTUAL_WIDTH / 2, VIRTUAL_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    // Set up depth test
    sceGuDepthRange(65535, 0);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    // Finish initialization
    sceGuFinish();
    // Wait for render to finish
    sceGuSync(GU_SYNC_WHAT_DONE, GU_SYNC_FINISH);
    // Wait for the next V-blank interval
    sceDisplayWaitVblankStart();
    // Start displaying frames
    sceGuDisplay(GU_TRUE);
}

static void endGu(void) {
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

extern const GameState IN_GAME;

static int init(void) {
#ifdef DEBUG
    // Initialize debug screen
    // only if this is a debug build
    pspDebugScreenInit();
    pspDebugScreenClear();
#endif
    // Set up the input mode
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    // Initialize graphics
    initGu();
    // Set the initial game state
    __currentState = &IN_GAME;
    __currentState->init();
    // Set up callbacks
    return setupCallbacks();
}

static int cleanup(void) {
    endGu();
    sceKernelExitGame();
    return 0;
}

int main(void) {
    long now, prev;
    running = init();
    prev = sceKernelLibcClock();
    while (running) {
        now = sceKernelLibcClock();
        // Poll input
        sceCtrlReadBufferPositive(&__ctrlData, 1);
        // Tick current frame
        __currentState->update(now - prev);
        // Prepare next frame
        startFrame();
        // Render current state
        __currentState->render();
        // Display new frame
        endFrame();
        prev = now;
    }
    return cleanup();
}
