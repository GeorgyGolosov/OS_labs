#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cmath>
#include <cstring>

bool is_composite(int n) {
    if (n < 2) return false; // Отрицательное или 0, 1
    for (int i = 2; i <= sqrt(n); ++i) {
        if (n % i == 0) return true; // Число составное
    }
    return false; // Число простое
}

int main() {
    int number;
    if (read(STDIN_FILENO, &number, sizeof(number)) == -1) {
        std::cerr << "Ошибка при чтении числа" << std::endl;
        return 1;
    }

    if (number < 0) {
        return 1; // Число отрицательное
    }

    if (is_composite(number)) {
        // Если число составное, записываем его в файл
        int file = open("result.txt", O_WRONLY | O_CREAT | O_TRUNC);
        if (file == -1) {
            std::cerr << "Ошибка при открытии файла" << std::endl;
            return 1;
        }

        // Форматирование результата в строку
        char buffer[16];
        int n = snprintf(buffer, sizeof(buffer), "%d\n", number);
        if (n <= 0) {
            std::cerr << "Ошибка при форматировании строки" << std::endl;
            close(file);
            return 1;
        }

        // Запись строки в файл
        if (write(file, buffer, n) == -1) {
            std::cerr << "Ошибка при записи в файл" << std::endl;
            close(file);
            return 1;
        }

        close(file);
    } else {
        return 1;
    }

    return 0;
}
