#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>


int main() {
    int fd1[2], fd2[2];

    if (pipe(fd1) < 0 || pipe(fd2) < 0) {
        std::cout << "Ошибка при создании pipe" << std::endl;
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cout << "Ошибка при создании дочернего процесса" << std::endl;
        return 1;
    }

    if (pid > 0) { // Родительский процесс
        close(fd1[0]); // Закрываем чтение из первого pipe
        close(fd2[1]); // Закрываем запись во второй pipe

        std::cout << "Введите данные в формате: «число<endline>»." << std::endl;

        int number;
        std::cin >> number;

        if (write(fd1[1], &number, sizeof(number)) == -1) {
            std::cout << "Ошибка при передаче числа";
            return 1;
        }

        // Ожидание завершения дочернего процесса
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 1) {
            std::cerr << "Число отрицательное, либо простое. Завершение работы." << std::endl;
            return 1;
        }
        close(fd1[1]); // Закрываем запись в первый pipe
        close(fd2[0]); // Закрываем чтение из второго pipe
    } else { // Дочерний процесс
        close(fd1[1]); // Закрываем запись в первый pipe
        close(fd2[0]); // Закрываем чтение из второго pipe

        // Перенаправление stdin и stdout на pipe
        if (dup2(fd1[0], STDIN_FILENO) == -1) {
            std::cout << "Ошибка при перенаправлении stdin";
            return 1;
        }
        if (dup2(fd2[1], STDOUT_FILENO) == -1) {
            std::cout << "Ошибка при перенаправлении stdout";
            return 1;
        }

        // Запуск дочернего процесса
        execl("./child", "child", NULL);
        std::cout << "Ошибка при вызове execl" << std::endl;
        return 1;
    }

    return 0;
}
