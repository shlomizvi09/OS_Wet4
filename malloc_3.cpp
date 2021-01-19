#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#define MAX_SIZE 100000000
#define SBRK_FAIL (void*)(-1)

struct MallocMetadata {
    size_t size;
    bool is_free;
    void* user_pointer;
    MallocMetadata* next;
    MallocMetadata* prev;
};

MallocMetadata dummy_pointer = {0, false, nullptr, &dummy_pointer, &dummy_pointer};

size_t _size_meta_data() {
    return (size_t)sizeof(struct MallocMetadata);
}

MallocMetadata* _get_meta_data_block(void* p) {
    if (p == nullptr) {
        return nullptr;
    }
    MallocMetadata* iter = dummy_pointer.next;
    while (iter != &dummy_pointer) {
        if (iter->user_pointer == p) {
            return iter;
        }
        iter = iter->next;
    }
    return nullptr;
}

void print_meta_data() {  // for debugging
    MallocMetadata* md = dummy_pointer.next;
    while (md != &dummy_pointer) {
        std::cout << "metadata pointer: " << md << std::endl;
        std::cout << md->size << std::endl;
        if (md->is_free)
            std::cout << "Free" << std::endl;
        else
            std::cout << "NOT Free" << std::endl;

        std::cout << "actual block pointer: " << md->user_pointer << std::endl;
        std::cout << "next: " << md->next << std::endl;
        std::cout << "prev :" << md->prev << std::endl;
        std::cout << std::endl;
        md = md->next;
    }
}

MallocMetadata* _get_first_free_block(size_t size) {
    MallocMetadata* iter = dummy_pointer.next;
    while (iter != &dummy_pointer) {
        if (size <= iter->size && iter->is_free) {
            return iter;
        }
        iter = iter->next;
    }
    return nullptr;
}

void* _smalloc_aux(size_t size) {
    void* block = sbrk(size + _size_meta_data());
    if (block == SBRK_FAIL) {
        return nullptr;
    }
    MallocMetadata* new_meta_data = (MallocMetadata*)block;
    new_meta_data->size = size;
    new_meta_data->is_free = false;
    new_meta_data->user_pointer = (void*)((size_t)block + _size_meta_data());
    new_meta_data->next = &dummy_pointer;
    new_meta_data->prev = dummy_pointer.prev;
    (dummy_pointer.prev)->next = new_meta_data;
    dummy_pointer.prev = new_meta_data;
    return new_meta_data->user_pointer;
}

void* smalloc(size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }
    MallocMetadata* first_free_block = _get_first_free_block(size);
    if (first_free_block != nullptr) {
        first_free_block->is_free = false;
        return first_free_block->user_pointer;
    }
    return _smalloc_aux(size);
}

size_t _num_free_blocks() {
    size_t counter = 0;
    MallocMetadata* iter = dummy_pointer.next;
    while (iter != &dummy_pointer) {
        if (iter->is_free)
            counter++;
        iter = iter->next;
    }
    return counter;
}

size_t _num_free_bytes() {
    size_t counter = 0;
    MallocMetadata* iter = dummy_pointer.next;
    while (iter != &dummy_pointer) {
        if (iter->is_free)
            counter += iter->size;
        iter = iter->next;
    }
    return counter;
}

size_t _num_allocated_blocks() {
    size_t counter = 0;
    MallocMetadata* iter = dummy_pointer.next;
    while (iter != &dummy_pointer) {
        counter++;
        iter = iter->next;
    }
    return counter;
}

size_t _num_allocated_bytes() {
    size_t counter = 0;
    MallocMetadata* iter = dummy_pointer.next;
    while (iter != &dummy_pointer) {
        counter += iter->size;
        iter = iter->next;
    }
    return counter;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(struct MallocMetadata);
}

void sfree(void* p) {
    MallocMetadata* block_meta_data = _get_meta_data_block(p);
    if (block_meta_data == nullptr || block_meta_data->is_free) {
        return;
    }
    block_meta_data->is_free = true;
}

void* scalloc(size_t num, size_t size) {
    if (size * num == 0 || size * num > MAX_SIZE) {
        return nullptr;
    }
    MallocMetadata* first_free_block = _get_first_free_block(size * num);
    if (first_free_block != nullptr) {
        first_free_block->is_free = false;
        memset(first_free_block->user_pointer, 0, first_free_block->size);
        return first_free_block->user_pointer;
    }
    void* result = _smalloc_aux(size * num);
    if (result == nullptr) {
        return nullptr;
    }
    memset(result, 0, size * num);
    return result;
}

void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }
    if (oldp == nullptr) {
        return smalloc(size);
    }
    MallocMetadata* meta_data_block = _get_meta_data_block(oldp);
    if (size <= meta_data_block->size) {
        return meta_data_block->user_pointer;
    }
    void* smalloc_res = smalloc(size);
    if (smalloc_res == nullptr) {
        return nullptr;
    }
    if (memcpy(smalloc_res, oldp, meta_data_block->size) != 0) {
        sfree(oldp);
        return smalloc_res;
    }
    return nullptr;
}