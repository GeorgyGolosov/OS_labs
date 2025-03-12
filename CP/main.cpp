#include "binary_allocator.h"
#include "buddy_allocator.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <cstdlib>

int main() {
    // Размер пула памяти для обоих аллокаторов (в байтах)
    const uint64_t poolSize = 1024;

    // Создаем бинарный аллокатор (блоки по 2^n)
    BinaryAllocator* binAlloc = bin_alloc_create(poolSize);
    std::cout << "Бинарный аллокатор (блоки по 2^n) - начальное состояние:" << std::endl;
    bin_alloc_print(*binAlloc);

    // Создаем buddy-аллокатор
    BuddyAllocator* buddyAlloc = buddy_create(poolSize);
    std::cout << "\nBuddy-аллокатор - начальное состояние:" << std::endl;
    buddy_print(*buddyAlloc);

    // --- Тест выделения и освобождения ---
    std::cout << "\n=== Тест: Выделение и освобождение блоков ===" << std::endl;
    // Выделяем блоки разного размера
    void* binPtr1 = bin_alloc_allocate(binAlloc, 50);
    void* binPtr2 = bin_alloc_allocate(binAlloc, 100);
    std::cout << "\nБинарный аллокатор после выделения 50 и 100 байт:" << std::endl;
    bin_alloc_print(*binAlloc);

    void* buddyPtr1 = buddy_allocate(buddyAlloc, 50);
    void* buddyPtr2 = buddy_allocate(buddyAlloc, 100);
    std::cout << "\nBuddy-аллокатор после выделения 50 и 100 байт:" << std::endl;
    buddy_print(*buddyAlloc);

    // Освобождаем один блок в каждом аллокаторе
    uint64_t binFreed = bin_alloc_deallocate(binAlloc, binPtr1);
    std::cout << "\nБинарный аллокатор: освобожден блок размером " << binFreed << " байт" << std::endl;
    bin_alloc_print(*binAlloc);

    uint64_t buddyFreed = buddy_deallocate(buddyAlloc, buddyPtr1, 50);
    std::cout << "\nBuddy-аллокатор: освобожден блок размером " << buddyFreed << " байт" << std::endl;
    buddy_print(*buddyAlloc);

    // --- Тест производительности ---
    std::cout << "\n=== Тест производительности (1000 циклов выделения/освобождения) ===" << std::endl;
    const int iterations = 1000;
    const uint64_t testSize = 32; // фиксированный размер для теста
    std::vector<void*> binPtrs;
    std::vector<void*> buddyPtrs;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        void* ptr = bin_alloc_allocate(binAlloc, testSize);
        if (ptr) {
            binPtrs.push_back(ptr);
        }
    }
    for (auto ptr : binPtrs) {
        bin_alloc_deallocate(binAlloc, ptr);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto binDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        void* ptr = buddy_allocate(buddyAlloc, testSize);
        if (ptr) {
            buddyPtrs.push_back(ptr);
        }
    }
    for (auto ptr : buddyPtrs) {
        buddy_deallocate(buddyAlloc, ptr, testSize);
    }
    end = std::chrono::high_resolution_clock::now();
    auto buddyDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "\nРезультаты теста производительности:" << std::endl;
    std::cout << "Бинарный аллокатор: " << binDuration
              << " мкс для " << iterations << " выделений/освобождений." << std::endl;
    std::cout << "Buddy-аллокатор:   " << buddyDuration
              << " мкс для " << iterations << " выделений/освобождений." << std::endl;

    // Освобождаем аллокаторы
    bin_alloc_destroy(binAlloc);
    buddy_destroy(buddyAlloc);

    return 0;
}
