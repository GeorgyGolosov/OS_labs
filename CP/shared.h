#ifndef SHARED_H
#define SHARED_H

#include <cstdint>

// Возвращает наименьшую степень двойки, не меньшую n
inline uint64_t closest_pow2(uint64_t n) {
    if (n == 0) return 1;
    n -= 1;
    uint64_t result = 1;
    while (n != 0) {
        result *= 2;
        n /= 2;
    }
    return result;
}

// Возвращает порядок (количество делений на 2) для n (если n – степень двойки)
inline uint64_t closest_n_pow2(uint64_t n) {
    if (n == 0) return 0;
    n -= 1;
    uint64_t result = 0;
    while (n != 0) {
        result += 1;
        n /= 2;
    }
    return result;
}

// Возвращает 2^n
inline uint64_t pow2(uint64_t n) {
    return 1ULL << n;
}

struct ForwardMemory {
    void* memory;
    ForwardMemory* next;
};

#endif // SHARED_H
