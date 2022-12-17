#include "loader.h"
#include <pspgu.h>
#include "alloc.h"
#include "panic.h"
#define QOI_IMPLEMENTATION
#define QOI_MALLOC(sz) vramalloc(sz)
#define QOI_FREE(ptr) vfree(ptr)
#include "qoi.h"

unsigned char *readTextFile(const char *path) {
#define readFilePanic(msg, ...) panic("Error while reading file: %s\n" msg ".", path, ##__VA_ARGS__)
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        readFilePanic("Could not open file");
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    void *buffer = malloc(size);
    unsigned long bytes = fread(buffer, 1, size, file);
    fclose(file);
    if (bytes < size) {
        readFilePanic("Read bytes mismatch: read %u bytes out of %u", bytes, size);
    }
    return buffer;
#undef readFilePanic
}

void unloadTextFile(void *buffer) {
    free(buffer);
}

void *loadTexture(const char *path) {
#define loadTexturePanic(msg, ...) panic("Error while loading texture: %s\n" msg ".", path, ##__VA_ARGS__)
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        loadTexturePanic("Could not open file");
    }
    fseek(file, 0, SEEK_END);
    unsigned long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    void *buffer = malloc(size);
    unsigned long bytes = fread(buffer, 1, size, file);
    fclose(file);
    if (bytes < size) {
        loadTexturePanic("Read bytes mismatch: read %u bytes out of %u", bytes, size);
    }
    qoi_desc desc;
    void *texture = qoi_decode(buffer, size, &desc, 4);
    free(buffer);
    if (texture == NULL) {
        loadTexturePanic("Failed to decode QOI");
    }
    return texture;
#undef loadTexturePanic
}

void unloadTexture(void *texturePtr) {
    vfree(vabsptr(texturePtr));
}
