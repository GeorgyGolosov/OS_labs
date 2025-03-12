#ifndef BINARY_ALLOCATOR_H
#define BINARY_ALLOCATOR_H

#include "shared.h"
#include <cstdint>
#include <iostream>

// Структура, описывающая отдельный блок памяти
struct Block {
    void* memory;
    uint64_t taken; // если 0 – блок свободен, иначе хранится запрошенный объём
};


// Структура аллокатора, содержащая массив блоков и массив списков свободных блоков по порядкам
struct BinaryAllocator {
    Block* blocks;
    ForwardMemory** free_blocks; // free_blocks[i] – список блоков с ёмкостью 2^i
    uint64_t max_order;
};

// Функции создания/уничтожения аллокатора и операций выделения/освобождения
BinaryAllocator* bin_alloc_create(uint64_t byte_count);
BinaryAllocator* bin_alloc_create_with_block_size(uint64_t block_count, uint64_t block_size);
void bin_alloc_destroy(BinaryAllocator* ba);

void* bin_alloc_allocate(BinaryAllocator* ba, uint64_t byte_count);
uint64_t bin_alloc_deallocate(BinaryAllocator* ba, void* memory);

void bin_alloc_print(const BinaryAllocator& ba);

#endif
