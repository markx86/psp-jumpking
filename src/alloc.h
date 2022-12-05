#ifndef __ALLOC_H__
#define __ALLOC_H__

void initAllocator(void);
void endAllocator(void);

void *allocateMemory(unsigned int size);
void freeMemory(void *ptr);

void *allocateStaticVramBuffer(unsigned int width, unsigned int height, unsigned int psm);
void *allocateDynamicVramTexture(unsigned int width, unsigned int height, unsigned int psm);
void freeDynamicVramTexture(void *texture);

#endif
