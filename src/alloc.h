#ifndef __ALLOC_H__
#define __ALLOC_H__

#include <stdlib.h>
#include <vramalloc.h>

void *allocateStaticVramBuffer(unsigned int width, unsigned int height, unsigned int psm);

#endif
