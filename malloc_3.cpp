#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <iostream>

#define MAX_SIZE 100000000
#define SIZE_FOR_MMAP 128000
#define SBRK_FAIL (void*)(-1)
#define MINIMUM_REMAINDER 128  // Bytes

struct MallocMetadata {
    size_t size;
    bool is_free;
    bool is_mmap;
    void* user_pointer;
    MallocMetadata* next;
    MallocMetadata* prev;
};

MallocMetadata* _split_meta_data_block(MallocMetadata* old_meta_data_block, size_t wanted_size);
MallocMetadata* _get_wilderness_chunk();
void* srealloc(void* oldp, size_t size);
MallocMetadata dummy_pointer = {0, false, false, nullptr, &dummy_pointer, &dummy_pointer};
MallocMetadata mmap_dummy_pointer = {0, false, true, nullptr, &mmap_dummy_pointer, &mmap_dummy_pointer};
void* _srealloc_wilderness_chunk(MallocMetadata* wilderness_chunk, size_t size);

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
    std::cout << "### sbrk allocated ###" << std::endl;
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
    md = mmap_dummy_pointer.next;
    std::cout << std::endl;
    std::cout << "### mmap allocated ###" << std::endl;
    while (md != &mmap_dummy_pointer) {
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

void _init_and_append_meta_data(MallocMetadata* new_meta_data, size_t size, bool is_mmap) {
    new_meta_data->size = size;
    new_meta_data->is_free = false;
    new_meta_data->is_mmap = is_mmap;
    new_meta_data->user_pointer = (void*)((size_t)new_meta_data + _size_meta_data());
    if (is_mmap) {
        new_meta_data->next = &mmap_dummy_pointer;
        new_meta_data->prev = mmap_dummy_pointer.prev;
        (mmap_dummy_pointer.prev)->next = new_meta_data;
        mmap_dummy_pointer.prev = new_meta_data;
    } else {
        new_meta_data->next = &dummy_pointer;
        new_meta_data->prev = dummy_pointer.prev;
        (dummy_pointer.prev)->next = new_meta_data;
        dummy_pointer.prev = new_meta_data;
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
    _init_and_append_meta_data(new_meta_data, size, false);
    return new_meta_data->user_pointer;
}

void* _alloc_with_mmap(size_t size) {
    void* result = mmap(nullptr, size + _size_meta_data(), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (result == MAP_FAILED) {
        return nullptr;
    }
    MallocMetadata* new_meta_data = (MallocMetadata*)result;
    _init_and_append_meta_data(new_meta_data, size, true);
    return new_meta_data->user_pointer;
}

void* smalloc(size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }
    if (size >= SIZE_FOR_MMAP) {
        return _alloc_with_mmap(size);
    }
    MallocMetadata* new_meta_data_block = _get_first_free_block(size);
    if (new_meta_data_block != nullptr) {
        new_meta_data_block->is_free = false;
        if (new_meta_data_block->size >= size + _size_meta_data() + MINIMUM_REMAINDER) {
            new_meta_data_block = _split_meta_data_block(new_meta_data_block, size);
        }
        return new_meta_data_block->user_pointer;
    } else {
        MallocMetadata* wilderness_chunk = _get_wilderness_chunk();
        if (wilderness_chunk != nullptr && wilderness_chunk->is_free) {
            return _srealloc_wilderness_chunk(wilderness_chunk, size);
        }
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
    iter = mmap_dummy_pointer.next;
    while (iter != &mmap_dummy_pointer) {
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
    iter = mmap_dummy_pointer.next;
    while (iter != &mmap_dummy_pointer) {
        counter += iter->size;
        iter = iter->next;
    }
    return counter;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(struct MallocMetadata);
}

void _remove_meta_data_node_from_list(MallocMetadata* meta_data) {
    if (meta_data == nullptr) {
        return;
    }
    meta_data->next->prev = meta_data->prev;
    meta_data->prev->next = meta_data->next;
}

void _merge_two_free_blocks(MallocMetadata* lower_block, MallocMetadata* upper_block) {
    if (lower_block == nullptr || upper_block == nullptr) {
        return;
    }
    lower_block->size += upper_block->size + _size_meta_data();
    _remove_meta_data_node_from_list(upper_block);
}

void _free_mmapped_block(MallocMetadata* meta_data) {
    MallocMetadata* next = meta_data->next;
    MallocMetadata* prev = meta_data->prev;
    if (munmap(meta_data, meta_data->size + _size_meta_data()) == -1) {  // munmap failed
        return;
    }
    next->prev = prev;  // removing from mmap list
    prev->next = next;  // removing from mmap list
}

void sfree(void* p) {
    if (p == nullptr) {
        return;
    }
    MallocMetadata* meta_data_block = _get_meta_data_block(p);
    if (meta_data_block == nullptr || meta_data_block->is_free) {
        return;
    }
    if (meta_data_block->is_mmap) {
        _free_mmapped_block(meta_data_block);
        return;
    }
    meta_data_block->is_free = true;
    if (meta_data_block->next != &dummy_pointer && meta_data_block->next->is_free) {  // merge with upper block
        _merge_two_free_blocks(meta_data_block, meta_data_block->next);
    }
    if (meta_data_block->prev != &dummy_pointer && meta_data_block->prev->is_free) {  // merge with lower block
        _merge_two_free_blocks(meta_data_block->prev, meta_data_block);
    }
}

void* scalloc(size_t num, size_t size) {
    if (size * num == 0 || size * num > MAX_SIZE) {
        return nullptr;
    }
    if (size * num >= SIZE_FOR_MMAP) {
        return _alloc_with_mmap(size * num);
    }
    MallocMetadata* new_meta_data_block = _get_first_free_block(size * num);
    if (new_meta_data_block != nullptr) {
        new_meta_data_block->is_free = false;
        memset(new_meta_data_block->user_pointer, 0, new_meta_data_block->size);
        return new_meta_data_block->user_pointer;
    }
    MallocMetadata* wilderness_chunk = _get_wilderness_chunk();
    if (wilderness_chunk != nullptr && wilderness_chunk->is_free) {
        return _srealloc_wilderness_chunk(wilderness_chunk, size * num);
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
        if (meta_data_block->size - size <= _size_meta_data() + MINIMUM_REMAINDER) {
            meta_data_block = _split_meta_data_block(meta_data_block, size);
        }
        return meta_data_block->user_pointer;
    } else if (meta_data_block->prev != &dummy_pointer && (meta_data_block->prev)->is_free && size <= meta_data_block->size + (meta_data_block->prev)->size + _size_meta_data()) {
        void* data_ptr = meta_data_block->user_pointer;
        size_t size_to_copy = meta_data_block->size;
        MallocMetadata* new_meta_data_block = meta_data_block->prev;
        new_meta_data_block->next = meta_data_block->next;
        (meta_data_block->next)->prev = new_meta_data_block;
        new_meta_data_block->size += meta_data_block->size + _size_meta_data();
        memcpy(new_meta_data_block->user_pointer, data_ptr, size_to_copy);
        new_meta_data_block->is_free = false;
        MallocMetadata* temp_res = _split_meta_data_block(new_meta_data_block, size);
        if (temp_res != nullptr) {
            new_meta_data_block = temp_res;
        }
        return new_meta_data_block->user_pointer;

    } else if (meta_data_block->next != &dummy_pointer && (meta_data_block->next)->is_free && size <= meta_data_block->size + (meta_data_block->next)->size + _size_meta_data()) {
        MallocMetadata* new_meta_data_block = meta_data_block;
        new_meta_data_block->next = meta_data_block->next->next;
        (meta_data_block->next->next)->prev = new_meta_data_block;
        new_meta_data_block->size += meta_data_block->next->size + _size_meta_data();
        MallocMetadata* temp_res = _split_meta_data_block(new_meta_data_block, size);
        if (temp_res != nullptr) {
            new_meta_data_block = temp_res;
        }
        return new_meta_data_block->user_pointer;
    }
    void* smalloc_res = smalloc(size);
    if (smalloc_res == nullptr) {
        return nullptr;
    }
    memcpy(smalloc_res, oldp, meta_data_block->size);
    sfree(oldp);
    return smalloc_res;
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

MallocMetadata* _get_wilderness_chunk() {
    if (dummy_pointer.next == &dummy_pointer) {  // list is empty
        return nullptr;
    }
    MallocMetadata* res = dummy_pointer.next;
    while (res->next != &dummy_pointer) {
        res = res->next;
    }
    return res;
}

void* _srealloc_wilderness_chunk(MallocMetadata* wilderness_chunk, size_t size) {
    if (wilderness_chunk == nullptr) {
        return nullptr;
    }
    if (wilderness_chunk->size >= size) {
        return nullptr;
    }
    void* res = sbrk(size - wilderness_chunk->size);
    if (res == SBRK_FAIL) {
        return nullptr;
    }
    wilderness_chunk->size = size;
    wilderness_chunk->is_free = false;
    return wilderness_chunk->user_pointer;
}

int main() {
    void* ptr2 = smalloc(100);
    for (size_t i = 1; i <= 20; i++) {
        sfree(ptr2);
        ptr2 = scalloc(100 + 10 * i, 10);
    }
    print_meta_data();
    return 0;
}