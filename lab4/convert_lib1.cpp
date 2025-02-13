#include <cstdlib>

extern "C" char* translation(long x) {
    if (x == 0) {
        char* result = new char[2];
        result[0] = '0';
        result[1] = '\0';
        return result;
    }

    const int BITS = 64;
    char temp[BITS];

    bool isNegative = (x < 0);
    if (isNegative) {
        x = -(x + 1);
    }

    for (int i = BITS - 1; i >= 0; i--) {
        if (isNegative) {
            temp[i] = ((x & 1) ^ 1) + '0';
        } else {
            temp[i] = (x & 1) + '0';
        }
        x >>= 1;
    }

    int firstDigit = 0;
    while (firstDigit < BITS && temp[firstDigit] == '0') {
        firstDigit++;
    }
    if (firstDigit == BITS)
        firstDigit--;

    int resultSize = BITS - firstDigit + (isNegative ? 2 : 1);
    char* result = new char[resultSize];

    int j = 0;
    if (isNegative) {
        result[j++] = '-';
    }

    for (int i = firstDigit; i < BITS; i++) {
        result[j++] = temp[i];
    }
    result[j] = '\0';

    return result;
}
