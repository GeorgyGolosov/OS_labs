#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <map>
#include <set>
#include <vector>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include "zmq.h"
#include "lib.h"

using namespace std;
using namespace std::chrono;

struct NodeInfo {
    int id;
    int parent; // -1, если родитель не указан
    pid_t pid;
    steady_clock::time_point lastHeartbeat;
};

mutex nodesMutex;
map<int, NodeInfo> nodes;
set<int> unavailableNodes;
int heartbeatInterval = 2000; // Интервал heartbeat (может задаваться командой heartbit)

mutex queueMutex;
queue<string> commandQueue;

// Поток для чтения команд с консоли
void inputThread() {
    string line;
    while (getline(cin, line)) {
        lock_guard<mutex> lock(queueMutex);
        commandQueue.push(line);
    }
}

void spawnComputingNode(int nodeId, int parentId) {
    pid_t pid = fork();
    if (pid < 0) {
        cerr << "Error: Could not fork process" << endl;
    } else if (pid == 0) {
        // В дочернем процессе запускаем вычислительный узел
        string nodeIdStr = to_string(nodeId);
        string parentIdStr = (parentId != -1) ? to_string(parentId) : "";
        if (parentId != -1)
            execl("./computing", "computing", nodeIdStr.c_str(), parentIdStr.c_str(), (char*)NULL);
        else
            execl("./computing", "computing", nodeIdStr.c_str(), (char*)NULL);
        cerr << "Error: execl failed" << endl;
        exit(1);
    } else {
        // В родительском процессе сохраняем информацию об узле
        NodeInfo info;
        info.id = nodeId;
        info.parent = parentId;
        info.pid = pid;
        info.lastHeartbeat = steady_clock::now();
        {
            lock_guard<mutex> lock(nodesMutex);
            nodes[nodeId] = info;
        }
        cout << "Ok: " << pid << endl;
    }
}

bool isNodeAvailable(const NodeInfo &node) {
    auto now = steady_clock::now();
    auto elapsed = duration_cast<milliseconds>(now - node.lastHeartbeat).count();
    return elapsed <= 4 * heartbeatInterval;
}

int main() {
    void* context = zmq_ctx_new();

    // Создаем ROUTER-сокет для отправки команд вычислительным узлам
    void* router = zmq_socket(context, ZMQ_ROUTER);
    zmq_bind(router, "tcp://*:5555");

    // Создаем PULL-сокет для приема heartbeat-сообщений от вычислительных узлов
    void* heartbeatSocket = zmq_socket(context, ZMQ_PULL);
    zmq_bind(heartbeatSocket, "tcp://*:5557");

    // Запускаем поток для чтения команд с консоли
    thread t(inputThread);

    zmq_pollitem_t items[2];
    items[0].socket = router;
    items[0].fd = 0;
    items[0].events = ZMQ_POLLIN;
    items[0].revents = 0;
    items[1].socket = heartbeatSocket;
    items[1].fd = 0;
    items[1].events = ZMQ_POLLIN;
    items[1].revents = 0;

    while (true) {
        zmq_poll(items, 2, 100);  // Таймаут 100 мс

        // Обработка heartbeat-сообщений
        if (items[1].revents & ZMQ_POLLIN) {
            zmq_msg_t hbMsg;
            zmq_msg_init(&hbMsg);
            int r = zmq_msg_recv(&hbMsg, heartbeatSocket, 0);
            if (r != -1) {
                string hb((char*)zmq_msg_data(&hbMsg), zmq_msg_size(&hbMsg));
                auto tokens = split(hb);
                if (tokens.size() == 2 && tokens[0] == "HEARTBEAT") {
                    int nodeId = stoi(tokens[1]);
                    lock_guard<mutex> lock(nodesMutex);
                    if (nodes.find(nodeId) != nodes.end()) {
                        nodes[nodeId].lastHeartbeat = steady_clock::now();
                        unavailableNodes.erase(nodeId);
                    }
                }
            }
            zmq_msg_close(&hbMsg);
        }

        // Обработка ответов от вычислительных узлов (через ROUTER-сокет)
        if (items[0].revents & ZMQ_POLLIN) {
            // Ожидаем multipart-сообщение: [identity][empty][reply]
            zmq_msg_t identity, empty, replyMsg;
            zmq_msg_init(&identity);
            zmq_msg_recv(&identity, router, 0);
            zmq_msg_init(&empty);
            zmq_msg_recv(&empty, router, 0);
            zmq_msg_init(&replyMsg);
            zmq_msg_recv(&replyMsg, router, 0);
            string reply((char*)zmq_msg_data(&replyMsg), zmq_msg_size(&replyMsg));
            cout << reply << endl;
            zmq_msg_close(&identity);
            zmq_msg_close(&empty);
            zmq_msg_close(&replyMsg);
        }

        // Проверяем очередь команд
        string cmd;
        {
            lock_guard<mutex> lock(queueMutex);
            if (!commandQueue.empty()) {
                cmd = commandQueue.front();
                commandQueue.pop();
            }
        }
        if (!cmd.empty()) {
            auto tokens = split(cmd);
            if (tokens.empty())
                continue;
            if (tokens[0] == "create") {
                // Формат: create id [parent]
                if (tokens.size() < 2) {
                    cout << "Error: Invalid create command" << endl;
                    continue;
                }
                int newId = stoi(tokens[1]);
                int parentId = -1;
                if (tokens.size() >= 3) {
                    parentId = stoi(tokens[2]);
                    lock_guard<mutex> lock(nodesMutex);
                    if (nodes.find(parentId) == nodes.end()) {
                        cout << "Error: Parent not found" << endl;
                        continue;
                    }
                    if (!isNodeAvailable(nodes[parentId])) {
                        cout << "Error: Parent is unavailable" << endl;
                        continue;
                    }
                }
                {
                    lock_guard<mutex> lock(nodesMutex);
                    if (nodes.find(newId) != nodes.end()) {
                        cout << "Error: Already exists" << endl;
                        continue;
                    }
                }
                spawnComputingNode(newId, parentId);
            }
            else if (tokens[0] == "exec") {
                // Формат: exec id name [value]
                if (tokens.size() < 3) {
                    cout << "Error: Invalid exec command" << endl;
                    continue;
                }
                int targetId = stoi(tokens[1]);
                {
                    lock_guard<mutex> lock(nodesMutex);
                    if (nodes.find(targetId) == nodes.end()) {
                        cout << "Error:" << targetId << ": Not found" << endl;
                        continue;
                    }
                    if (!isNodeAvailable(nodes[targetId])) {
                        cout << "Error:" << targetId << ": Node is unavailable" << endl;
                        continue;
                    }
                }
                string nodeCommand;
                for (size_t i = 2; i < tokens.size(); ++i)
                    nodeCommand += tokens[i] + (i != tokens.size() - 1 ? " " : "");
                // Отправляем сообщение через ROUTER-сокет:
                // Формат: [identity][empty][command]
                string targetIdStr = to_string(targetId);
                zmq_msg_t identityMsg;
                zmq_msg_init_size(&identityMsg, targetIdStr.size());
                memcpy(zmq_msg_data(&identityMsg), targetIdStr.c_str(), targetIdStr.size());
                zmq_msg_send(&identityMsg, router, ZMQ_SNDMORE);
                zmq_msg_close(&identityMsg);

                zmq_msg_t emptyMsg;
                zmq_msg_init_size(&emptyMsg, 0);
                zmq_msg_send(&emptyMsg, router, ZMQ_SNDMORE);
                zmq_msg_close(&emptyMsg);

                zmq_msg_t commandMsg;
                zmq_msg_init_size(&commandMsg, nodeCommand.size());
                memcpy(zmq_msg_data(&commandMsg), nodeCommand.c_str(), nodeCommand.size());
                zmq_msg_send(&commandMsg, router, 0);
                zmq_msg_close(&commandMsg);
            }
            else if (tokens[0] == "heartbit") {
                // Формат: heartbit time
                if (tokens.size() < 2) {
                    cout << "Error: Invalid heartbit command" << endl;
                    continue;
                }
                heartbeatInterval = stoi(tokens[1]);
                cout << "Ok" << endl;
            }
            else if (tokens[0] == "ping") {
                // Формат: ping id
                if (tokens.size() < 2) {
                    cout << "Error: Invalid ping command" << endl;
                    continue;
                }
                int pingId = stoi(tokens[1]);
                bool available = false;
                {
                    lock_guard<mutex> lock(nodesMutex);
                    if (nodes.find(pingId) != nodes.end())
                        available = isNodeAvailable(nodes[pingId]);
                }
                cout << "Ok: " << (available ? "1" : "0") << endl;
            }
            else {
                cout << "Error: Unknown command" << endl;
            }
        }

        // Периодическая проверка доступности узлов
        {
            lock_guard<mutex> lock(nodesMutex);
            auto now = steady_clock::now();
            for (auto &pair : nodes) {
                int nodeId = pair.first;
                auto &node = pair.second;
                auto elapsed = duration_cast<milliseconds>(now - node.lastHeartbeat).count();
                if (elapsed > 4 * heartbeatInterval) {
                    if (unavailableNodes.find(nodeId) == unavailableNodes.end()) {
                        cout << "Heartbit: node " << nodeId << " is unavailable now" << endl;
                        unavailableNodes.insert(nodeId);
                    }
                }
            }
        }

        this_thread::sleep_for(chrono::milliseconds(50));
    }

    t.join();
    zmq_close(router);
    zmq_close(heartbeatSocket);
    zmq_ctx_destroy(context);
    return 0;
}
