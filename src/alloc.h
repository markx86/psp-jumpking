#ifndef __ALLOC_H__
#define __ALLOC_H__

#include <stdlib.h>

void *allocateStaticVramBuffer(unsigned int width, unsigned int height, unsigned int psm);
void *allocateDynamicVramTexture(unsigned int width, unsigned int height, unsigned int psm);
void freeDynamicVramTexture(void *texture);

#endif
