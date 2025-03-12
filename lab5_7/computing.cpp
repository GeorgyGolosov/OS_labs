#include "lib.h"
#include <iostream>
#include <map>
#include <chrono>
#include <thread>
#include <functional>

using namespace std::chrono;

Node* children_root = nullptr;

int main(int argc, char *argv[])
{
    // Создаем узел текущего процесса
    Node I = createNode(atoi(argv[1]), true);
    std::map<std::string, int> dict;

    // Переменные для heartbeat
    std::chrono::milliseconds local_heartbeat(0);
    auto last_beat = steady_clock::now();

    while (true)
    {
        // Периодическая отправка heartbeat-сообщения родительскому узлу
        if (local_heartbeat > std::chrono::milliseconds::zero() &&
            duration_cast<milliseconds>(steady_clock::now() - last_beat).count() > local_heartbeat.count())
        {
            message hb(HeartBeat, I.id, -1);
            send_mes(I, hb);
            last_beat = steady_clock::now();
        }

        // Обрабатываем сообщения от дочерних узлов
        traverseChildren(children_root, [&](Node& child) {
            message m = get_mes(child);
            if (m.command != None)
                send_mes(I, m);
        });

        // Проверяем сообщение от родителя
        message m = get_mes(I);
        switch (m.command)
        {
        case Create:
            if (m.id == I.id)
            {
                Node child = createProcess(m.num);
                Node* childPtr = new Node(child);
                children_root = insertChild(children_root, childPtr);
                send_mes(I, {Create, child.id, child.pid});
            }
            else
                traverseChildren(children_root, [&](Node& child) {
                    send_mes(child, m);
                });
            break;
        case Ping:
            if (m.id == I.id)
                send_mes(I, m);
            else
                traverseChildren(children_root, [&](Node& child) {
                    send_mes(child, m);
                });
            break;
        case ExecAdd:
            if (m.id == I.id)
            {
                dict[std::string(m.st)] = m.num;
                send_mes(I, m);
            }
            else
                traverseChildren(children_root, [&](Node& child) {
                    send_mes(child, m);
                });
            break;
        case ExecFnd:
            if (m.id == I.id)
            {
                if (dict.find(std::string(m.st)) != dict.end())
                    send_mes(I, {ExecFnd, I.id, dict[std::string(m.st)], m.st});
                else
                    send_mes(I, {ExecErr, I.id, -1, m.st});
            }
            else
                traverseChildren(children_root, [&](Node& child) {
                    send_mes(child, m);
                });
            break;
        case HeartBeat:
            // Обновляем локальный интервал heartbeat при получении команды от управляющего узла
            local_heartbeat = std::chrono::milliseconds(m.num);
            break;
        default:
            break;
        }
        usleep(1000000);
    }
    return 0;
}
