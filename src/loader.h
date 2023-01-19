#ifndef __LOADER_H__
#define __LOADER_H__

#include <pspkerneltypes.h>

void initLoader(void);
void endLoader(void);

void lazySwapTextureRam(const char *path, void *dest);
void swapTextureRam(const char *path, void *dest);

void *readFile(const char *path, unsigned int *outSize);
void unloadFile(void *buffer);

void *loadTextureVram(const char *path, unsigned int *outWidth, unsigned int *outHeight);
void unloadTextureVram(void *texturePtr);

#endif
