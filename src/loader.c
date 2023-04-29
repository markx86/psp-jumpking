#include "loader.h"
#include "alloc.h"
#include "panic.h"
#include "jki.h"
#include "qoi.h"
#include <pspuser.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <string.h>

#define LOADER_MAX_LAZYJOBS 5
#define LOADER_MAX_PATH_LENGTH 64

typedef enum {
    LAZYJOB_IDLE,
    LAZYJOB_PENDING,
    LAZYJOB_SEEK,
    LAZYJOB_REWIND,
    LAZYJOB_READ,
    LAZYJOB_CLOSE,
    LAZYJOB_DONE,
} LazyJobStatus;

typedef struct {
    LazyJobStatus status;
    char path[LOADER_MAX_PATH_LENGTH];
    void *dest;
    LazyJobFinishCB finishCallback;
    void* callbackData;
    unsigned int size;
    SceUID fd;
} LazyJob;

static SceUID lazyIoCallbackId;
static int queueEnd, queueStart;
static LazyJob lazyJobs[LOADER_MAX_LAZYJOBS];

static int lazyIoCallback(int arg1, int jobPtr, void *argp) {
#define lazyLoaderPanic(msg, ...) panic("Error while lazy loading %s\n" msg, job->path, ##__VA_ARGS__)
    SceInt64 res;
    LazyJob *job = (LazyJob *) jobPtr;
    unsigned int width, height;
    if (sceIoPollAsync(job->fd, &res) < 0) {
        lazyLoaderPanic("Could not poll fd %d", job->fd);
    }
    switch (job->status) {
        case LAZYJOB_SEEK:
            sceIoLseekAsync(job->fd, 0, PSP_SEEK_END);
            job->status = LAZYJOB_REWIND;
            break;
        
        case LAZYJOB_REWIND:
            job->size = (unsigned int) res;
            sceIoLseekAsync(job->fd, 0, PSP_SEEK_SET);
            job->status = LAZYJOB_READ;
            break;
        
        case LAZYJOB_READ:
            sceIoReadAsync(job->fd, job->dest, job->size);
            job->status = LAZYJOB_CLOSE;
            break;
        
        case LAZYJOB_CLOSE:
            if (job->size != (unsigned int) res) {
                lazyLoaderPanic("Read bytes mismatch: read %lu bytes out of %u", res, job->fd);
            }
            sceIoCloseAsync(job->fd);
            if (jkiGetInfo(job->dest, job->size, &width, &height, NULL)) {
                lazyLoaderPanic("Invalid JKI header");
            }
            if (job->finishCallback != NULL) {
                job->finishCallback(job->callbackData, width, height);
            }
            job->status = LAZYJOB_DONE;
            break;
        
        case LAZYJOB_DONE:
            ++queueStart;
            if (queueStart == LOADER_MAX_LAZYJOBS) {
                queueStart = 0;
            }
            job->status = LAZYJOB_IDLE;
            job = &lazyJobs[queueStart];
            if (job->status == LAZYJOB_PENDING) {
                job->status = LAZYJOB_SEEK;
                job->fd = sceIoOpenAsync(job->path, PSP_O_RDONLY, 0444);
                if (job->fd < 0) {
                    lazyLoaderPanic("Could not open file");
                }
                sceIoSetAsyncCallback(job->fd, lazyIoCallbackId, job);
            }
            break;
        
        // This should never be executed.
        default:
            break;
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
}

void endLoader(void) {
    sceKernelDeleteCallback(lazyIoCallbackId);
}

void *readFile(const char *path, unsigned int *outSize) {
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
        job->status = LAZYJOB_SEEK;
        job->fd = sceIoOpenAsync(job->path, PSP_O_RDONLY, 0444);
        if (job->fd < 0) {
            swapTexturePanic("Could not open file");
        }
        sceIoSetAsyncCallback(job->fd, lazyIoCallbackId, job);
    } else {
        job->status = LAZYJOB_PENDING;
    }
    if (++queueEnd == LOADER_MAX_LAZYJOBS) {
        queueEnd = 0;
    }
}

void swapTextureRam(const char *path, void *dest, unsigned int* width, unsigned int* height) {
    unsigned int size;
    void *buffer = readFile(path, &size);
    memcpy(dest, buffer, size);
    if (jkiGetInfo(dest, size, width, height, NULL)) {
        swapTexturePanic("Invalid JKI header");
    }
    unloadFile(buffer);
#undef swapTexturePanic
}

void *loadTextureVram(const char *path, unsigned int *outWidth, unsigned int *outHeight) {
#define loadTexturePanic(msg, ...) panic("Error while loading texture: %s\n" msg, path, ##__VA_ARGS__)
    unsigned int size;
    void *buffer = readFile(path, &size);
    QoiDescriptor desc;
    qoiDecode(buffer, size, &desc, NULL);
    void *texture = vramalloc(desc.width * desc.height * desc.channels);
    if (texture == NULL) {
        loadTexturePanic("Failed to allocate VRAM");
    }
    if (qoiDecode(buffer, size, &desc, texture)) {
        loadTexturePanic("Failed to decode QOI");
    }
    unloadFile(buffer);
    if (outWidth != NULL) {
        *outWidth = desc.width;
    }
    if (outHeight != NULL) {
        *outHeight = desc.height;
    }
    return texture;
}

void unloadFile(void *buffer) {
    free(buffer);
}

void unloadTextureVram(void *texturePtr) {
    vfree(texturePtr);
}
