#include "loader.h"
#include "alloc.h"
#include "panic.h"
#include "qoi.h"
#include <pspuser.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <string.h>

#define LOADER_ARENA_SIZE 0x20000
#define LOADER_MAX_LAZYJOBS 5
#define LOADER_MAX_PATH_LENGTH 64

typedef enum {
    LAZYJOB_IDLE,
    LAZYJOB_BUSY
} LoaderLazyJobStatus;

typedef struct {
    LoaderLazyJobStatus status;
    char path[LOADER_MAX_PATH_LENGTH];
    void *dest;
} LoaderLazyJob;

static SceUID loaderArena;
static int queueEnd, queueStart;
static LoaderLazyJob lazyJobs[LOADER_MAX_LAZYJOBS];

static const char *filenameFromPath(const char *path) {
    unsigned int lastSlashIndex = 0;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            lastSlashIndex = i;
        }
    }
    return path + lastSlashIndex + 1;
}

void initLoader(void) {
    loaderArena = createArena("LoaderVPL", LOADER_ARENA_SIZE);
    memset(lazyJobs, 0, sizeof(lazyJobs));
    queueEnd = 0;
    queueStart = 0;
}

// (TODO) Implement lazy loading (async loading)
void tickLoader(unsigned long start, const unsigned long cycleDeltaT) {
    while (sceKernelLibcClock() - start < cycleDeltaT) {
        LoaderLazyJob *job = &lazyJobs[queueStart];
        if (job->status == LAZYJOB_BUSY) {
            unsigned int size;
            void *buffer = readFile(job->path, &size);
            QoiDescriptor desc;
            if (qoiDecode(buffer, size, &desc, job->dest)) {
                panic("Error while lazy loading texture: %s\nFailed to decode QOI", job->path);
            }
            unloadFile(buffer);
            ++queueStart;
            if (queueStart == LOADER_MAX_LAZYJOBS) {
                queueStart = 0;
            }
            job->status = LAZYJOB_IDLE;
        } else {
            break;
        }
    }
}

void endLoader(void) {
    destroyArena(loaderArena);
}

void *readFile(const char *path, unsigned int *outSize) {
#define readFilePanic(msg, ...) panic("Error while reading file: %s\n" msg, path, ##__VA_ARGS__)
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0444);
    if (fd < 0) {
        readFilePanic("Could not open file");
    }
    SceOff size = sceIoLseek(fd, 0, PSP_SEEK_END);
    sceIoLseek(fd, 0, PSP_SEEK_SET);
    void *buffer = allocateMemory(loaderArena, size);
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

void lazySwapTextureRam(const char *path, void *dest) {
    LoaderLazyJob *job = &lazyJobs[queueEnd++];
    if (queueEnd == LOADER_MAX_LAZYJOBS) {
        queueEnd = 0;
    }
    while (job->status == LAZYJOB_BUSY) {}
    job->status = LAZYJOB_BUSY;
    memcpy(job->path, path, 64);
    job->dest = dest;
}

void swapTextureRam(const char *path, void *dest) {
    unsigned int size;
    void *buffer = readFile(path, &size);
    QoiDescriptor desc;
    if (qoiDecode(buffer, size, &desc, dest)) {
        panic("Error while swapping texture: %s\nFailed to decode QOI", path);
    }
    unloadFile(buffer);
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

void *loadTextureRam(const char *path, SceUID *pool, unsigned int *outWidth, unsigned int *outHeight) {
    unsigned int size;
    SceUID targetPool = *pool;
    void *buffer = readFile(path, &size);
    QoiDescriptor desc;
    if (targetPool < 0) {
        qoiDecode(buffer, size, &desc, NULL);
        unsigned int allocSize = desc.width * desc.height * desc.channels;
        targetPool = createPool(filenameFromPath(path), allocSize);
    }
    void *texture = allocatePool(targetPool);
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
    *pool = targetPool;
    return texture;
#undef loadTexturePanic
}

void unloadFile(void *buffer) {
    freeMemory(loaderArena, buffer);
}

void unloadTextureVram(void *texturePtr) {
    vfree(texturePtr);
}

void unloadTextureRam(SceUID pool, void *texturePtr) {
    freePool(pool, texturePtr);
}
