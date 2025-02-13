#include "lib.h"
#include <sstream>
#include <sys/time.h>
#include <cstdlib>
#include <cstring>

// Функция inputAvailable – проверяет, есть ли данные в STDIN без блокировки
bool inputAvailable()
{
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    return (FD_ISSET(STDIN_FILENO, &fds));
}

// Функция t_now – возвращает текущее время в формате time_t
std::time_t t_now()
{
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

// Функция split – разбивает строку по пробельным символам и возвращает вектор токенов
std::vector<std::string> split(const std::string &s)
{
    std::istringstream iss(s);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token)
    {
        tokens.push_back(token);
    }
    return tokens;
}

// Функция createNode – создаёт объект Node и настраивает ZeroMQ-сокет
Node createNode(int id, bool is_child)
{
    Node node;
    node.id = id;
    node.pid = getpid();

    // Создание контекста и сокета ZeroMQ (тип DEALER)
    node.context = zmq_ctx_new();
    node.socket = zmq_socket(node.context, ZMQ_DEALER);

    // Формирование адреса: если использовать отдельное адресование, можно задать порт = 5555 + id
    // Однако для вычислительных узлов, которые подключаются к управляющему узлу, используется фиксированный адрес
    node.address = "tcp://localhost:5555";

    if (is_child)
        zmq_connect(node.socket, node.address.c_str());
    else
        zmq_bind(node.socket, node.address.c_str());

    return node;
}

// Функция createProcess – порождает новый процесс для вычислительного узла
Node createProcess(int id)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        // В дочернем процессе запускаем программу computing с параметром id
        execl("./computing", "computing", std::to_string(id).c_str(), NULL);
        std::cerr << "execl failed" << std::endl;
        exit(1);
    }
    else if (pid == -1)
    {
        std::cerr << "Fork failed" << std::endl;
        exit(1);
    }
    // В родительском процессе создаём Node с is_child = false
    Node node = createNode(id, false);
    node.pid = pid;
    return node;
}

// Функция send_mes – отправляет сообщение через сокет узла
void send_mes(Node &node, message m)
{
    zmq_msg_t request_message;
    zmq_msg_init_size(&request_message, sizeof(m));
    std::memcpy(zmq_msg_data(&request_message), &m, sizeof(m));
    zmq_msg_send(&request_message, node.socket, ZMQ_DONTWAIT);
    zmq_msg_close(&request_message);
}

// Функция get_mes – пытается получить сообщение из сокета узла в неблокирующем режиме
message get_mes(Node &node)
{
    zmq_msg_t request;
    zmq_msg_init(&request);
    auto result = zmq_msg_recv(&request, node.socket, ZMQ_DONTWAIT);
    if (result == -1)
    {
        zmq_msg_close(&request);
        return message(None, -1, -1);
    }
    message m;
    std::memcpy(&m, zmq_msg_data(&request), sizeof(message));
    zmq_msg_close(&request);
    return m;
}
