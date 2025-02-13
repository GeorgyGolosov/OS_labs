// control.cpp
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <map>
#include <set>
#include <condition_variable>
#include <vector>
#include <cstdlib>
#include <csignal>
#include <zmq.hpp>
#include "lib.h"

using namespace std;
using namespace std::chrono;

struct NodeInfo {
    int id;
    int parent; // -1, если родитель не указан
    pid_t pid;
    steady_clock::time_point lastHeartbeat;
};

std::mutex nodesMutex;
std::map<int, NodeInfo> nodes;
std::set<int> unavailableNodes;
int heartbeatInterval = 2000; // Интервал heartbeat (может задаваться командой heartbit)

// Потокобезопасная очередь команд пользователя
std::mutex queueMutex;
std::queue<std::string> commandQueue;
std::condition_variable queueCV;

// Поток для чтения команд с консоли
void inputThread() {
    std::string line;
    while (std::getline(std::cin, line)) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            commandQueue.push(line);
        }
        queueCV.notify_one();
    }
}

// Функция для порождения вычислительного узла
void spawnComputingNode(int nodeId, int parentId) {
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Error: Could not fork process" << std::endl;
    } else if (pid == 0) {
        // В дочернем процессе запускаем исполняемый файл computing
        std::string nodeIdStr = std::to_string(nodeId);
        std::string parentIdStr = (parentId != -1) ? std::to_string(parentId) : "";
        if (parentId != -1) {
            execl("./computing", "./computing", nodeIdStr.c_str(), parentIdStr.c_str(), (char*)NULL);
        } else {
            execl("./computing", "./computing", nodeIdStr.c_str(), (char*)NULL);
        }
        std::cerr << "Error: execl failed" << std::endl;
        exit(1);
    } else {
        // В родительском процессе сохраняем информацию об узле
        NodeInfo info;
        info.id = nodeId;
        info.parent = parentId;
        info.pid = pid;
        info.lastHeartbeat = steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(nodesMutex);
            nodes[nodeId] = info;
        }
        std::cout << "Ok: " << pid << std::endl;
    }
}

// Функция проверки доступности узла: если с момента последнего heartbeat прошло не более 4*heartbeatInterval мс.
bool isNodeAvailable(const NodeInfo& node) {
    auto now = steady_clock::now();
    auto elapsed = duration_cast<milliseconds>(now - node.lastHeartbeat).count();
    return elapsed <= 4 * heartbeatInterval;
}

int main() {
    zmq::context_t context(1);

    // ROUTER-сокет для отправки команд вычислительным узлам
    zmq::socket_t router(context, zmq::socket_type::router);
    router.bind("tcp://*:5555");

    // PULL-сокет для приёма heartbeat-сообщений от вычислительных узлов
    zmq::socket_t heartbeatSocket(context, zmq::socket_type::pull);
    heartbeatSocket.bind("tcp://*:5557");

    // Запускаем поток для чтения команд с консоли
    std::thread t(inputThread);

    // Основной цикл – опрос сокетов и обработка команд
    while (true) {
        // Опрос сокетов с таймаутом 100 мс
        zmq::pollitem_t items[] = {
            { static_cast<void*>(router), 0, ZMQ_POLLIN, 0 },
            { static_cast<void*>(heartbeatSocket), 0, ZMQ_POLLIN, 0 }
        };
        zmq::poll(items, 2, 100);

        // Обработка heartbeat-сообщений
        if (items[1].revents & ZMQ_POLLIN) {
            zmq::message_t hbMsg;
            heartbeatSocket.recv(hbMsg, zmq::recv_flags::none);
            std::string hb(static_cast<char*>(hbMsg.data()), hbMsg.size());
            // Ожидается формат: "HEARTBEAT <node_id>"
            auto tokens = split(hb);
            if (tokens.size() == 2 && tokens[0] == "HEARTBEAT") {
                int nodeId = std::stoi(tokens[1]);
                std::lock_guard<std::mutex> lock(nodesMutex);
                if (nodes.find(nodeId) != nodes.end()) {
                    nodes[nodeId].lastHeartbeat = steady_clock::now();
                    // Если ранее узел был помечен как недоступный – снимаем это состояние.
                    unavailableNodes.erase(nodeId);
                }
            }
        }

        // Обработка ответов от вычислительных узлов (через ROUTER-сокет)
        if (items[0].revents & ZMQ_POLLIN) {
            // При схеме ROUTER/DEALER сообщение состоит из нескольких частей:
            // [identity][empty][reply]
            zmq::message_t identity;
            router.recv(identity, zmq::recv_flags::none);
            zmq::message_t empty;
            router.recv(empty, zmq::recv_flags::none);
            zmq::message_t replyMsg;
            router.recv(replyMsg, zmq::recv_flags::none);
            std::string reply(static_cast<char*>(replyMsg.data()), replyMsg.size());
            std::cout << reply << std::endl;
        }

        // Если появились команды от пользователя – извлекаем их из очереди
        std::string cmd;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!commandQueue.empty()) {
                cmd = commandQueue.front();
                commandQueue.pop();
            }
        }
        if (!cmd.empty()) {
            auto tokens = split(cmd);
            if (tokens.empty()) continue;
            if (tokens[0] == "create") {
                // Формат: create id [parent]
                if (tokens.size() < 2) {
                    std::cout << "Error: Invalid create command" << std::endl;
                    continue;
                }
                int newId = std::stoi(tokens[1]);
                int parentId = -1;
                if (tokens.size() >= 3) {
                    parentId = std::stoi(tokens[2]);
                    std::lock_guard<std::mutex> lock(nodesMutex);
                    if (nodes.find(parentId) == nodes.end()) {
                        std::cout << "Error: Parent not found" << std::endl;
                        continue;
                    }
                    if (!isNodeAvailable(nodes[parentId])) {
                        std::cout << "Error: Parent is unavailable" << std::endl;
                        continue;
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(nodesMutex);
                    if (nodes.find(newId) != nodes.end()) {
                        std::cout << "Error: Already exists" << std::endl;
                        continue;
                    }
                }
                spawnComputingNode(newId, parentId);
            }
            else if (tokens[0] == "exec") {
                // Формат: exec id name [value]
                if (tokens.size() < 3) {
                    std::cout << "Error: Invalid exec command" << std::endl;
                    continue;
                }
                int targetId = std::stoi(tokens[1]);
                {
                    std::lock_guard<std::mutex> lock(nodesMutex);
                    if (nodes.find(targetId) == nodes.end()) {
                        std::cout << "Error:" << targetId << ": Not found" << std::endl;
                        continue;
                    }
                    if (!isNodeAvailable(nodes[targetId])) {
                        std::cout << "Error:" << targetId << ": Node is unavailable" << std::endl;
                        continue;
                    }
                }
                // Для отправки команды вычислительному узлу убираем из команды "exec" и id узла.
                std::string nodeCommand;
                for (size_t i = 2; i < tokens.size(); ++i) {
                    nodeCommand += tokens[i] + (i != tokens.size()-1 ? " " : "");
                }
                // Отправляем сообщение через ROUTER-сокет:
                // Первая часть – identity (id узла как строка)
                std::string targetIdStr = std::to_string(targetId);
                zmq::message_t identityMsg(targetIdStr.size());
                memcpy(identityMsg.data(), targetIdStr.c_str(), targetIdStr.size());
                router.send(identityMsg, zmq::send_flags::sndmore);
                // Пустая часть
                zmq::message_t emptyMsg(0);
                router.send(emptyMsg, zmq::send_flags::sndmore);
                // Третья часть – команда
                zmq::message_t commandMsg(nodeCommand.size());
                memcpy(commandMsg.data(), nodeCommand.c_str(), nodeCommand.size());
                router.send(commandMsg, zmq::send_flags::none);
            }
            else if (tokens[0] == "heartbit") {
                // Команда для задания интервала heartbeat: heartbit time
                if (tokens.size() < 2) {
                    std::cout << "Error: Invalid heartbit command" << std::endl;
                    continue;
                }
                heartbeatInterval = std::stoi(tokens[1]);
                std::cout << "Ok" << std::endl;
            }
            else if (tokens[0] == "ping") {
                // Формат: ping id
                if (tokens.size() < 2) {
                    std::cout << "Error: Invalid ping command" << std::endl;
                    continue;
                }
                int pingId = std::stoi(tokens[1]);
                bool available = false;
                {
                    std::lock_guard<std::mutex> lock(nodesMutex);
                    if (nodes.find(pingId) != nodes.end())
                        available = isNodeAvailable(nodes[pingId]);
                }
                std::cout << "Ok: " << (available ? "1" : "0") << std::endl;
            }
            else {
                std::cout << "Error: Unknown command" << std::endl;
            }
        }

        // Периодическая проверка доступности узлов – если с узла не поступали heartbeat дольше 4*heartbeatInterval, выводим сообщение.
        {
            std::lock_guard<std::mutex> lock(nodesMutex);
            auto now = steady_clock::now();
            for (auto& pair : nodes) {
                int nodeId = pair.first;
                auto& node = pair.second;
                auto elapsed = duration_cast<milliseconds>(now - node.lastHeartbeat).count();
                if (elapsed > 4 * heartbeatInterval) {
                    if (unavailableNodes.find(nodeId) == unavailableNodes.end()) {
                        std::cout << "Heartbit: node " << nodeId << " is unavailable now" << std::endl;
                        unavailableNodes.insert(nodeId);
                    }
                }
            }
        }
    }

    t.join();
    return 0;
}
