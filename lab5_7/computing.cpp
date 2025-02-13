// computing.cpp
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <map>
#include <mutex>
#include <zmq.hpp>
#include "lib.h"

using namespace std;

// Локальный целочисленный словарь вычислительного узла
std::map<std::string, int> localDict;
std::mutex dictMutex;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <node_id> [parent_id]" << std::endl;
        return 1;
    }
    // Получаем id узла (параметр parent пока не используется в логике вычислительного узла)
    std::string node_id = argv[1];

    // Создаём ZeroMQ контекст
    zmq::context_t context(1);

    // Создаём DEALER-сокет для приёма команд от управляющего узла.
    // Устанавливаем identity равным node_id, чтобы управляющий узел мог адресовать этот узел.
    zmq::socket_t dealer(context, zmq::socket_type::dealer);
    dealer.set(zmq::sockopt::routing_id, node_id);
    dealer.connect("tcp://localhost:5555"); // Подключаемся к ROUTER-сокету управляющего узла

    // Создаём PUSH-сокет для отправки heartbeat-сообщений.
    zmq::socket_t heartbeatSocket(context, zmq::socket_type::push);
    heartbeatSocket.connect("tcp://localhost:5557");

    // Интервал heartbeat в миллисекундах (по умолчанию 2000)
    int heartbeatInterval = 2000;

    // Отдельный поток для отправки heartbeat каждые heartbeatInterval мс.
    std::thread heartbeatThread([&]() {
        while (true) {
            // Формируем сообщение: "HEARTBEAT <node_id>"
            std::string hb = "HEARTBEAT " + node_id;
            zmq::message_t message(hb.size());
            memcpy(message.data(), hb.c_str(), hb.size());
            try {
                heartbeatSocket.send(message, zmq::send_flags::none);
            } catch (const zmq::error_t &e) {
                // Обработка ошибок отправки (при необходимости)
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(heartbeatInterval));
        }
    });

    // Основной цикл обработки входящих команд.
    while (true) {
        zmq::message_t msg;
        // Получаем первый фрейм
        auto res = dealer.recv(msg, zmq::recv_flags::none);
        if (!res)
            continue;

        // Если фрейм пустой, получаем следующий
        if (msg.size() == 0) {
            auto res = dealer.recv(msg, zmq::recv_flags::none);
            (void)res; // явное игнорирование результата
        }

        std::string command(static_cast<char*>(msg.data()), msg.size());
        auto tokens = split(command);
        std::string reply;

        if (tokens.size() == 1) { // запрос на чтение: exec id name
            std::string key = tokens[0];
            std::lock_guard<std::mutex> lock(dictMutex);
            auto it = localDict.find(key);
            if (it != localDict.end()) {
                reply = "Ok:" + node_id + ": " + std::to_string(it->second);
            } else {
                reply = "Ok:" + node_id + ": '" + key + "' not found";
            }
        }
        else if (tokens.size() == 2) { // запись: exec id name value
            std::string key = tokens[0];
            int value = std::stoi(tokens[1]);
            {
                std::lock_guard<std::mutex> lock(dictMutex);
                localDict[key] = value;
            }
            reply = "Ok:" + node_id;
        }
        else {
            reply = "Error:" + node_id + ": Invalid command format";
        }

        // Отправляем ответ управляющему узлу
        // Отправляем пустой фрейм-делимитер:
        zmq::message_t emptyMsg(0);
        dealer.send(emptyMsg, zmq::send_flags::sndmore);
        // Отправляем фрейм с ответом:
        zmq::message_t replyMsg(reply.size());
        memcpy(replyMsg.data(), reply.c_str(), reply.size());
        dealer.send(replyMsg, zmq::send_flags::none);
    }

    heartbeatThread.join(); // Этот вызов никогда не будет достигнут
    return 0;
}
