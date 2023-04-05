#ifndef __LOADER_H__
#define __LOADER_H__

#include <pspkerneltypes.h>

typedef void (*LazyCallbackFn)(void *data, unsigned int width, unsigned int height);

void initLoader(void);
void endLoader(void);

void lazySwapTextureRam(const char *path, void *dest, LazyCallbackFn callback, void *callbackData);
void swapTextureRam(const char *path, void *dest, unsigned int *width, unsigned int *height);

void *readFile(const char *path, unsigned int *outSize);
void unloadFile(void *buffer);

void *loadTextureVram(const char *path, unsigned int *outWidth, unsigned int *outHeight);
void unloadTextureVram(void *texturePtr);

#endif
