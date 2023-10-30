#include "loader.h"
#include "alloc.h"
#include "panic.h"
#include "qoi.h"
#include <pspuser.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <string.h>

#define LOADER_MAX_LAZYJOBS 8
#define LOADER_MAX_PATH_LENGTH 64

typedef enum {
    LAZYJOB_IDLE,
    LAZYJOB_PENDING,
    LAZYJOB_SEEK,
    LAZYJOB_REWIND,
    LAZYJOB_READ,
    LAZYJOB_CLOSE,
    LAZYJOB_DECODING,
    LAZYJOB_DONE,
} LazyJobStatus;

typedef struct {
    LazyJobStatus status;
    char path[LOADER_MAX_PATH_LENGTH];
    void *dest;
    void *readBuffer;
    QoiJobDescriptor desc;
    LazyJobFinishCB finishCallback;
    void* callbackData;
    uint32_t size;
    SceUID fd;
} LazyJob;

static SceUID lazyIoCallbackId;
static int queueEnd, queueStart;
static LazyJob lazyJobs[LOADER_MAX_LAZYJOBS];
static LazyJob *currentJob;

static int lazyIoCallback(int arg1, int arg2, void *argp) {
#define lazyLoaderPanic(msg, ...) panic("Error while lazy loading %s\n" msg, currentJob->path, ##__VA_ARGS__)
    SceInt64 res;
    if (sceIoPollAsync(currentJob->fd, &res) < 0) {
        lazyLoaderPanic("Could not poll fd %d", currentJob->fd);
    }
    switch (currentJob->status) {
        case LAZYJOB_SEEK:
            if (res < 0) {
                lazyLoaderPanic("Could not open file");
            }
            sceIoLseekAsync(currentJob->fd, 0, PSP_SEEK_END);
            currentJob->status = LAZYJOB_REWIND;
            break;
        
        case LAZYJOB_REWIND:
            currentJob->size = (uint32_t) res;
            sceIoLseekAsync(currentJob->fd, 0, PSP_SEEK_SET);
            currentJob->status = LAZYJOB_READ;
            break;
        
        case LAZYJOB_READ:
            if (res != 0) {
                lazyLoaderPanic("Failed to rewind file");
            }
            currentJob->readBuffer = malloc(currentJob->size);
            sceIoReadAsync(currentJob->fd, currentJob->readBuffer, currentJob->size);
            currentJob->status = LAZYJOB_CLOSE;
            break;
        
        case LAZYJOB_CLOSE:
            if (currentJob->size != (uint32_t) res) {
                lazyLoaderPanic("Read bytes mismatch: read %lu bytes out of %u", res, currentJob->size);
            }
            if (qoiInitJob(&currentJob->desc, currentJob->readBuffer, currentJob->dest)) {
                lazyLoaderPanic("Failed to decode QOI");
            }
            sceIoClose(currentJob->fd);
            currentJob->status = LAZYJOB_DECODING;
            break;        

        // This should never be executed.
        default:
            break;
    }
    return 0;
}

int lazyLoad(void) {
    if (currentJob == NULL || currentJob->status != LAZYJOB_DECODING) {
        return !sceDisplayIsVblank();
    }
    if (qoiDecodeJobWhileVBlank(&currentJob->desc)) {
        return 0;
    }
    currentJob->finishCallback(currentJob->callbackData, currentJob->desc.width, currentJob->desc.height);
    free(currentJob->readBuffer);
    ++queueStart;
    if (queueStart == LOADER_MAX_LAZYJOBS) {
        queueStart = 0;
    }
    currentJob->status = LAZYJOB_IDLE;
    currentJob = &lazyJobs[queueStart];
    if (currentJob->status == LAZYJOB_PENDING) {
        currentJob->status = LAZYJOB_SEEK;
        currentJob->fd = sceIoOpenAsync(currentJob->path, PSP_O_RDONLY, 0444);
        if (currentJob->fd < 0) {
            lazyLoaderPanic("Could not open file");
        }
        sceIoSetAsyncCallback(currentJob->fd, lazyIoCallbackId, currentJob);
    }
    return 0;
#undef lazyLoaderPanic
}

void initLoader(void) {
    lazyIoCallbackId = sceKernelCreateCallback("LazyIoCallback", &lazyIoCallback, NULL);
    if (lazyIoCallbackId < 0) {
        panic("Failed to create lazy loader callback.");
    }
    memset(lazyJobs, 0, sizeof(lazyJobs));
    queueEnd = 0;
    queueStart = 0;
    currentJob = NULL;
}

void endLoader(void) {
    sceKernelDeleteCallback(lazyIoCallbackId);
}

void *readFile(const char *path, uint32_t *outSize) {
#define readFilePanic(msg, ...) panic("Error while reading file: %s\n" msg, path, ##__VA_ARGS__)
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0444);
    if (fd < 0) {
        readFilePanic("Could not open file");
    }
    SceOff size = sceIoLseek(fd, 0, PSP_SEEK_END);
    sceIoLseek(fd, 0, PSP_SEEK_SET);
    void *buffer = malloc(size);
    unsigned long bytes = sceIoRead(fd, buffer, size);
    sceIoClose(fd);
    if (bytes < size) {
        readFilePanic("Read bytes mismatch: read %u bytes out of %u", bytes, size);
    }
    if (outSize != NULL) {
        *outSize = size;
    }
    return buffer;
#undef readFilePanic
}

void lazySwapTextureRam(const char *path, void *dest, LazyJobFinishCB callback, void* callbackData) {
#define swapTexturePanic(msg, ...) panic("Error while swapping texture %s\n" msg, path, ##__VA_ARGS__)
    LazyJob *job = &lazyJobs[queueEnd];
    while (job->status != LAZYJOB_IDLE) {
        sceKernelDelayThreadCB(1000);
    }
    strcpy(job->path, path);
    job->dest = dest;
    job->finishCallback = callback;
    job->callbackData = callbackData;
    if (queueEnd == queueStart) {
        currentJob = &lazyJobs[queueStart];
        job->status = LAZYJOB_SEEK;
        job->fd = sceIoOpenAsync(job->path, PSP_O_RDONLY, 0444);
        if (job->fd < 0) {
            swapTexturePanic("Could not open file");
        }
        sceIoSetAsyncCallback(job->fd, lazyIoCallbackId, NULL);
    } else {
        job->status = LAZYJOB_PENDING;
    }
    if (++queueEnd == LOADER_MAX_LAZYJOBS) {
        queueEnd = 0;
    }
}

void swapTextureRam(const char *path, void *dest, uint32_t *width, uint32_t *height) {
    uint32_t size;
    void *buffer = readFile(path, &size);
    if (qoiDecode(buffer, dest, width, height)) {
        swapTexturePanic("Could not decode QOI");
    }
    unloadFile(buffer);
#undef swapTexturePanic
}

void *loadTextureVram(const char *path, uint32_t *width, uint32_t *height) {
#define loadTexturePanic(msg, ...) panic("Error while loading texture: %s\n" msg, path, ##__VA_ARGS__)
    uint32_t size;
    void *buffer = readFile(path, &size);
    uint32_t w, h;
    qoiDecode(buffer, NULL, &w, &h);
    void *texture = vramalloc(w * h * 4);
    if (texture == NULL) {
        loadTexturePanic("Failed to allocate VRAM");
    }
    if (qoiDecode(buffer, texture, NULL, NULL)) {
        loadTexturePanic("Failed to decode QOI");
    }
    unloadFile(buffer);
    if (width != NULL) {
        *width = w;
    }
    if (height != NULL) {
        *height = h;
    }
    return texture;
}

void unloadFile(void *buffer) {
    free(buffer);
}

void unloadTextureVram(void *texturePtr) {
    vfree(texturePtr);
}
