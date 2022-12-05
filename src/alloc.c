#include "alloc.h"
#include <pspkernel.h>
#include <pspgu.h>
#include <vram.h>
#include "panic.h"

#define HEAP_SIZE 0x100000

static unsigned int staticOffset = 0;
static SceUID poolId;

static unsigned int getMemorySize(unsigned int width, unsigned int height, unsigned int psm) {
    switch (psm) {
        case GU_PSM_T4:
            return ((width * height) >> 1);
        case GU_PSM_T8:
            return (width * height);
        case GU_PSM_5650:
        case GU_PSM_5551:
        case GU_PSM_4444:
        case GU_PSM_T16:
            return (2 * width * height);
        case GU_PSM_8888:
        case GU_PSM_T32:
            return (4 * width * height);
        default:
            return 0;
    }
}

void initAllocator(void) {
    poolId = sceKernelCreateVpl("Allocator", PSP_MEMORY_PARTITION_USER, 0, HEAP_SIZE, NULL);
    if (poolId < 0) {
        panic("Failed to create memory pool of size %d", HEAP_SIZE);
    }
}

void endAllocator(void) {
    // We can ignore any error (it will "just" leak memory)
    sceKernelDeleteVpl(poolId);
}

void *allocateMemory(unsigned int size) {
    void *ptr;
    unsigned int timeout = 1000;
    if (sceKernelAllocateVpl(poolId, size, &ptr, &timeout) < 0) {
        panic("Failed to allocate memory of size: %u", size);
    }
    return ptr;
}

void freeMemory(void *ptr) {
    // We can ignore any error (it will "just" leak memory)
    sceKernelFreeVpl(poolId, ptr);
}

void *allocateStaticVramBuffer(unsigned int width, unsigned int height, unsigned int psm) {
    unsigned int memorySize = getMemorySize(width, height, psm);
    void *result = (void *) staticOffset;
    staticOffset += memorySize;
    return result;
}

void *allocateDynamicVramTexture(unsigned int width, unsigned int height, unsigned int psm) {
    unsigned int memorySize = getMemorySize(width, height, psm);
    void *result = vramalloc(memorySize);
    return result;
}

void freeDynamicVramTexture(void *texture) {
    vfree(texture);
}
