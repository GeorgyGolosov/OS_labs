#include "buddy_allocator.h"
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <cinttypes>

// Рекурсивное освобождение элементов списка ForwardMemory
void recursive_free_forward_memory(ForwardMemory* fm) {
    if (!fm) return;
    recursive_free_forward_memory(fm->next);
    free(fm);
}

BuddyAllocator* buddy_create(uint64_t byte_count) {
    // Округляем общий размер до ближайшей степени двойки и вычисляем max_order
    byte_count = closest_pow2(byte_count);
    uint64_t max_order = closest_n_pow2(byte_count);

    // Выделяем пул памяти через calloc (инициализирован нулями)
    void* memory = calloc(byte_count, 1);
    assert(memory != nullptr);

    // Выделяем массив списков свободных блоков (по порядкам от 0 до max_order)
    ForwardMemory** free_blocks = (ForwardMemory**) calloc(max_order + 1, sizeof(ForwardMemory*));
    assert(free_blocks != nullptr);

    // Инициализируем список для максимального порядка – весь пул как один свободный блок
    free_blocks[max_order] = (ForwardMemory*) calloc(1, sizeof(ForwardMemory));
    assert(free_blocks[max_order] != nullptr);
    free_blocks[max_order]->memory = memory;
    free_blocks[max_order]->next = nullptr;

    // Создаём сам аллокатор
    BuddyAllocator* ba = (BuddyAllocator*) calloc(1, sizeof(BuddyAllocator));
    assert(ba != nullptr);

    ba->free_blocks = free_blocks;
    ba->max_order = max_order;
    ba->mem_start = memory;

    return ba;
}

BuddyAllocator* buddy_create_with_block_size(uint64_t block_count, uint64_t block_size) {
    return buddy_create(block_count * block_size);
}

void buddy_destroy(BuddyAllocator* ba) {
    for (uint64_t i = 0; i <= ba->max_order; ++i) {
        recursive_free_forward_memory(ba->free_blocks[i]);
    }
    free(ba->free_blocks);
    free(ba->mem_start);
    free(ba);
}

// Вспомогательная функция для деления блока до требуемого порядка (order_needed)
void* buddy_divide_block(BuddyAllocator* ba, uint64_t order, uint64_t order_needed) {
    void* result = nullptr;
    uint64_t shift = pow2(order - 1);

    // Удаляем блок из списка free_blocks[order]
    void* memory = ba->free_blocks[order]->memory;
    ForwardMemory* next = ba->free_blocks[order]->next;
    free(ba->free_blocks[order]);
    ba->free_blocks[order] = next;

    // Формируем правый "близнец" – смещён на shift байт
    ForwardMemory* right_buddy = (ForwardMemory*) calloc(1, sizeof(ForwardMemory));
    assert(right_buddy != nullptr);
    right_buddy->memory = static_cast<char*>(memory) + shift;
    right_buddy->next = ba->free_blocks[order - 1];
    ba->free_blocks[order - 1] = right_buddy;

    // Если достигнут нужный порядок, возвращаем левый блок; иначе – делим левый блок дальше
    if (order - 1 != order_needed) {
        ForwardMemory* left_buddy = (ForwardMemory*) calloc(1, sizeof(ForwardMemory));
        assert(left_buddy != nullptr);
        left_buddy->memory = memory;
        left_buddy->next = ba->free_blocks[order - 1];
        ba->free_blocks[order - 1] = left_buddy;

        result = buddy_divide_block(ba, order - 1, order_needed);
    } else {
        result = memory;
    }

    return result;
}

void* buddy_allocate(BuddyAllocator* ba, uint64_t bytes_needed) {
    // Вычисляем требуемый порядок для запрошенного количества байт
    uint64_t order_needed = closest_n_pow2(bytes_needed);
    uint64_t order = order_needed;
    if (order > ba->max_order) return nullptr;

    // Если в списке свободных блоков нужного порядка есть блок – возвращаем его
    ForwardMemory* current_fm = ba->free_blocks[order];
    if (current_fm != nullptr) {
        ForwardMemory* next = current_fm->next;
        void* memory = current_fm->memory;
        ba->free_blocks[order] = next;
        free(current_fm);
        return memory;
    }

    // Если блока нужного порядка нет, ищем блок большего порядка
    while (order <= ba->max_order && ba->free_blocks[order] == nullptr) {
        ++order;
    }
    if (order > ba->max_order) return nullptr;

    // Делим блок до нужного порядка
    return buddy_divide_block(ba, order, order_needed);
}

void buddy_merge(BuddyAllocator* ba, uint64_t order) {
    // Функция объединяет свободные блоки одинакового порядка в блоки более высокого порядка
    void* memory = ba->free_blocks[order]->memory;
    uint64_t byte_count = pow2(order);
    int is_right = ((static_cast<char*>(memory) - static_cast<char*>(ba->mem_start)) / byte_count) % 2;

    ForwardMemory* buddy_left = nullptr;
    ForwardMemory* buddy_right = nullptr;
    void* buddy_memory = nullptr;

    if (is_right) {
        buddy_right = ba->free_blocks[order];
        buddy_memory = static_cast<char*>(memory) - byte_count;
    } else {
        buddy_left = ba->free_blocks[order];
        buddy_memory = static_cast<char*>(memory) + byte_count;
    }

    ForwardMemory* previous = nullptr;
    ForwardMemory* current = ba->free_blocks[order]->next;

    // Поиск блока-близнеца в списке
    while (current != nullptr) {
        if (current->memory == buddy_memory) {
            if (is_right)
                buddy_left = current;
            else
                buddy_right = current;
            break;
        }
        previous = current;
        current = current->next;
    }

    if (buddy_left && buddy_right) {
        // Объединяем блоки в один блок более высокого порядка
        ForwardMemory* merged = (ForwardMemory*) calloc(1, sizeof(ForwardMemory));
        assert(merged != nullptr);
        merged->memory = buddy_left->memory;

        // Удаляем найденные блоки из списка
        if (is_right)
            ba->free_blocks[order] = buddy_right->next;
        else
            ba->free_blocks[order] = buddy_left->next;

        if (previous != nullptr) {
            if (is_right)
                previous->next = buddy_left->next;
            else
                previous->next = buddy_right->next;
        } else {
            if (is_right)
                ba->free_blocks[order] = buddy_left->next;
            else
                ba->free_blocks[order] = buddy_right->next;
        }

        free(buddy_right);
        free(buddy_left);

        // Добавляем объединённый блок в список более высокого порядка
        merged->next = ba->free_blocks[order + 1];
        ba->free_blocks[order + 1] = merged;

        buddy_merge(ba, order + 1);
    }
}

uint64_t buddy_deallocate(BuddyAllocator* ba, void* memory, uint64_t byte_count) {
    // Приводим переданное количество байт к ближайшей степени двойки и вычисляем порядок
    byte_count = closest_pow2(byte_count);
    uint64_t order = closest_n_pow2(byte_count);
    if (order >= ba->max_order) return 0;

    // Определяем, является ли данный блок правым близнецом
    int is_right = ((static_cast<char*>(memory) - static_cast<char*>(ba->mem_start)) / byte_count) % 2;

    ForwardMemory* buddy_left = nullptr;
    ForwardMemory* buddy_right = nullptr;
    void* buddy_memory = nullptr;

    if (is_right) {
        buddy_right = (ForwardMemory*) calloc(1, sizeof(ForwardMemory));
        assert(buddy_right != nullptr);
        buddy_right->memory = memory;
        buddy_right->next = nullptr;
        buddy_memory = static_cast<char*>(memory) - byte_count;
    } else {
        buddy_left = (ForwardMemory*) calloc(1, sizeof(ForwardMemory));
        assert(buddy_left != nullptr);
        buddy_left->memory = memory;
        buddy_left->next = nullptr;
        buddy_memory = static_cast<char*>(memory) + byte_count;
    }

    ForwardMemory* previous = nullptr;
    ForwardMemory* current = ba->free_blocks[order];

    // Поиск блока-близнеца в списке
    while (current != nullptr) {
        if (current->memory == buddy_memory) {
            if (is_right)
                buddy_left = current;
            else
                buddy_right = current;
            break;
        }
        previous = current;
        current = current->next;
    }

    if (buddy_left && buddy_right) {
        ForwardMemory* merged = (ForwardMemory*) calloc(1, sizeof(ForwardMemory));
        assert(merged != nullptr);
        merged->memory = buddy_left->memory;

        if (previous != nullptr) {
            if (is_right)
                previous->next = buddy_left->next;
            else
                previous->next = buddy_right->next;
        } else {
            if (is_right)
                ba->free_blocks[order] = buddy_left->next;
            else
                ba->free_blocks[order] = buddy_right->next;
        }

        free(buddy_right);
        free(buddy_left);

        merged->next = ba->free_blocks[order + 1];
        ba->free_blocks[order + 1] = merged;

        buddy_merge(ba, order + 1);
    } else {
        // Если объединение невозможно, просто добавляем блок в соответствующий список
        ForwardMemory* leftmost_fm = (is_right ? buddy_right : buddy_left);
        leftmost_fm->next = ba->free_blocks[order];
        ba->free_blocks[order] = leftmost_fm;
    }

    return byte_count;
}

void buddy_print(const BuddyAllocator& ba) {
    std::printf("BuddyAllocator = {\n");
    std::printf("\tfree_blocks = {\n");
    for (uint64_t i = 0; i <= ba.max_order; ++i) {
        ForwardMemory* fm = ba.free_blocks[i];
        std::printf("\t\t");
        if (!fm) {
            std::printf("(nil); ");
        } else {
            while (fm != nullptr) {
                std::printf("%p; ", fm->memory);
                fm = fm->next;
            }
        }
        std::printf("\n");
    }
    std::printf("\t}\n");
    std::printf("\tmax_order = %" PRIu64 "\n", ba.max_order);
    std::printf("\tmem_start = %p\n", ba.mem_start);
    std::printf("}\n");
}
