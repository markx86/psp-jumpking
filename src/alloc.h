#ifndef __ALLOC_H__
#define __ALLOC_H__

#include <malloc.h>
#include <stdint.h>
#include <vramalloc.h>

#define get_texture_size(w, h, psm) vgetMemorySize(w, h, psm)
#define alloc_mem(s) malloc(s)
#define alloc_vmem(s) vramalloc(s)
#define free_mem(p) free(p)
#define free_vmem(p) vfree(p)
#define vmem_to_mem(p) vabsptr(p)
#define mem_to_vmem(p) vrelptr(p)

#endif
