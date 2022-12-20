#ifndef __LOADER_H__
#define __LOADER_H__

unsigned char *readTextFile(const char *path);
void unloadTextFile(void *buffer);

void *loadTextureVram(const char *path);
void unloadTextureVram(void *texturePtr);

#endif
