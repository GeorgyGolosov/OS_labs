#include "lib.h"
#include <fcntl.h>   // Для fcntl, O_NONBLOCK
#include <errno.h>
#include <signal.h>
#include <chrono>

// Функция проверки ввода
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

std::time_t t_now()
{
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

Node createNode(int id, bool is_child)
{
    Node node;
    node.id = id;
    node.pid = getpid();

    node.context = zmq_ctx_new();
    node.socket = zmq_socket(node.context, ZMQ_DEALER);

    node.address = "tcp://127.0.0.1:" + std::to_string(5555 + id);

    if (is_child)
        zmq_connect(node.socket, (node.address).c_str());
    else
        zmq_bind(node.socket, (node.address).c_str());

    return node;
}

Node createProcess(int id)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        // Дочерний процесс
        execl("./computing", "computing", std::to_string(id).c_str(), NULL);
        std::cerr << "execl failed" << std::endl;
        exit(1);
    }
    else if (pid == -1)
    {
        std::cerr << "Fork failed" << std::endl;
        exit(1);
    }
    Node node = createNode(id, false);
    node.pid = pid;
    return node;
}

void send_mes(Node &node, message m)
{
    zmq_msg_t request_message;
    zmq_msg_init_size(&request_message, sizeof(m));
    std::memcpy(zmq_msg_data(&request_message), &m, sizeof(m));
    zmq_msg_send(&request_message, node.socket, ZMQ_DONTWAIT);
}

message get_mes(Node &node)
{
    zmq_msg_t request;
    zmq_msg_init(&request);
    auto result = zmq_msg_recv(&request, node.socket, ZMQ_DONTWAIT);
    if (result == -1)
        return message(None, -1, -1);
    message m;
    std::memcpy(&m, zmq_msg_data(&request), sizeof(message));
    return m;
}

void traverseChildren(Node* root, const std::function<void(Node&)>& f) {
    if (!root)
        return;
    traverseChildren(root->left, f);
    f(*root);
    traverseChildren(root->right, f);
}

Node* insertChild(Node* root, Node* newChild) {
    if (root == nullptr)
        return newChild;
    if (newChild->id < root->id)
        root->left = insertChild(root->left, newChild);
    else if (newChild->id > root->id)
        root->right = insertChild(root->right, newChild);
    return root;
}


// Внутренние статические переменные для heartbeat
static std::chrono::milliseconds heartbeat_interval(0);
static std::map<int, std::chrono::steady_clock::time_point> beat_tracker;

// Вспомогательная функция, возвращающая текущее время (steady_clock)
std::chrono::steady_clock::time_point now() {
    return std::chrono::steady_clock::now();
}

// Обновление времени последнего heartbeat для узла
void update_beat(int node_id) {
    beat_tracker[node_id] = now();
}

// Функция обработки команды heartbeat:
// Читает новый интервал с консоли, сохраняет его и рассылает сообщение  детям.
void handle_heartbeat_command(Node* children_root) {
    int time;
    std::cin >> time;
    heartbeat_interval = std::chrono::milliseconds(time);
    // Сброс времени для всех узлов
    for (auto& kv : beat_tracker) {
        kv.second = now();
    }
    // Рассылка нового интервала детям
    message msg(HeartBeat, -1, time);
    traverseChildren(children_root, [&](Node& child) {
        send_mes(child, msg);
    });
}

// Функция проверки полученных heartbeat от узлов.
// Сообщения о недоступности выводятся не чаще, чем раз в 5 секунд.
void check_beats() {
    static auto last_print_time = now();
    auto current = now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(current - last_print_time).count();
    if (diff < 5000) {
        return;
    }
    last_print_time = current;
    for (auto& kv : beat_tracker) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current - kv.second).count();
        if (elapsed > 4 * heartbeat_interval.count()) {
            std::cout << "Node: " << kv.first << " is unavailable now" << std::endl;
            kv.second = current; // Сброс, чтобы избежать повторного вывода до следующего периода
        }
    }
}

Node* searchChild(Node* root, int id) {
    if (root == nullptr)
        return nullptr;
    if (root->id == id)
        return root;
    Node* found = searchChild(root->left, id);
    if (found)
        return found;
    return searchChild(root->right, id);
}
