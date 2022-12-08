#include "alloc.h"
#include <pspkernel.h>
#include <pspgu.h>
#include <vramalloc.h>
#include "panic.h"

static unsigned int staticOffset = 0;

void *allocateStaticVramBuffer(unsigned int width, unsigned int height, unsigned int psm) {
    unsigned int memorySize = vgetMemorySize(width, height, psm);
    void *result = (void *) staticOffset;
    staticOffset += memorySize;
    return result;
}

void *allocateDynamicVramTexture(unsigned int width, unsigned int height, unsigned int psm) {
    unsigned int memorySize = vgetMemorySize(width, height, psm);
    void *result = vramalloc(memorySize);
    return vrelptr(result);
}

void freeDynamicVramTexture(void *texture) {
    vfree(texture);
}
