#ifndef __LOADER_H__
#define __LOADER_H__

#include <pspkerneltypes.h>
#include <stdint.h>

typedef void (*LazyJobFinishCB)(void *data, uint32_t width, uint32_t height);

int lazyLoad(void);
void initLoader(void);
void endLoader(void);

void lazySwapTextureRam(const char *path, void *dest, LazyJobFinishCB callback, void *callbackData);
void swapTextureRam(const char *path, void *dest, uint32_t *width, uint32_t *height);

void *readFile(const char *path, uint32_t *outSize);
void unloadFile(void *buffer);

void *loadTextureVram(const char *path, uint32_t *outWidth, uint32_t *outHeight);
void unloadTextureVram(void *texturePtr);

#endif
