#ifndef __LOADER_H__
#define __LOADER_H__

unsigned char *readTextFile(const char *path);
void unloadTextFile(void *buffer);

void *loadTexture(const char *path);
void unloadTexture(void *texturePtr);

#endif
