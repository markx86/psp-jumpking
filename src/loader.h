#ifndef __LOADER_H__
#define __LOADER_H__

void initLoader(void);
void endLoader(void);

void *loadTextureVram(const char *path, unsigned int *outWidth, unsigned int *outHeight);
void unloadTextureVram(void *texturePtr);

#endif
