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

    // Максимальное количество цифр для двоичного представления 64-битного числа – 64
    const int MAX_DIGITS = 65; // 64 цифры + 1 для '\0'
    char* temp = new char[MAX_DIGITS];
    int pos = 0;

    // Преобразуем число в двоичное представление (цифры записываются в обратном порядке)
    while (x > 0) {
        temp[pos++] = (x % 2) + '0';
        x /= 2;
    }

    // Вычисляем итоговый размер строки: если число отрицательное, добавляем 1 символ для знака
    int resultSize = pos + (isNegative ? 2 : 1);
    char* result = new char[resultSize];

    int j = 0;
    if (isNegative) {
        result[j++] = '-';
    }

    // Переворачиваем цифры из temp
    for (int i = pos - 1; i >= 0; i--) {
        result[j++] = temp[i];
    }
    result[j] = '\0';

    delete[] temp;
    return result;
}
