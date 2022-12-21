#ifndef __ALLOC_H__
#define __ALLOC_H__

#include <pspkerneltypes.h>
#include <vram.h>

SceUID createArena(const char *name, unsigned int size);
void destroyArena(SceUID arena);

SceUID createPool(const char *name, unsigned int size);
void destroyPool(SceUID pool);

void *allocateMemory(SceUID arena, unsigned int size);
void freeMemory(SceUID arena, void *data);

void *allocatePool(SceUID pool);
void freePool(SceUID pool, void *data);

unsigned int getVramMemorySize(unsigned int width, unsigned int height, unsigned int psm);

#endif
