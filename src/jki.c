#include "jki.h"
#include <stdlib.h>
#include <string.h>

#define JKI_HEADER_SIZE sizeof("JKI/000/000/0")
#define JKI_MAGIC "JKI"

typedef struct {
    char magic[4];
    char width[4];
    char height[4];
    char bpp[2];
} __attribute__((packed)) JKIHeader;

int jkiGetInfo(void *data, unsigned int size, unsigned int *outWidth, unsigned int *outHeight, unsigned int *outBpp) {
    JKIHeader *header = (JKIHeader *) &((char *) data)[size - JKI_HEADER_SIZE];
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
