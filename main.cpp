#include <unistd.h>

#include <iostream>
#define MAX_SIZE 100000000
#define SBRK_FAIL (void*)(-1)

void* smalloc(size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        perror("invalid size");
        return nullptr;
    }
    void* result = sbrk(size);
    if (result == SBRK_FAIL) {
        perror("sbrk fail");
        return nullptr;
    }
    return result;
}

int main() {
    int size = 100;
    int* test_1 = (int*)smalloc(sizeof(int) * size);
    std::cout << SBRK_FAIL << std::endl;
    /*for (size_t i = 0; i < size; i++) {
        test[i] = rand() % 100;
        std::cout << "iteration: " << i << std::endl;
        std::cout << "added " << test[i] << " to test" << std::endl;
    }*/
}