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
} disp_buffer_update_t;

SceCtrlData _ctrl_data;
SceCtrlLatch _latch_data;
const game_state_t *_current_state = NULL;

static char display_list[DISPLAY_LIST_SIZE] __attribute__((aligned(64)));
static disp_buffer_update_t disp_buffer_updates[8];
static int running, clear_flags;
static int queued_disp_buffer_updates;
static void *draw_buffer, *disp_buffer, *depth_buffer;

static int exit_callback(int arg1, int arg2, void *common) {
    running = 0;
    return 0;
}

static int exit_callback_thread(SceSize args, void *argp) {
    int callbackId = sceKernelCreateCallback("exit_callback", &exit_callback, NULL);
    sceKernelRegisterExitCallback(callbackId);
    sceKernelSleepThreadCB();
    return 0;
}

static int setup_callbacks(void) {
    int exit_callback_thread_id = sceKernelCreateThread("exit_callback_thread_id", &exit_callback_thread, 0x11, 0xFA0, 0, NULL);
    if(exit_callback_thread_id >= 0) {
        sceKernelStartThread(exit_callback_thread_id, 0, NULL);
        return 1;
    }
    return 0;
}

static void frame_start(void) {
    sceGuStart(GU_DIRECT, display_list);
    sceGuClear(clear_flags);

    uint32_t *disp = vabsptr(disp_buffer);
    uint32_t *draw = vabsptr(draw_buffer);
    for (int i = 0; i < queued_disp_buffer_updates; i++) {
        disp_buffer_update_t *u = &disp_buffer_updates[i];
        sceGuCopyImage(GU_PSM_8888, u->x, u->y, u->width, u->height, BUFFER_WIDTH, draw, u->x, u->y, BUFFER_WIDTH, disp);
    }
    queued_disp_buffer_updates = 0;
}

static void frame_end(void) {
    // Start rendering.
    sceGuFinish();
    // Lazy load or wait for the next V-blank interval.
    if (loader_lazy_load()) {
        sceDisplayWaitVblankStartCB();
    }
    // Wait for the frame to finish rendering.
    sceGuSync(GU_SYNC_WHAT_DONE, GU_SYNC_FINISH);
    // Swap the buffers.
    sceGuSwapBuffers();
}

static void gu_start(void) {
    // Reserve VRAM for draw, display and depth buffers.
    draw_buffer = vrelptr(vramalloc(get_vram_memory_size(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888)));
    disp_buffer = vrelptr(vramalloc(get_vram_memory_size(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888)));
    depth_buffer = vrelptr(vramalloc(get_vram_memory_size(BUFFER_WIDTH, PSP_SCREEN_HEIGHT, GU_PSM_4444)));
    // Initialize the graphics utility.
    sceGuInit();
    sceGuStart(GU_DIRECT, display_list);
    // Set up the buffers.
    sceGuDrawBuffer(GU_PSM_8888, draw_buffer, BUFFER_WIDTH);
    sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, disp_buffer, BUFFER_WIDTH);
    sceGuDepthBuffer(depth_buffer, BUFFER_WIDTH);
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

static void gu_end(void) {
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    // Remember to free the buffers.
    vfree(vabsptr(depth_buffer));
    vfree(vabsptr(disp_buffer));
    vfree(vabsptr(draw_buffer));
}

static void start(void) {
    running = 1;
    // Set the default clear flags.
    clear_flags = GU_DEPTH_BUFFER_BIT | GU_COLOR_BUFFER_BIT;
    // Initialize resource loader.
    loader_start();
    // Set up the input mode.
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    // Initialize graphics.
    gu_start();
    // Set up callbacks.
    setup_callbacks();
    // Set the initial game state.
    state_start(&game_state);
}

static void end(void) {
    state_end();
    gu_end();
    loader_end();
    sceKernelExitGame();
}

void set_clear_flags(int flags) {
    clear_flags = flags;
}

void queue_display_buffer_update(short x, short y, short w, short h) {
    disp_buffer_updates[queued_disp_buffer_updates].x = x;
    disp_buffer_updates[queued_disp_buffer_updates].y = y;
    disp_buffer_updates[queued_disp_buffer_updates].width = w;
    disp_buffer_updates[queued_disp_buffer_updates].height = h;
    ++queued_disp_buffer_updates;
}

void set_background_scroll(short offset) {
    if (offset > PSP_SCREEN_MAX_SCROLL) {
        panic("Scroll value too big. Got %d but the maximum is %d", offset, PSP_SCREEN_MAX_SCROLL);
    } else if (offset < 0) {
        panic("Scroll value is negative. Got %d", offset);
    }
    uint32_t bufferOffset = get_vram_memory_size(BUFFER_WIDTH, (uint32_t) offset, GU_PSM_8888);
    sceGuDrawBuffer(GU_PSM_8888, ((char *) draw_buffer) + bufferOffset, BUFFER_WIDTH);
    sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, ((char *) disp_buffer) + bufferOffset, BUFFER_WIDTH);
}

int main(void) {
    start();
    const float delta = 1.0f / sceDisplayGetFramePerSec();
    while (running) {
        // Poll input.
        sceCtrlReadBufferPositive(&_ctrl_data, 1);
        sceCtrlReadLatch(&_latch_data);
        // Update the current state.
        state_update(delta);
        // Render the current state.
        frame_start();
        state_render();
        frame_end();
    }
    end();
    return 0;
}
