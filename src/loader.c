#include "loader.h"
#include <pspkernel.h>
#include <png.h>
#include "alloc.h"
#include "panic.h"

char *readFile(const char *path) {
    SceUID file = sceIoOpen(path, PSP_O_RDONLY, 777);
    if (file < 0) {
        panic("Could not open file for reading: %s", path);
    }
    unsigned int size = sceIoLseek(file, 0, SEEK_END);
    sceIoLseek(file, 0, SEEK_SET);
    void *buffer = malloc(size);
    int bytes = sceIoRead(file, buffer, size);
    sceIoClose(file);
    if (bytes < size) {
        panic("Read bytes mismatch: read %d bytes out of %u", bytes, size);
    }
    return buffer;
}

void unloadFile(void *buffer) {
    free(buffer);
}

void *loadTexture(const char *path) {
    char *png = readFile(path);
    unloadFile(png);
}
