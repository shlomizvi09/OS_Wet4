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

void print_meta_data(MallocMetadata* md) {  // for debugging
    std::cout << "current: " << md << std::endl;
    std::cout << md->size << std::endl;
    std::cout << md->is_free << std::endl;
    std::cout << md->user_pointer << std::endl;
    std::cout << "next: " << md->next << std::endl;
    std::cout << "prev :" << md->prev << std::endl;
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
    print_meta_data(new_meta_data);
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

int main() {
    int* num_ptr = (int*)smalloc(sizeof(int) * 100);
    for (int i = 0; i < 10; i++) {
        num_ptr[i] = i;
        std::cout << num_ptr[i] << std::endl;
    }
    int* num_ptr2 = (int*)smalloc(sizeof(int) * 50);
    int* num_ptr3 = (int*)smalloc(sizeof(int) * 50);

    return 0;
}