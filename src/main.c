#include <pspuser.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <string.h>
#include "alloc.h"
#include "state.h"

PSP_MODULE_INFO("Jump King", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define BUFFER_WIDTH 512
#define BUFFER_HEIGHT STATE_SCREEN_HEIGHT

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

static void endFrame(void) {
    // Start rendering bitch
    sceGuFinish();
    // Wait for render to finish.
    sceGuSync(GU_SYNC_WHAT_DONE, GU_SYNC_FINISH);
    // Wait for the next V-blank interval.
    if (!sceDisplayIsVblank()) {
        sceDisplayWaitVblankStartCB();
    }
    // Swap the buffers.
    sceGuSwapBuffers();
}

static void initGu(void) {
    // Reserve VRAM for draw, display and depth buffers.
    drawBuffer = vrelptr(vramalloc(getVramMemorySize(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888)));
    dispBuffer = vrelptr(vramalloc(getVramMemorySize(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888)));
    depthBuffer = vrelptr(vramalloc(getVramMemorySize(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_4444)));
    // Initialize the graphics utility.
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);
    // Set up the buffers.
    sceGuDrawBuffer(GU_PSM_8888, drawBuffer, BUFFER_WIDTH);
    sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, dispBuffer, BUFFER_WIDTH);
    sceGuDepthBuffer(depthBuffer, BUFFER_WIDTH);
    // Set up viewport.
    sceGuOffset((VIRTUAL_WIDTH - PSP_SCREEN_WIDTH) / 2, (VIRTUAL_HEIGHT - PSP_SCREEN_HEIGHT) / 2);
    sceGuViewport(VIRTUAL_WIDTH / 2, VIRTUAL_HEIGHT / 2, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
    sceGuScissor(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
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
    vfree(vabsptr(depthBuffer));
    vfree(vabsptr(dispBuffer));
    vfree(vabsptr(drawBuffer));
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

void setBackgroundData(void *data, unsigned int width, unsigned int height) {
    unsigned int size = getVramMemorySize(width, height, GU_PSM_8888);
    memcpy(vabsptr(drawBuffer), data, size);
    memcpy(vabsptr(dispBuffer), data, size);
    memset(vabsptr(depthBuffer), 0, getVramMemorySize(width, height, GU_PSM_4444));
}

void cleanBackgroundAt(void *data, short x, short y, short w, short h, unsigned int stride) {
    unsigned int *pixels = data;
    unsigned int *disp = vabsptr(dispBuffer);
    for (short hh = 0; hh < h; hh++) {
        for (short ww = 0; ww < w; ww++) {
            int offset = (y + hh) * stride + (x + ww);
            disp[offset] = pixels[offset];
        }
    }
}

void setBackgroundScroll(int offset) {
    if (offset > SCREEN_MAX_SCROLL) {
        panic("Scroll scroll too big. Got %d but the maximum is %d", offset, SCREEN_MAX_SCROLL);
    } else if (offset < 0) {
        panic("Scroll scroll is negative. Got %d", offset);
    }
    unsigned int bufferOffset = getVramMemorySize(BUFFER_WIDTH, (unsigned int) offset, GU_PSM_8888);
    sceGuDrawBuffer(GU_PSM_8888, ((char *) drawBuffer) + bufferOffset, BUFFER_WIDTH);
    sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, ((char *) dispBuffer) + bufferOffset, BUFFER_WIDTH);
}

int main(void) {
    init();
    unsigned long thisFrameTime, lastFrameTime;
    lastFrameTime = sceKernelLibcClock();
    long deltaU = 0;
    while (running) {
        thisFrameTime = sceKernelLibcClock();
        // Poll input.
        sceCtrlReadBufferPositive(&__ctrlData, 1);
        sceCtrlReadLatch(&__latchData);
        // Update the current state.
        deltaU += thisFrameTime - lastFrameTime;
        if (deltaU > STATE_UPDATE_DELTA_MS) {
            __currentState->update();
            deltaU -= STATE_UPDATE_DELTA_MS;
        }
        // Render the current state.
        startFrame();
        __currentState->render();
        endFrame();
        lastFrameTime = thisFrameTime;
    }
    cleanup();
    return 0;
}
