#include "alloc.h"

void *allocateStaticVramBuffer(unsigned int width, unsigned int height, unsigned int psm) {
    unsigned int memorySize = vgetMemorySize(width, height, psm);
    void* buffer = vramalloc(memorySize);
    return vrelptr(buffer);
}
