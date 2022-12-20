#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <time.h>
#include "alloc.h"
#include "state.h"

PSP_MODULE_INFO("Jump King", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define BUFFER_WIDTH 512
#define BUFFER_HEIGHT SCREEN_HEIGHT

#define VIRTUAL_WIDTH 4096
#define VIRTUAL_HEIGHT 4096

#define DISPLAY_LIST_SIZE 0x20000

const GameState *__currentState;
SceCtrlData __ctrlData;
SceCtrlLatch __latchData;

static int running = 1;
static char displayList[DISPLAY_LIST_SIZE] __attribute__((aligned(64)));
static void *drawBuffer, *dispBuffer, *depthBuffer;

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
    // Clear screen with black
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
    // Reserve VRAM for draw, display and depth buffers
    drawBuffer = vramalloc(vgetMemorySize(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888));
    dispBuffer = vramalloc(vgetMemorySize(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888));
    depthBuffer = vramalloc(vgetMemorySize(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_4444));
    // Initialize the graphics utility
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);
    // Set up the buffers
    sceGuDrawBuffer(GU_PSM_8888, vrelptr(drawBuffer), BUFFER_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, vrelptr(dispBuffer), BUFFER_WIDTH);
    sceGuDepthBuffer(vrelptr(depthBuffer), BUFFER_WIDTH);
    // Set up viewport
    sceGuOffset((VIRTUAL_WIDTH - SCREEN_WIDTH) / 2, (VIRTUAL_HEIGHT - SCREEN_HEIGHT) / 2);
    sceGuViewport(VIRTUAL_WIDTH / 2, VIRTUAL_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    // Set up depth test
    sceGuDepthRange(65535, 0);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    // Enable texture support
    sceGuEnable(GU_TEXTURE_2D);
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
    vfree(depthBuffer);
    vfree(dispBuffer);
    vfree(drawBuffer);
}

static void init(void) {
    // Set up the input mode
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    // Initialize graphics
    initGu();
    // Set up callbacks
    setupCallbacks();
    // Set the initial game state
    __currentState = &IN_GAME;
    __currentState->init();
}

static void cleanup(void) {
    __currentState->cleanup();
    endGu();
    sceKernelExitGame();
}

int main(void) {
    init();
    unsigned long now, prev;
    prev = sceKernelLibcClock();
    while (running) {
        now = sceKernelLibcClock();
        // Poll input
        sceCtrlReadBufferPositive(&__ctrlData, 1);
        sceCtrlReadLatch(&__latchData);
        // Tick current frame
        float delta = ((float)(now - prev)) / 1000000.0f;
        __currentState->update(delta);
        // Prepare next frame
        startFrame();
        // Render current state
        __currentState->render();
        // Display new frame
        endFrame();
        prev = now;
    }
    cleanup();
    return 0;
}
