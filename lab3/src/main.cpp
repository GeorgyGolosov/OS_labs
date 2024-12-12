#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>

struct SharedData {
    int number;
    char message[256];
};

int main() {
    // Размер общей памяти
    size_t shared_size = sizeof(SharedData);

    // Создание разделяемой памяти
    int fd = shm_open("/my_shared_memory", O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "Ошибка при создании разделяемой памяти" << std::endl;
        return 1;
    }
    ftruncate(fd, shared_size);

    // Отображение памяти
    SharedData* shared_data = (SharedData*)mmap(nullptr, shared_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_data == MAP_FAILED) {
        std::cerr << "Ошибка при отображении разделяемой памяти" << std::endl;
        return 1;
    }

    // Создание семафоров
    sem_t* sem_parent = sem_open("/sem_parent", O_CREAT, 0666, 0);
    sem_t* sem_child = sem_open("/sem_child", O_CREAT, 0666, 0);
    if (sem_parent == SEM_FAILED || sem_child == SEM_FAILED) {
        std::cerr << "Ошибка при создании семафоров" << std::endl;
        return 1;
    }

    // Создание дочернего процесса
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Ошибка при создании дочернего процесса" << std::endl;
        return 1;
    }

    if (pid > 0) { // Родительский процесс
        std::cout << "Введите число: ";
        int number;
        std::cin >> number;

        // Запись числа в разделяемую память
        shared_data->number = number;
        sem_post(sem_child); // Уведомляем дочерний процесс

        // Ожидание результата от дочернего процесса
        sem_wait(sem_parent);
        std::cout << "Результат: " << shared_data->message << std::endl;

        // Завершаем работу
        waitpid(pid, nullptr, 0);

        // Удаление ресурсов
        munmap(shared_data, shared_size);
        shm_unlink("/my_shared_memory");
        sem_close(sem_parent);
        sem_close(sem_child);
        sem_unlink("/sem_parent");
        sem_unlink("/sem_child");
    } else { // Дочерний процесс
        execl("./child", "child", nullptr);
        std::cerr << "Ошибка при вызове дочернего процесса" << std::endl;
        return 1;
    }

    return 0;
}
