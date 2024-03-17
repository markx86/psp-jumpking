#include "loader.h"
#include "alloc.h"
#include "panic.h"
#include "qoi.h"
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspuser.h>
#include <string.h>

#define LAZYJOBS_MAX 3
#define LAZYJOB_PATH_LEN 64

typedef enum {
  LAZYJOB_IDLE,
  LAZYJOB_PENDING,
  LAZYJOB_SEEK,
  LAZYJOB_REWIND,
  LAZYJOB_READ,
  LAZYJOB_CLOSE,
  LAZYJOB_DECODING,
  LAZYJOB_DONE,
} lazyjob_status_t;

typedef struct {
  lazyjob_status_t status;
  char path[LAZYJOB_PATH_LEN];
  SceUID fd;
  uint32_t file_size;
  void* decode_dst;
  void* read_dst;
  qoi_job_descriptor_t desc;
  loader_lazyjob_finish_callback_t callback;
  void* callback_data;
} lazyjob_t;

static SceUID asyncio_callback_id;
static int queue_end, queue_start;
static lazyjob_t lazy_jobs[LAZYJOBS_MAX];
static lazyjob_t* current_job;

static int
asyncio_callback(int arg1, int arg2, void* argp) {
#define loader_panic(msg, ...) \
  panic("Error while lazy loading %s\n" msg, current_job->path, ##__VA_ARGS__)
  SceInt64 res;
  if (sceIoPollAsync(current_job->fd, &res) < 0)
    loader_panic("Could not poll fd %d", current_job->fd);
  switch (current_job->status) {
    case LAZYJOB_SEEK:
      if (res < 0)
        loader_panic("Could not open file");
      sceIoLseekAsync(current_job->fd, 0, PSP_SEEK_END);
      current_job->status = LAZYJOB_REWIND;
      break;

    case LAZYJOB_REWIND:
      current_job->file_size = (uint32_t)res;
      sceIoLseekAsync(current_job->fd, 0, PSP_SEEK_SET);
      current_job->status = LAZYJOB_READ;
      break;

    case LAZYJOB_READ:
      if (res != 0)
        loader_panic("Failed to rewind file");
      current_job->read_dst = alloc_mem(current_job->file_size);
      sceIoReadAsync(
          current_job->fd,
          current_job->read_dst,
          current_job->file_size);
      current_job->status = LAZYJOB_CLOSE;
      break;

    case LAZYJOB_CLOSE:
      if (current_job->file_size != (uint32_t)res)
        loader_panic(
            "Read bytes mismatch: read %lu bytes out of %u",
            res,
            current_job->file_size);
      if (qoi_init_job(
              &current_job->desc,
              current_job->read_dst,
              current_job->decode_dst))
        loader_panic("Failed to decode QOI");
      sceIoClose(current_job->fd);
      current_job->status = LAZYJOB_DECODING;
      break;

    // This should never be executed.
    default:
      break;
  }
  return 0;
}

int
loader_lazy_load(void) {
  if (current_job == NULL || current_job->status != LAZYJOB_DECODING)
    return !sceDisplayIsVblank();
  if (qoi_lazy_decode(&current_job->desc))
    return 0;
  sceKernelDcacheWritebackAll();
  current_job->callback(
      current_job->callback_data,
      current_job->desc.width,
      current_job->desc.height);
  free_mem(current_job->read_dst);
  ++queue_start;
  if (queue_start == LAZYJOBS_MAX)
    queue_start = 0;
  current_job->status = LAZYJOB_IDLE;
  current_job = &lazy_jobs[queue_start];
  if (current_job->status == LAZYJOB_PENDING) {
    current_job->status = LAZYJOB_SEEK;
    current_job->fd = sceIoOpenAsync(current_job->path, PSP_O_RDONLY, 0444);
    if (current_job->fd < 0)
      loader_panic("Could not open file");
    sceIoSetAsyncCallback(current_job->fd, asyncio_callback_id, current_job);
  }
  return 0;
#undef loader_panic
}

void
loader_start(void) {
  asyncio_callback_id =
      sceKernelCreateCallback("asyncio_callback", &asyncio_callback, NULL);
  if (asyncio_callback_id < 0)
    panic("Failed to create lazy loader callback.");
  memset(lazy_jobs, 0, sizeof(lazy_jobs));
  queue_end = 0;
  queue_start = 0;
  current_job = NULL;
}

void
loader_end(void) {
  sceKernelDeleteCallback(asyncio_callback_id);
}

void*
loader_read_file(const char* path, uint32_t* out_size) {
#define loader_panic(msg, ...) \
  panic("Error while reading file: %s\n" msg, path, ##__VA_ARGS__)
  SceUID fd;
  SceOff size;
  void* buffer;
  int bytes_read;

  fd = sceIoOpen(path, PSP_O_RDONLY, 0444);
  if (fd < 0)
    loader_panic("Could not open file");

  size = sceIoLseek(fd, 0, PSP_SEEK_END);
  sceIoLseek(fd, 0, PSP_SEEK_SET);

  buffer = alloc_mem(size);
  bytes_read = sceIoRead(fd, buffer, size);
  sceIoClose(fd);
  if (bytes_read < size)
    loader_panic(
        "Read bytes mismatch: read %u bytes out of %u",
        bytes_read,
        size);

  if (out_size != NULL)
    *out_size = size;

  return buffer;
#undef loader_panic
}

void
loader_lazy_swap_texture_ram(
    const char* path,
    void* dest,
    loader_lazyjob_finish_callback_t callback,
    void* callback_data) {
#define loader_panic(msg, ...) \
  panic("Error while swapping texture %s\n" msg, path, ##__VA_ARGS__)
  lazyjob_t* job;

  job = &lazy_jobs[queue_end];
  while (job->status != LAZYJOB_IDLE)
    sceKernelDelayThreadCB(1000);

  strncpy(job->path, path, LAZYJOB_PATH_LEN);
  job->decode_dst = dest;
  job->callback = callback;
  job->callback_data = callback_data;

  if (queue_end == queue_start) {
    current_job = &lazy_jobs[queue_start];
    job->status = LAZYJOB_SEEK;
    job->fd = sceIoOpenAsync(job->path, PSP_O_RDONLY, 0444);
    if (job->fd < 0)
      loader_panic("Could not open file");
    sceIoSetAsyncCallback(job->fd, asyncio_callback_id, NULL);
  } else
    job->status = LAZYJOB_PENDING;

  if (++queue_end == LAZYJOBS_MAX)
    queue_end = 0;
}

void
loader_swap_texture_ram(
    const char* path,
    void* dest,
    uint32_t* width,
    uint32_t* height) {
  uint32_t size;
  void* buffer;

  buffer = loader_read_file(path, &size);
  if (qoi_decode(buffer, dest, width, height))
    loader_panic("Could not decode QOI");
  loader_unload_file(buffer);

#undef loader_panic
}

void*
loader_load_texture_vram(const char* path, uint32_t* width, uint32_t* height) {
#define loader_panic(msg, ...) \
  panic("Error while loading texture: %s\n" msg, path, ##__VA_ARGS__)
  uint32_t size, w, h;
  void *buffer, *texture;

  buffer = loader_read_file(path, &size);
  qoi_decode(buffer, NULL, &w, &h);

  texture = alloc_vmem(w * h * 4);
  if (texture == NULL)
    loader_panic("Failed to allocate VRAM");

  if (qoi_decode(buffer, texture, NULL, NULL))
    loader_panic("Failed to decode QOI");

  loader_unload_file(buffer);

  if (width != NULL)
    *width = w;
  if (height != NULL)
    *height = h;

  return texture;
#undef loader_panic
}

void
loader_unload_file(void* buffer) {
  free_mem(buffer);
}

void
loader_unload_texture_vram(void* texture_ptr) {
  free_vmem(texture_ptr);
}
