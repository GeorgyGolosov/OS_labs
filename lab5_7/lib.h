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
#include <functional>

bool inputAvailable();
std::time_t t_now();

enum com : char {
    None = 0,
    Create = 1,
    Ping = 2,
    ExecAdd = 3,
    ExecFnd = 4,
    ExecErr = 5,
    HeartBeat = 6  // Новый тип сообщения для heartbeat
};

class message {
public:
    message() {}
    message(com command, int id, int num)
        : command(command), id(id), num(num), sent_time(t_now()) {}
    message(com command, int id, int num, char s[])
        : command(command), id(id), num(num), sent_time(t_now()) {
        strncpy(st, s, 30);
    }

    bool operator==(const message &other) const {
        return command == other.command && id == other.id && num == other.num;
    }

    com command;
    int id;
    int num;
    std::time_t sent_time;
    char st[30];
};

class Node {
public:
    int id;
    pid_t pid;
    void* context;
    void* socket;
    std::string address;

    Node* left = nullptr;  // Левый потомок (меньшие id)
    Node* right = nullptr; // Правый потомок (большие id)

    int beat_counter = 0;
};

Node createNode(int id, bool is_child);
Node createProcess(int id);
Node* insertChild(Node* root, Node* newChild);
void send_mes(Node &node, message m);
message get_mes(Node &node);
void traverseChildren(Node* root, const std::function<void(Node&)>& f);
Node* searchChild(Node* root, int id);

// Функции для heartbeat
void handle_heartbeat_command(Node* children_root);
void check_beats();
void update_beat(int node_id);
