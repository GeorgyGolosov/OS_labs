#include "binary_allocator.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

// Рекурсивное освобождение элементов списка ForwardMemory
static void recursive_free_forward_memory(ForwardMemory* fm) {
    if (!fm) return;
    recursive_free_forward_memory(fm->next);
    free(fm);
}

BinaryAllocator* bin_alloc_create(uint64_t byte_count) {
    uint64_t max_order = closest_n_pow2(byte_count);
    byte_count = pow2(max_order);

    // Выделяем общий пул памяти
    void* memory = calloc(byte_count, 1);
    assert(memory != nullptr);

    // Создаём массив блоков – один блок на каждый байт пула
    Block* blocks = (Block*) calloc(byte_count, sizeof(Block));
    assert(blocks != nullptr);
    for (uint64_t i = 0; i < byte_count; ++i) {
        blocks[i].memory = static_cast<char*>(memory) + i;
        blocks[i].taken = 0;
    }

    // Выделяем массив указателей на списки свободных блоков
    ForwardMemory** free_blocks = (ForwardMemory**) calloc(max_order + 1, sizeof(ForwardMemory*));
    assert(free_blocks != nullptr);
    // Инициализируем список для максимального порядка – весь пул как один свободный блок
    free_blocks[max_order] = (ForwardMemory*) calloc(1, sizeof(ForwardMemory));
    assert(free_blocks[max_order] != nullptr);
    free_blocks[max_order]->memory = memory;
    free_blocks[max_order]->next = nullptr;

    BinaryAllocator* ba = (BinaryAllocator*) calloc(1, sizeof(BinaryAllocator));
    ba->blocks = blocks;
    ba->free_blocks = free_blocks;
    ba->max_order = max_order;

    return ba;
}

BinaryAllocator* bin_alloc_create_with_block_size(uint64_t block_count, uint64_t block_size) {
    return bin_alloc_create(block_count * block_size);
}

void bin_alloc_destroy(BinaryAllocator* ba) {
    if (!ba) return;
    free(ba->blocks[0].memory);
    free(ba->blocks);
    for (uint64_t i = 0; i <= ba->max_order; ++i) {
        recursive_free_forward_memory(ba->free_blocks[i]);
    }
    free(ba->free_blocks);
    free(ba);
}

// Вспомогательная функция, которая делит блок указанного порядка до уровня,
// на котором его размер удовлетворяет требуемому числу байт.
void* bin_alloc_divide_block(BinaryAllocator* ba, uint64_t order, uint64_t bytes_needed) {
    if (order == 0 || order > ba->max_order)
        return nullptr;

    void* result = nullptr;
    // Извлекаем блок из списка free_blocks[order]
    void* memory1 = ba->free_blocks[order]->memory;
    void* memory2 = static_cast<char*>(ba->free_blocks[order]->memory) + pow2(order - 1);

    // Находим индекс блока в массиве blocks
    uint64_t block_index = 0;
    uint64_t total_blocks = pow2(ba->max_order);
    for (; block_index < total_blocks; ++block_index) {
        if (ba->blocks[block_index].memory == memory1)
            break;
    }

    ForwardMemory* next = ba->free_blocks[order]->next;
    free(ba->free_blocks[order]);
    ba->free_blocks[order] = next;

    // Формируем два новых блока для списка порядка order-1
    ForwardMemory* leftmost_free = ba->free_blocks[order - 1];

    ForwardMemory* new_block2 = (ForwardMemory*) calloc(1, sizeof(ForwardMemory));
    new_block2->memory = memory2;
    new_block2->next = leftmost_free;

    ForwardMemory* new_block1 = (ForwardMemory*) calloc(1, sizeof(ForwardMemory));
    new_block1->memory = memory1;
    new_block1->next = new_block2;

    ba->free_blocks[order - 1] = new_block1;

    if (pow2(order - 1) > bytes_needed) {
        result = bin_alloc_divide_block(ba, order - 1, bytes_needed);
    } else {
        result = memory1;
        ba->blocks[block_index].taken = bytes_needed;
        // Удаляем первый элемент списка free_blocks[order - 1]
        ForwardMemory* temp = ba->free_blocks[order - 1];
        ba->free_blocks[order - 1] = new_block2;
        free(temp);
    }

    return result;
}

void* bin_alloc_allocate(BinaryAllocator* ba, uint64_t bytes_needed) {
    // Округляем запрошенный объём до ближайшей степени двойки
    bytes_needed = closest_pow2(bytes_needed);
    uint64_t order = closest_n_pow2(bytes_needed);

    ForwardMemory* current_fm = ba->free_blocks[order];
    if (current_fm != nullptr) {
        uint64_t block_index = 0;
        uint64_t total_blocks = pow2(ba->max_order);
        for (; block_index < total_blocks; ++block_index) {
            if (ba->blocks[block_index].memory == current_fm->memory)
                break;
        }
        ba->blocks[block_index].taken = bytes_needed;

        ForwardMemory* next = current_fm->next;
        void* memory = current_fm->memory;
        ba->free_blocks[order] = next;
        free(current_fm);
        return memory;
    }

    while (order <= ba->max_order && ba->free_blocks[order] == nullptr) {
        ++order;
    }
    if (order > ba->max_order)
        return nullptr;

    return bin_alloc_divide_block(ba, order, bytes_needed);
}

uint64_t bin_alloc_deallocate(BinaryAllocator* ba, void* memory) {
    uint64_t block_index = 0;
    uint64_t total_blocks = pow2(ba->max_order);
    for (; block_index < total_blocks; ++block_index) {
        if (ba->blocks[block_index].memory == memory)
            break;
    }
    uint64_t order = closest_n_pow2(ba->blocks[block_index].taken);
    uint64_t result = ba->blocks[block_index].taken;
    ba->blocks[block_index].taken = 0;

    ForwardMemory* leftmost_free = ba->free_blocks[order];
    ForwardMemory* new_block = (ForwardMemory*) calloc(1, sizeof(ForwardMemory));
    new_block->memory = memory;
    new_block->next = leftmost_free;
    ba->free_blocks[order] = new_block;

    return result;
}

void bin_alloc_print(const BinaryAllocator& ba) {
    std::cout << "BinaryAllocator = {\n";
    if (ba.max_order < 6) {
        std::cout << "\tblocks = {\n\t\t";
        uint64_t total_blocks = pow2(ba.max_order);
        for (uint64_t i = 0; i < total_blocks; ++i) {
            std::cout << ba.blocks[i].memory << ", " << ba.blocks[i].taken << "; ";
        }
        std::cout << "\n\t}\n";
    }
    std::cout << "\tfree_blocks = {\n";
    for (uint64_t i = 0; i <= ba.max_order; ++i) {
        ForwardMemory* curr = ba.free_blocks[i];
        std::cout << "\t\t";
        if (!curr) {
            std::cout << "(nil); ";
        } else {
            while (curr != nullptr) {
                std::cout << curr->memory << "; ";
                curr = curr->next;
            }
        }
        std::cout << "\n";
    }
    std::cout << "\t}\n";
    std::cout << "\tmax_order = " << ba.max_order << "\n";
    std::cout << "}\n";
}
