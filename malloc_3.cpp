#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#define MAX_SIZE 100000000
#define SBRK_FAIL (void*)(-1)
#define MINIMUM_REMAINDER 128  // Bytes

struct MallocMetadata {
    size_t size;
    bool is_free;
    void* user_pointer;
    MallocMetadata* next;
    MallocMetadata* prev;
};

MallocMetadata* _split_meta_data_block(MallocMetadata* old_meta_data_block, size_t wanted_size);

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
    MallocMetadata* new_meta_data_block = _get_first_free_block(size);
    if (new_meta_data_block != nullptr) {
        new_meta_data_block->is_free = false;
        if (new_meta_data_block->size >= size + _size_meta_data() + MINIMUM_REMAINDER) {
            new_meta_data_block = _split_meta_data_block(new_meta_data_block, size);
        }
        return new_meta_data_block->user_pointer;
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
    MallocMetadata* meta_data_block = _get_meta_data_block(p);
    if (meta_data_block == nullptr || meta_data_block->is_free) {
        return;
    }
    meta_data_block->is_free = true;
}

void* scalloc(size_t num, size_t size) {
    if (size * num == 0 || size * num > MAX_SIZE) {
        return nullptr;
    }
    MallocMetadata* new_meta_data_block = _get_first_free_block(size * num);
    if (new_meta_data_block != nullptr) {
        new_meta_data_block->is_free = false;
        memset(new_meta_data_block->user_pointer, 0, new_meta_data_block->size);
        return new_meta_data_block->user_pointer;
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

MallocMetadata* _split_meta_data_block(MallocMetadata* old_meta_data_block, size_t wanted_size) {
    if (old_meta_data_block->size < wanted_size + _size_meta_data() + MINIMUM_REMAINDER) {
        return nullptr;
    }
    size_t old_size = old_meta_data_block->size;
    MallocMetadata* new_used_meta_data_block = old_meta_data_block;
    new_used_meta_data_block->size = wanted_size;
    new_used_meta_data_block->is_free = false;
    new_used_meta_data_block->user_pointer = old_meta_data_block->user_pointer;

    MallocMetadata* remainder_meta_data_block = (MallocMetadata*)((size_t)new_used_meta_data_block->user_pointer + new_used_meta_data_block->size);
    remainder_meta_data_block->size = (int)(old_size - wanted_size - _size_meta_data());
    remainder_meta_data_block->is_free = true;
    remainder_meta_data_block->user_pointer = (void*)((size_t)new_used_meta_data_block->user_pointer + wanted_size + _size_meta_data());

    remainder_meta_data_block->next = old_meta_data_block->next;
    remainder_meta_data_block->prev = new_used_meta_data_block;
    (old_meta_data_block->next)->prev = remainder_meta_data_block;
    (old_meta_data_block->prev)->next = new_used_meta_data_block;
    new_used_meta_data_block->next = remainder_meta_data_block;
    new_used_meta_data_block->prev = old_meta_data_block->prev;

    return new_used_meta_data_block;
}

int main() {
    int* ptr1 = (int*)smalloc(150);
    int* ptr2 = (int*)smalloc(1000);
    sfree(ptr2);
    int* ptr4 = (int*)smalloc(200);
    print_meta_data();

    ptr1 = (int*)srealloc(ptr1, 200);

    print_meta_data();
    return 0;
}