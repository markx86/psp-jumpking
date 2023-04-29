#include "jki.h"
#include <stdlib.h>
#include <string.h>

#define JKI_MAGIC "JKIMG"

typedef struct {
    char magic[sizeof(JKI_MAGIC)];
    char width[4];
    char height[4];
    char bpp[2];
} __attribute__((packed)) JKITail;

int jkiGetInfo(void *data, unsigned int size, unsigned int *outWidth, unsigned int *outHeight, unsigned int *outBpp) {
    JKITail *header = (JKITail *) &((char *) data)[size - JKI_TAIL_SIZE];
    if (strcmp(header->magic, JKI_MAGIC)) {
        return -1;
    }
    if (outWidth != NULL) {
        unsigned int value = strtol(header->width, NULL, 10);
        if (value == 0) {
            return -1;
        }
        *outWidth = value;
    }
    if (outHeight != NULL) {
        unsigned int value = strtol(header->height, NULL, 10);
        if (value == 0) {
            return -1;
        }
        *outHeight = value;
    }
    if (outBpp != NULL) {
        unsigned int value = strtol(header->bpp, NULL, 10);
        if (value == 0) {
            return -1;
        }
        *outBpp = value;
    }
    return 0;
}
