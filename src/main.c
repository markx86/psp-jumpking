#include <pspuser.h>
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

#define DISPLAY_LIST_SIZE 0x40000

SceCtrlData __ctrlData;
SceCtrlLatch __latchData;

static int running, clearFlags;
static char displayList[DISPLAY_LIST_SIZE] __attribute__((aligned(64)));
static void *drawBuffer, *dispBuffer, *depthBuffer;
static const GameState *__currentState;

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
    sceGuClear(clearFlags);
}

static void endFrame(unsigned long start, const unsigned long cycleDeltaT) {
    // Start rendering bitch
    sceGuFinish();
    // Tick the loader while we can
    tickLoader(start, cycleDeltaT);
    // Wait for render to finish.
    sceGuSync(GU_SYNC_WHAT_DONE, GU_SYNC_FINISH);
    // Wait for the next V-blank interval.
    if (!sceDisplayIsVblank()) {
        sceDisplayWaitVblankStart();
    }
    // Swap the buffers.
    sceGuSwapBuffers();
}

static void initGu(void) {
    // Reserve VRAM for draw, display and depth buffers.
    drawBuffer = vramalloc(getVramMemorySize(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888));
    dispBuffer = vramalloc(getVramMemorySize(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888));
    depthBuffer = vramalloc(getVramMemorySize(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_4444));
    // Initialize the graphics utility.
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);
    // Set up the buffers.
    sceGuDrawBuffer(GU_PSM_8888, vrelptr(drawBuffer), BUFFER_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, vrelptr(dispBuffer), BUFFER_WIDTH);
    sceGuDepthBuffer(vrelptr(depthBuffer), BUFFER_WIDTH);
    // Set up viewport.
    sceGuOffset((VIRTUAL_WIDTH - SCREEN_WIDTH) / 2, (VIRTUAL_HEIGHT - SCREEN_HEIGHT) / 2);
    sceGuViewport(VIRTUAL_WIDTH / 2, VIRTUAL_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    // Set up depth test.
    sceGuDepthRange(65535, 0);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    // Enable texture support.
    sceGuEnable(GU_TEXTURE_2D);
    // Set the default clear values.
    sceGuClearColor(0xFF000000);
    sceGuClearDepth(0);
    // Finish initialization.
    sceGuFinish();
    // Wait for render to finish.
    sceGuSync(GU_SYNC_WHAT_DONE, GU_SYNC_FINISH);
    // Wait for the next V-blank interval.
    sceDisplayWaitVblankStart();
    // Start displaying frames.
    sceGuDisplay(GU_TRUE);
}

static void endGu(void) {
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    // Remember to free the buffers.
    vfree(depthBuffer);
    vfree(dispBuffer);
    vfree(drawBuffer);
}

static void init(void) {
    // Set the default clear flags.
    clearFlags = GU_DEPTH_BUFFER_BIT | GU_COLOR_BUFFER_BIT;
    // Set running state to 1.
    running = 1;
    // Initialize resource loader.
    initLoader();
    // Set up the input mode.
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    // Initialize graphics.
    initGu();
    // Set up callbacks.
    setupCallbacks();
    // Set the initial game state.
    switchState(&GAME);
}

static void cleanup(void) {
    __currentState->cleanup();
    endGu();
    endLoader();
    sceKernelExitGame();
}

void switchState(const GameState *new) {
    if (__currentState != NULL) {
        __currentState->cleanup();
    }
    __currentState = new;
    __currentState->init();
}

void setClearFlags(int flags) {
    clearFlags = flags;
}

int main(void) {
    init();
    const unsigned long cycleDeltaT = 1.0f / sceDisplayGetFramePerSec();
    unsigned long start, end;
    end = sceKernelLibcClock();
    while (running) {
        start = sceKernelLibcClock();
        // Poll input.
        sceCtrlReadBufferPositive(&__ctrlData, 1);
        sceCtrlReadLatch(&__latchData);
        // Update the current state.
        float deltaT = ((float)(start - end)) / 1000000.0f;
        __currentState->update(deltaT);
        // Render the current state.
        startFrame();
        __currentState->render();
        endFrame(start, cycleDeltaT);
        end = start;
    }
    cleanup();
    return 0;
}
