#ifndef BUDDY_ALLOCATOR_H
#define BUDDY_ALLOCATOR_H

#include "shared.h"
#include <cstdint>
#include <cstdio>


// Структура buddy‑аллокатора: содержит массив списков свободных блоков по порядкам и начальный адрес пула памяти
struct BuddyAllocator {
    ForwardMemory** free_blocks; // free_blocks[i] – список блоков с размером 2^i
    uint64_t max_order;
    void* mem_start;
};

// Функции создания/уничтожения аллокатора и операций выделения/освобождения
BuddyAllocator* buddy_create(uint64_t byte_count);
BuddyAllocator* buddy_create_with_block_size(uint64_t block_count, uint64_t block_size);
void buddy_destroy(BuddyAllocator* ba);

void* buddy_allocate(BuddyAllocator* ba, uint64_t bytes_needed);
uint64_t buddy_deallocate(BuddyAllocator* ba, void* memory, uint64_t byte_count);

void buddy_print(const BuddyAllocator& ba);

// Вспомогательная функция для рекурсивного освобождения односвязных списков
void recursive_free_forward_memory(ForwardMemory* fm);

#endif // BUDDY_ALLOCATOR_H
