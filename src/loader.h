#ifndef __LOADER_H__
#define __LOADER_H__

#include <pspkerneltypes.h>
#include <stdint.h>

typedef void (*loader_lazyjob_finish_callback_t)(void *data, uint32_t width, uint32_t height);

int loader_lazy_load(void);
void loader_start(void);
void loader_end(void);

void loader_lazy_swap_texture_ram(const char *path, void *dest, loader_lazyjob_finish_callback_t callback, void *callback_data);
void loader_swap_texture_ram(const char *path, void *dest, uint32_t *width, uint32_t *height);

void *loader_read_file(const char *path, uint32_t *out_size);
void loader_unload_file(void *buffer);

void *loader_load_texture_vram(const char *path, uint32_t *out_width, uint32_t *out_height);
void loader_unload_texture_vram(void *texture_ptr);

#endif
