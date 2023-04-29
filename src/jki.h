#ifndef __JKI_H__
#define __JKI_H__

#define JKI_TAIL_SIZE sizeof("JKIMG/000/000/0")

int jkiGetInfo(void *data, unsigned int size, unsigned int *outWidth, unsigned int *outHeight, unsigned int *outBpp);

#endif
