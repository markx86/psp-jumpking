#ifndef __VRAM_H__
#define __VRAM_H__

void *getStaticVramBuffer(unsigned int width, unsigned int height, unsigned int psm);
void *getStaticVramTexture(unsigned int width, unsigned int height, unsigned int psm);

#endif
