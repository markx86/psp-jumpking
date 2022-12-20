#include "alloc.h"
#include <pspuser.h>
#include <pspgu.h>
#include "panic.h"

SceUID createArena(const char *name, unsigned int size) {
    SceUID arena = sceKernelCreateVpl(name, PSP_MEMORY_PARTITION_USER, 0, size, NULL);
    if (arena < 0) {
        panic("Failed to create arena with size %u", size);
    }
    return arena;
}

void destroyArena(SceUID arena) {
    if (sceKernelDeleteVpl(arena) < 0) {
        panic("Failed to destroy arena with ID %d", arena);
    }
}

void *allocateMemory(SceUID arena, unsigned int size) {
    void *data = NULL;
    if (sceKernelTryAllocateVpl(arena, size, &data) < 0) {
        panic("Failed to allocate %u bytes in arena with ID %d", size, arena);
    }
    return data;
}

void *freeMemory(SceUID arena, void *data) {
    if (sceKernelFreeVpl(arena, data) < 0) {
        panic("Failed to free %p from arena with ID %d", data, arena);
    }
}

unsigned int getVramMemorySize(unsigned int width, unsigned int height, unsigned int psm) {
    switch (psm) {
        case GU_PSM_T4:
            return (width * height) >> 1;
        case GU_PSM_T8:
            return width * height;
        case GU_PSM_5650:
        case GU_PSM_5551:
        case GU_PSM_4444:
        case GU_PSM_T16:
            return 2 * width * height;
        case GU_PSM_8888:
        case GU_PSM_T32:
            return 4 * width * height;
        default:
            return 0;
    }
}
