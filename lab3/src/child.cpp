#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <cmath>
#include <cstring>

struct SharedData {
    int number;
    char message[256];
};

bool is_composite(int n) {
    if (n < 2) return false; // Отрицательное, 0 или 1
    for (int i = 2; i <= sqrt(n); ++i) {
        if (n % i == 0) return true; // Составное число
    }
    return false; // Простое число
}

int main() {
    // Открытие разделяемой памяти
    int fd = shm_open("/my_shared_memory", O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "Ошибка при открытии разделяемой памяти" << std::endl;
        return 1;
    }

    // Отображение памяти
    size_t shared_size = sizeof(SharedData);
    SharedData* shared_data = (SharedData*)mmap(nullptr, shared_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_data == MAP_FAILED) {
        std::cerr << "Ошибка при отображении разделяемой памяти" << std::endl;
        return 1;
    }

    // Открытие семафоров
    sem_t* sem_parent = sem_open("/sem_parent", 0);
    sem_t* sem_child = sem_open("/sem_child", 0);
    if (sem_parent == SEM_FAILED || sem_child == SEM_FAILED) {
        std::cerr << "Ошибка при открытии семафоров" << std::endl;
        return 1;
    }

    // Ожидание числа от родительского процесса
    sem_wait(sem_child);

    int number = shared_data->number;
    if (number < 0) {
        strncpy(shared_data->message, "Число отрицательное", sizeof(shared_data->message));
    } else {
        // Проверка числа
        if (is_composite(number)) {
            // Запись в файл, если число составное
            int file = open("result.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file != -1) {
                dprintf(file, "%d\n", number);
                close(file);
                strncpy(shared_data->message, "Число составное, записано в файл", sizeof(shared_data->message));
            } else {
                strncpy(shared_data->message, "Ошибка записи в файл", sizeof(shared_data->message));
            }
        } else {
            strncpy(shared_data->message, "Число простое", sizeof(shared_data->message));
        }
    }

    sem_post(sem_parent); // Уведомляем родительский процесс

    // Завершаем работу
    munmap(shared_data, shared_size);
    return 0;
}
