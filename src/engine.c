#include <pspuser.h>
#include <pspdisplay.h>
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

typedef struct {
    short x, y;
    short width, height;
} DisplayBufferUpdate;

SceCtrlData __ctrlData;
SceCtrlLatch __latchData;

static char displayList[DISPLAY_LIST_SIZE] __attribute__((aligned(64)));
static DisplayBufferUpdate dispBufferUpdates[8];
static int running, clearFlags;
static int queuedDispBufferUpdates;
static void *drawBuffer, *dispBuffer, *depthBuffer;

static int exitCallback(int arg1, int arg2, void *common) {
    running = 0;
    return 0;
}

static int exitCallbackThread(SceSize args, void *argp) {
    int callbackId = sceKernelCreateCallback("ExitCallback", &exitCallback, NULL);
    sceKernelRegisterExitCallback(callbackId);
    sceKernelSleepThreadCB();
    return 0;
}

static int setupCallbacks(void) {
    int exitCallbackThreadId = sceKernelCreateThread("ExitCallbackThread", &exitCallbackThread, 0x11, 0xFA0, 0, NULL);
    if(exitCallbackThreadId >= 0) {
        sceKernelStartThread(exitCallbackThreadId, 0, NULL);
        return 1;
    }
    return 0;
}

static void startFrame(void) {
    sceGuStart(GU_DIRECT, displayList);
    sceGuClear(clearFlags);

    unsigned int *disp = vabsptr(dispBuffer);
    unsigned int *draw = vabsptr(drawBuffer);
    for (int i = 0; i < queuedDispBufferUpdates; i++) {
        DisplayBufferUpdate *u = &dispBufferUpdates[i];
        sceGuCopyImage(GU_PSM_8888, u->x, u->y, u->width, u->height, BUFFER_WIDTH, draw, u->x, u->y, BUFFER_WIDTH, disp);
    }
    queuedDispBufferUpdates = 0;
}

static void endFrame(void) {
    // Start rendering.
    sceGuFinish();
    // Wait for the next V-blank interval.
    if (!sceDisplayIsVblank()) {
        sceDisplayWaitVblankStartCB();
    }
    // Wait for the frame to finish rendering.
    sceGuSync(GU_SYNC_WHAT_DONE, GU_SYNC_FINISH);
    // Swap the buffers.
    sceGuSwapBuffers();
}

static void initGu(void) {
    // Reserve VRAM for draw, display and depth buffers.
    drawBuffer = vrelptr(vramalloc(getVramMemorySize(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888)));
    dispBuffer = vrelptr(vramalloc(getVramMemorySize(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888)));
    depthBuffer = vrelptr(vramalloc(getVramMemorySize(BUFFER_WIDTH, PSP_SCREEN_HEIGHT, GU_PSM_4444)));
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
    running = 1;
    // Set the default clear flags.
    clearFlags = GU_DEPTH_BUFFER_BIT | GU_COLOR_BUFFER_BIT;
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
    cleanupCurrentState();
    endGu();
    endLoader();
    sceKernelExitGame();
}

void setClearFlags(int flags) {
    clearFlags = flags;
}

void queueDisplayBufferUpdate(short x, short y, short w, short h) {
    dispBufferUpdates[queuedDispBufferUpdates].x = x;
    dispBufferUpdates[queuedDispBufferUpdates].y = y;
    dispBufferUpdates[queuedDispBufferUpdates].width = w;
    dispBufferUpdates[queuedDispBufferUpdates].height = h;
    ++queuedDispBufferUpdates;
}

void setBackgroundScroll(short offset) {
    if (offset > PSP_SCREEN_MAX_SCROLL) {
        panic("Scroll value too big. Got %d but the maximum is %d", offset, PSP_SCREEN_MAX_SCROLL);
    } else if (offset < 0) {
        panic("Scroll value is negative. Got %d", offset);
    }
    unsigned int bufferOffset = getVramMemorySize(BUFFER_WIDTH, (unsigned int) offset, GU_PSM_8888);
    sceGuDrawBuffer(GU_PSM_8888, ((char *) drawBuffer) + bufferOffset, BUFFER_WIDTH);
    sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, ((char *) dispBuffer) + bufferOffset, BUFFER_WIDTH);
}

int main(void) {
    init();
    const float delta = 1.0f / sceDisplayGetFramePerSec();
    while (running) {
        // Poll input.
        sceCtrlReadBufferPositive(&__ctrlData, 1);
        sceCtrlReadLatch(&__latchData);
        // Update the current state.
        updateCurrentState(delta);
        // Render the current state.
        startFrame();
        renderCurrentState();
        endFrame();
    }
    cleanup();
    return 0;
}
