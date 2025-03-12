#include "lib.h"
#include <cstdio>      // для fgets, sscanf
#include <fcntl.h>     // для fcntl, O_NONBLOCK
#include <unistd.h>
#include <errno.h>
#include <string>

Node* children_root = nullptr;

int main()
{
    std::unordered_set<int> all_id;
    // Управляющий узел имеет id -1
    all_id.insert(-1);
    std::list<message> saved_mes;
    std::string command;

    while (true)
    {
        // Обрабатываем входящие сообщения от прямых детей
        traverseChildren(children_root, [&](Node& child) {
            message m = get_mes(child);
            switch (m.command)
            {
            case Create:
                all_id.insert(m.num);
                std::cout << "Ok: " << m.num << std::endl;
                for (auto it = saved_mes.begin(); it != saved_mes.end(); ++it)
                {
                    if (it->command == Create && it->num == m.num)
                    {
                        saved_mes.erase(it);
                        break;
                    }
                }
                break;
            case Ping:
                std::cout << "Ok: " << m.id << " is available" << std::endl;
                for (auto it = saved_mes.begin(); it != saved_mes.end(); ++it)
                {
                    if (it->command == Ping && it->id == m.id)
                    {
                        saved_mes.erase(it);
                        break;
                    }
                }
                break;
            case ExecErr:
                std::cout << "Ok: " << m.id << " '" << m.st << "' not found" << std::endl;
                for (auto it = saved_mes.begin(); it != saved_mes.end(); ++it)
                {
                    if (it->command == ExecFnd && it->id == m.id)
                    {
                        saved_mes.erase(it);
                        break;
                    }
                }
                break;
            case ExecAdd:
                std::cout << "Ok: " << m.id << std::endl;
                for (auto it = saved_mes.begin(); it != saved_mes.end(); ++it)
                {
                    if (it->command == ExecAdd && it->id == m.id)
                    {
                        saved_mes.erase(it);
                        break;
                    }
                }
                break;
            case ExecFnd:
                std::cout << "Ok: " << m.id << " '" << m.st << "' " << m.num << std::endl;
                for (auto it = saved_mes.begin(); it != saved_mes.end(); ++it)
                {
                    if (it->command == ExecFnd && it->id == m.id)
                    {
                        saved_mes.erase(it);
                        break;
                    }
                }
                break;
            case HeartBeat:
                update_beat(m.id);
                break;
            default:
                break;
            }
        });

        // Проверяем недошедшие сообщения
        for (auto it = saved_mes.begin(); it != saved_mes.end(); ++it)
        {
            if (std::difftime(t_now(), it->sent_time) > 5)
            {
                switch (it->command)
                {
                case Ping:
                    std::cout << "Error:" << it->id << " is unavailable" << std::endl;
                    break;
                case Create:
                    std::cout << "Error: Parent " << it->id << " is unavailable" << std::endl;
                    break;
                case ExecAdd:
                case ExecFnd:
                    std::cout << "Error: Node " << it->id << " is unavailable" << std::endl;
                    break;
                default:
                    break;
                }
                saved_mes.erase(it);
                break;
            }
        }

        // Проверяем heartbeat – сообщения о недоступности выводятся не чаще, чем раз в 5 секунд
        check_beats();

        // Обрабатываем команды ввода
        if (!inputAvailable())
            continue;
        std::cin >> command;
        if (command == "create")
        {
            // Читаем оставшуюся часть строки
            char input_line[100];
            fgets(input_line, sizeof(input_line), stdin);
            int child_id, parent_id = -1;
            int count = sscanf(input_line, "%d %d", &child_id, &parent_id);
            if (count < 1)
            {
                std::cout << "Error: Missing child id" << std::endl;
                continue;
            }
            if (count == 1 || (count == 2 && parent_id == -1))
            {
                if (all_id.count(child_id))
                {
                    std::cout << "Error: Node with id " << child_id << " already exists" << std::endl;
                }
                else
                {
                    Node child = createProcess(child_id);
                    Node* childPtr = new Node(child);
                    children_root = insertChild(children_root, childPtr);
                    all_id.insert(child_id);
                    std::cout << "Ok: " << child.pid << std::endl;
                }
            }
            else
            {
                message m(Create, parent_id, child_id);
                saved_mes.push_back(m);
                Node* parentNode = searchChild(children_root, parent_id);
                if (parentNode)
                    send_mes(*parentNode, m);
                else
                    std::cout << "Error: Parent with id " << parent_id << " not found" << std::endl;
            }
        }
        else if (command == "exec")
        {
            char input_line[100];
            fgets(input_line, sizeof(input_line), stdin);
            int id, val;
            char key[30];
            if (sscanf(input_line, "%d %30s %d", &id, key, &val) == 3)
            {
                if (!all_id.count(id))
                {
                    std::cout << "Error: Node with id " << id << " doesn't exist" << std::endl;
                    continue;
                }
                message m = {ExecAdd, id, val, key};
                saved_mes.push_back(m);
                Node* target = searchChild(children_root, id);
                if (target)
                    send_mes(*target, m);
                else
                    std::cout << "Error: Node with id " << id << " is not directly accessible" << std::endl;
            }
            else if (sscanf(input_line, "%d %30s", &id, key) == 2)
            {
                if (!all_id.count(id))
                {
                    std::cout << "Error: Node with id " << id << " doesn't exist" << std::endl;
                    continue;
                }
                message m = {ExecFnd, id, -1, key};
                saved_mes.push_back(m);
                Node* target = searchChild(children_root, id);
                if (target)
                    send_mes(*target, m);
                else
                    std::cout << "Error: Node with id " << id << " is not directly accessible" << std::endl;
            }
        }
        else if (command == "ping")
        {
            int id;
            std::cin >> id;
            if (!all_id.count(id))
            {
                std::cout << "Error: Node with id " << id << " doesn't exist" << std::endl;
            }
            else
            {
                message m(Ping, id, 0);
                saved_mes.push_back(m);
                Node* target = searchChild(children_root, id);
                if (target)
                    send_mes(*target, m);
                else
                    std::cout << "Error: Node with id " << id << " is not directly accessible" << std::endl;
            }
        }
        else if (command == "heartbeat")
        {
            // Устанавливаем новый интервал heartbeat и рассылаем его всем прямым детям
            handle_heartbeat_command(children_root);
        }
        else
            std::cout << "Error: Command doesn't exist!" << std::endl;
        usleep(1000000);
    }
    return 0;
}
