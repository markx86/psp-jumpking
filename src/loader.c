#include "loader.h"
#include <pspuser.h>
#include <pspgu.h>
#include "alloc.h"
#include "panic.h"

#define LOADER_ARENA_SIZE 0x10000

static SceUID loaderArena, targetPool;
static enum {
    QOI_ALLOC_VRAM,
    QOI_ALLOC_RAM,
    QOI_ALLOC_TEST,
} qoiAllocType;

static void *qoiAlloc(unsigned int size) {
    switch (qoiAllocType) {
        case QOI_ALLOC_VRAM:
            return vramalloc(size);
        case QOI_ALLOC_RAM:
            return allocatePool(targetPool);
        case QOI_ALLOC_TEST:
            return NULL;
    }
}

static void qoiFree(void *ptr) {
    switch (qoiAllocType) {
        case QOI_ALLOC_VRAM:
            vfree(ptr);
            break;
        case QOI_ALLOC_RAM:
            freePool(targetPool, ptr);
            break;
        case QOI_ALLOC_TEST:
            break;
    }
}

static const char *filenameFromPath(const char *path) {
    unsigned int lastSlashIndex = 0;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            lastSlashIndex = i;
        }
    }
    return path + lastSlashIndex + 1;
}

#define QOI_NO_STDIO
#define QOI_IMPLEMENTATION
#define QOI_MALLOC(sz) qoiAlloc(sz)
#define QOI_FREE(ptr) qoiFree(ptr)
#include "qoi.h"

void initLoader(void) {
    loaderArena = createArena("VPL-loader", LOADER_ARENA_SIZE);
    qoiAllocType = QOI_ALLOC_RAM;
}

void endLoader(void) {
    destroyArena(loaderArena);
}

void *readFileData(const char *path, unsigned int *outSize) {
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

void *loadTextureVram(const char *path, unsigned int *outWidth, unsigned int *outHeight) {
#define loadTexturePanic(msg, ...) panic("Error while loading texture: %s\n" msg, path, ##__VA_ARGS__)
    unsigned int size;
    void *buffer = readFileData(path, &size);
    qoiAllocType = QOI_ALLOC_VRAM;
    qoi_desc desc;
    void *texture = qoi_decode(buffer, size, &desc, 0);
    freeMemory(loaderArena, buffer);
    if (texture == NULL) {
        loadTexturePanic("Failed to decode QOI");
    }
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
    void *buffer = readFileData(path, &size);
    qoiAllocType = QOI_ALLOC_RAM;
    qoi_desc desc;
    if (*pool < 0) {
        qoi_decode(buffer, size, &desc, 0);
        unsigned int allocSize = desc.width * desc.height * desc.channels;
        *pool = createPool(filenameFromPath(path), allocSize);
    }
    targetPool = *pool;
    void *texture = qoi_decode(buffer, size, &desc, 0);
    freeMemory(loaderArena, buffer);
    if (texture == NULL) {
        loadTexturePanic("Failed to decode QOI");
    }
    if (outWidth != NULL) {
        *outWidth = desc.width;
    }
    if (outHeight != NULL) {
        *outHeight = desc.height;
    }
    return texture;
#undef loadTexturePanic
}

void unloadTextureVram(void *texturePtr) {
    vfree(texturePtr);
}

void unloadTextureRam(SceUID pool, void *texturePtr) {
    freePool(pool, texturePtr);
}
