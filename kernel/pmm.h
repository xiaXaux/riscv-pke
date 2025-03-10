#ifndef _PMM_H_
#define _PMM_H_

#include "riscv.h"

// Initialize phisical memeory manager
void pmm_init();
// Allocate a free phisical page
void* alloc_page();
// Free an allocated page
void free_page(void* pa);

int getrefcnt(void* pa);

void addrefcnt(void* pa);

int cowpage(pagetable_t pagetable, uint64 va);

int cowalloc(pagetable_t pagetable,uint64 va);

#endif