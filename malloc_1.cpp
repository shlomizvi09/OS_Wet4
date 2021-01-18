#include <unistd.h>
#define MAX_SIZE 100000000
#define SBRK_FAIL (void*)(-1)

void* smalloc(size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }
    void* result = sbrk(size);
    if (result == SBRK_FAIL) {
        return nullptr;
    }
    return result;
}