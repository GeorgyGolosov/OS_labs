#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <map>
#include <mutex>
#include <cstring>
#include "zmq.h"
#include "lib.h"

using namespace std;

std::map<std::string, int> localDict;
std::mutex dictMutex;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <node_id> [parent_id]" << std::endl;
        return 1;
    }
    string node_id = argv[1];

    // Создаем контекст и DEALER-сокет через C API
    void* context = zmq_ctx_new();
    void* dealer = zmq_socket(context, ZMQ_DEALER);
    // Устанавливаем identity (идентификатор узла) для возможности маршрутизации
    zmq_setsockopt(dealer, ZMQ_IDENTITY, node_id.c_str(), node_id.size());
    zmq_connect(dealer, "tcp://localhost:5555");

    // Создаем PUSH-сокет для отправки heartbeat-сообщений
    void* heartbeatSocket = zmq_socket(context, ZMQ_PUSH);
    zmq_connect(heartbeatSocket, "tcp://localhost:5557");

    int heartbeatInterval = 2000; // Интервал heartbeat в мс

    // Поток отправки heartbeat-сообщений
    std::thread heartbeatThread([&]() {
        while (true) {
            string hb = "HEARTBEAT " + node_id;
            zmq_msg_t msg;
            zmq_msg_init_size(&msg, hb.size());
            memcpy(zmq_msg_data(&msg), hb.c_str(), hb.size());
            zmq_msg_send(&msg, heartbeatSocket, 0);
            zmq_msg_close(&msg);
            std::this_thread::sleep_for(std::chrono::milliseconds(heartbeatInterval));
        }
    });

    // Основной цикл обработки входящих команд
    while (true) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        int res = zmq_msg_recv(&msg, dealer, 0);
        if (res == -1) {
            zmq_msg_close(&msg);
            continue;
        }
        // Если первый фрейм пустой (делимитер), пропускаем его и читаем следующий
        if (zmq_msg_size(&msg) == 0) {
            zmq_msg_close(&msg);
            zmq_msg_init(&msg);
            res = zmq_msg_recv(&msg, dealer, 0);
            if (res == -1) {
                zmq_msg_close(&msg);
                continue;
            }
        }
        string command((char*)zmq_msg_data(&msg), zmq_msg_size(&msg));
        zmq_msg_close(&msg);

        auto tokens = split(command);
        string reply;
        if (tokens.size() == 1) {  // Запрос на чтение: exec id name
            string key = tokens[0];
            lock_guard<mutex> lock(dictMutex);
            auto it = localDict.find(key);
            if (it != localDict.end())
                reply = "Ok:" + node_id + ": " + to_string(it->second);
            else
                reply = "Ok:" + node_id + ": '" + key + "' not found";
        }
        else if (tokens.size() == 2) {  // Запись: exec id name value
            string key = tokens[0];
            int value = stoi(tokens[1]);
            {
                lock_guard<mutex> lock(dictMutex);
                localDict[key] = value;
            }
            reply = "Ok:" + node_id;
        } else {
            reply = "Error:" + node_id + ": Invalid command format";
        }

        // Отправляем ответ управляющему узлу в виде мультифреймового сообщения:
        // первый фрейм – пустой делимитер, второй – данные ответа.
        zmq_msg_t emptyMsg;
        zmq_msg_init_size(&emptyMsg, 0);
        zmq_msg_send(&emptyMsg, dealer, ZMQ_SNDMORE);
        zmq_msg_close(&emptyMsg);

        zmq_msg_t replyMsg;
        zmq_msg_init_size(&replyMsg, reply.size());
        memcpy(zmq_msg_data(&replyMsg), reply.c_str(), reply.size());
        zmq_msg_send(&replyMsg, dealer, 0);
        zmq_msg_close(&replyMsg);
    }

    heartbeatThread.join(); // На самом деле выполнение сюда не доходит
    zmq_close(dealer);
    zmq_close(heartbeatSocket);
    zmq_ctx_destroy(context);
    return 0;
}
