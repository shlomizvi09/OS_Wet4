#ifndef _MALLOC_3_H_
#define _MALLOC_3_H_

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <iostream>

#define MAX_SIZE 100000000
#define SIZE_FOR_MMAP 128 * 1024
#define SBRK_FAIL (void*)(-1)
#define MINIMUM_REMAINDER 128  // Bytes

struct MallocMetadata;
void* smalloc(size_t size);
void* scalloc(size_t num, size_t size);
void* srealloc(void* oldp, size_t size);
void sfree(void* p);

#endif