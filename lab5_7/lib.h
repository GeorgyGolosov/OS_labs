#ifndef LIB_H
#define LIB_H

#include <iostream>
#include <list>
#include <unordered_set>
#include <chrono>
#include <ctime>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include "zmq.h"
#include <sys/select.h>
#include <map>
#include <vector>

// Функция для проверки наличия ввода в STDIN без блокировки
bool inputAvailable();

// Функция для получения текущего времени в формате time_t
std::time_t t_now();

// Функция для разбиения строки на токены по пробелам
std::vector<std::string> split(const std::string &s);

// Перечисление команд для обмена сообщениями между узлами
enum com : char
{
    None = 0,
    Create = 1,
    Ping = 2,
    ExecAdd = 3,
    ExecFnd = 4,
    ExecErr = 5
};

// Класс message для упаковки данных, передаваемых между узлами
class message
{
public:
    message() {}

    // Конструктор без строки (только числовые данные)
    message(com command, int id, int num)
        : command(command), id(id), num(num), sent_time(t_now())
    {
    }

    // Конструктор с дополнительной строкой (например, для передачи имени переменной)
    message(com command, int id, int num, char s[])
        : command(command), id(id), num(num), sent_time(t_now())
    {
        for (int i = 0; i < 30; ++i)
            st[i] = s[i];
    }

    bool operator==(const message &other) const
    {
        return command == other.command && id == other.id &&
               num == other.num && sent_time == other.sent_time;
    }

    com command;           // Тип команды
    int id;                // Идентификатор получателя (или другой контекстный id)
    int num;               // Числовой параметр (например, значение для записи)
    std::time_t sent_time; // Время отправки сообщения
    char st[30];           // Дополнительная строка (например, имя переменной)
};

// Класс Node, описывающий узел распределённой системы
class Node
{
public:
    int id;              // Идентификатор узла
    pid_t pid;           // Идентификатор процесса
    void *context;       // Контекст ZeroMQ
    void *socket;        // Сокет ZeroMQ
    std::string address; // Адрес (например, tcp://127.0.0.1:порт)

    bool operator==(const Node &other) const
    {
        return id == other.id && pid == other.pid;
    }
};

// Функции для создания и работы с узлами
Node createNode(int id, bool is_child);
Node createProcess(int id);
void send_mes(Node &node, message m);
message get_mes(Node &node);

#endif // LIB_H
