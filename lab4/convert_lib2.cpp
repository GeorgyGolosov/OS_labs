#include <cstdlib>

extern "C" char* translation(long x) {
    if (x == 0) {
        char* result = new char[2];
        result[0] = '0';
        result[1] = '\0';
        return result;
    }

    bool isNegative = (x < 0);
    if (isNegative) {
        x = -x;
    }

    // Для системы счисления с основанием 3 достаточно 42 цифр
    const int MAX_DIGITS = 42;
    char* temp = new char[MAX_DIGITS];
    int pos = 0;

    while (x > 0) {
        temp[pos++] = (x % 3) + '0';
        x /= 3;
    }

    int resultSize = pos + (isNegative ? 2 : 1);
    char* result = new char[resultSize];

    int j = 0;
    if (isNegative) {
        result[j++] = '-';
    }

    for (int i = pos - 1; i >= 0; i--) {
        result[j++] = temp[i];
    }
    result[j] = '\0';

    delete[] temp;
    return result;
}
