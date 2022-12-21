#ifndef __LOADER_H__
#define __LOADER_H__

#include <pspkerneltypes.h>

void initLoader(void);
void endLoader(void);

void *loadTextureVram(const char *path, unsigned int *outWidth, unsigned int *outHeight);
void unloadTextureVram(void *texturePtr);

void *loadTextureRam(const char *path, SceUID *pool, unsigned int *outWidth, unsigned int *outHeight);
void unloadTextureRam(SceUID pool, void *texturePtr);

#endif
