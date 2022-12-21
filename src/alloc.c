#include "alloc.h"
#include <pspuser.h>
#include <pspgu.h>
#include "panic.h"

SceUID createArena(const char *name, unsigned int size) {
    SceUID arena = sceKernelCreateVpl(name, PSP_MEMORY_PARTITION_USER, 0, size, NULL);
    if (arena < 0) {
        panic("Failed to create arena '%s' with size %u", name, size);
    }
    return arena;
}

SceUID createPool(const char *name, unsigned int size) {
    SceUID pool = sceKernelCreateFpl(name, PSP_MEMORY_PARTITION_USER, 0, size, 1, NULL);
    if (pool < 0) {
        panic("Failed to create pool '%s' with size %u", name, size);
    }
    return pool;
}

void destroyArena(SceUID arena) {
    if (sceKernelDeleteVpl(arena) < 0) {
        panic("Failed to destroy arena with ID %d", arena);
    }
}

void destroyPool(SceUID pool) {
    if (sceKernelDeleteFpl(pool) < 0) {
        panic("Failed to destroy pool with ID %d", pool);
    }
}

void *allocateMemory(SceUID arena, unsigned int size) {
    void *data = NULL;
    if (sceKernelTryAllocateVpl(arena, size, &data) < 0) {
        panic("Failed to allocate %u bytes in arena with ID %d", size, arena);
    }
    return data;
}

void freeMemory(SceUID arena, void *data) {
    if (sceKernelFreeVpl(arena, data) < 0) {
        panic("Failed to free %p from arena with ID %d", data, arena);
    }
}

void *allocatePool(SceUID pool) {
    void *data = NULL;
    if (sceKernelTryAllocateFpl(pool, &data) < 0) {
        panic("Failed to allocate pool with ID %d", pool);
    }
    return data;
}

void freePool(SceUID pool, void *data) {
    if (sceKernelFreeFpl(pool, data) < 0) {
        panic("Failed to free %p from pool with ID %d", data, pool);
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