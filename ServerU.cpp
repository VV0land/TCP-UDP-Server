//24.11.25
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

using namespace std;

const int BUFFER_SIZE = 1024;
const int MAX_CLIENTS = 100;
const int EPOLL_MAX_EVENTS = 64;

// Порты для сервера
const int TCP_PORT = 50000;
const int UDP_PORT = 60000;

int total_connections = 0;        // Общее число подключений за всё время

struct ClientData {
    int fd;
    sockaddr_in addr;
};

vector<ClientData> clients;
int epoll_fd;

// Установка неблокирующего режима
bool setNonBlocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL)");
        return false;
    }
    flags |= O_NONBLOCK;
    if (fcntl(sock, F_SETFL, flags) == -1) {
        perror("fcntl(F_SETFL)");
        return false;
    }
    return true;
}

// Создание UDP‑сервера
int createUdpServer(int port) {
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1) {
        perror("socket() UDP failed");
        return -1;
    }

    if (!setNonBlocking(server_fd)) {
        close(server_fd);
        return -1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind() UDP failed");
        close(server_fd);
        return -1;
    }

    cout << "UDP server started on port " << port << endl;
    return server_fd;
}

// Создание TCP‑сервера
int createTcpServer(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket() TCP failed");
        return -1;
    }

    if (!setNonBlocking(server_fd)) {
        close(server_fd);
        return -1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind() TCP failed");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen() TCP failed");
        close(server_fd);
        return -1;
    }

    cout << "TCP server started on port " << port << endl;
    return server_fd;
}

// Добавление сокета в epoll
/*bool addToEpoll(int fd) {
    epoll_event event;
    //event.events = EPOLLIN | EPOLLET;  // edge‑triggered
    event.events = EPOLLIN | EPOLLET | EPOLLOUT;  // Добавляем EPOLLOUT
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        perror("epoll_ctl ADD failed");
        return false;
    }
    return true;
}*/

bool addToEpoll(int epoll_fd, int fd, uint32_t events) {
    struct epoll_event event;
    event.events = events;  // EPOLLIN | EPOLLET (без EPOLLOUT для серверных)
    event.data.fd = fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        perror("epoll_ctl ADD failed");
        return false;
    }
    return true;
}

// Парсер команд новый
string processCommand(const string& cmd) {
    if (cmd == "/time") {
        // Возвращаем текущее время в формате YYYY-MM-DD HH:MM:SS
        time_t now = time(nullptr);
        tm timeinfo;
        localtime_r(&now, &timeinfo);
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return string(buffer) + "\r\n";
    }
    else if (cmd == "/stats") {
        // Собираем статистику:
        // - total_connections: сколько клиентов подключалось за всё время
        // - active_clients: сколько сейчас онлайн (размер вектора clients)
        int active_clients = clients.size();
        return "Total connections: " + to_string(total_connections) +
            ", Active clients: " + to_string(active_clients) + "\r\n";
    }
    else if (cmd == "/shutdown") {
        cout << "Shutdown command received. Server will terminate.\n";
        exit(0);
    }
    else {
        // Неизвестная команда
        return "Unknown command: " + cmd + "\r\n";
    }
}

// Функция принятия нового TCP‑подключения
/*void acceptTcpConnection(int server_fd) {
    struct sockaddr_in client_addr {};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

    if (client_fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept() failed");
        }
        return;
    }

    // Проверяем лимит клиентов
    if (clients.size() >= MAX_CLIENTS) {
        cerr << "Max clients limit reached. Rejecting new connection.\n";
        close(client_fd);
        return;
    }

    // Устанавливаем неблокирующий режим для клиентского сокета
    if (!setNonBlocking(client_fd)) {
        close(client_fd);
        return;
    }

    // Добавляем клиента в список активных
    clients.push_back({ client_fd, client_addr });
    addToEpoll(client_fd);




    // После clients.push_back(...) и addToEpoll(...)
    const char* welcome = "Server ready. Type your message:\r\n";
    if (send(client_fd, welcome, strlen(welcome), 0) == -1) {
        perror("send welcome failed");
    }
    else {
        cout << "Sent welcome message to client " << client_fd << endl;
    }




    // Увеличиваем общий счётчик подключений
    total_connections++;

    // Логируем подключение
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    cout << "New TCP connection from " << client_ip << ":"
        << ntohs(client_addr.sin_port) << " (total: "
        << total_connections << ")\n";
}*/

void acceptTcpConnection(int tcp_server) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(tcp_server, (struct sockaddr*)&client_addr, &client_len);

    if (client_fd == -1) {
        perror("accept() failed");
        return;
    }

    cout << "New TCP connection from " << inet_ntoa(client_addr.sin_addr)
        << ":" << ntohs(client_addr.sin_port) << endl;

    // Добавляем клиентский fd в epoll
    if (!addToEpoll(epoll_fd, client_fd, EPOLLIN | EPOLLET)) {
        close(client_fd);
        return;
    }

    // Отправляем приветствие
    const char* welcome = "Server ready. Type your message:\r\n";
    send(client_fd, welcome, strlen(welcome), 0);
}

//  23.11.2025


// Обработка полной строки от клиента
void processCompleteLine(const string& line, int client_fd) {
    if (line.empty()) return;

    cout << "Received: " << line << endl;

    string response;
    if (!line.empty() && line[0] == '/') {
        response = processCommand(line) + "\r\n";
    } else {
        response = "Echo: " + line + "\r\n";
    }

    //if (send(client_fd, response.c_str(), response.length(), 0) == -1) {
    //    perror("send() failed");

    ssize_t sent = send(client_fd, response.c_str(), response.length(), 0);
    if (sent == -1) {
        perror("send() failed");
    }
    else {
        cout << "Sent " << sent << " bytes to client" << endl;
        cout.flush();  // Принудительный вывод
    }
}

// Чтение данных от TCP‑клиента
void handleTcpClient(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    string input_buffer;

    while (true) {
        bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);

        if (bytes_read > 0) {
            input_buffer.append(buffer, bytes_read);

            // Поиск полных строк (\r\n или \n)
            size_t pos;
            while (true) {
                pos = string::npos;

                if ((pos = input_buffer.find("\r\n")) != string::npos) {
                    string line = input_buffer.substr(0, pos);
                    input_buffer.erase(0, pos + 2);
                    processCompleteLine(line, client_fd);
                    continue;
                }

                if ((pos = input_buffer.find('\n')) != string::npos) {
                    string line = input_buffer.substr(0, pos);
                    input_buffer.erase(0, pos + 1);
                    processCompleteLine(line, client_fd);
                    continue;
                }

                break; // Нет полных строк — ждём дальше
            }
        }
        else if (bytes_read == 0) {
            // Клиент закрыл соединение
            cout << "TCP client (" << client_fd << ") disconnected." << endl;
            close(client_fd);
            // Удаляем из списка клиентов
            for (auto it = clients.begin(); it != clients.end(); ++it) {
                if (it->fd == client_fd) {
                    clients.erase(it);
                    break;
                }
            }
            return;
        }
        else { // bytes_read < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Нет данных — продолжаем цикл
                return;
            }
            else {
                perror("recv() failed");
                close(client_fd);
                // Удаляем клиента
                for (auto it = clients.begin(); it != clients.end(); ++it) {
                    if (it->fd == client_fd) {
                        clients.erase(it);
                        break;
                    }
                }
                return;
            }
        }
    }
}

// Обработка UDP‑клиента
void handleUdpClient(int udp_fd) {
    char buffer[BUFFER_SIZE];
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    ssize_t bytes_read = recvfrom(udp_fd, buffer, BUFFER_SIZE, 0,
                                (sockaddr*)&client_addr, &client_len);

    if (bytes_read > 0) {
        string msg(buffer, bytes_read);

        // Отслеживаем уникальных клиентов (IP + порт)
        static set<pair<uint32_t, uint16_t>> known_clients;
        auto client_id = make_pair(client_addr.sin_addr.s_addr,
                                 client_addr.sin_port);

        if (known_clients.find(client_id) == known_clients.end()) {
            known_clients.insert(client_id);
            cout << "New UDP client: ";
        }
        else {
            cout << "UDP client: ";
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        cout << client_ip << ":" << ntohs(client_addr.sin_port)
             << " -> " << msg << endl;

        string response;
        if (!msg.empty() && msg[0] == '/') {
            response = processCommand(msg);
        }
        else {
            response = msg; // Эхо для UDP
        }

        if (sendto(udp_fd, response.c_str(), response.length(), 0,
              (sockaddr*)&client_addr, client_len) == -1) {
            perror("sendto() failed");
        }
    }
    else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("recvfrom() failed");
    }
}

int main() {
    // Создаём epoll
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1() failed");
        return 1;
    }

    // Запускаем серверы
    int udp_server = createUdpServer(UDP_PORT);
    int tcp_server = createTcpServer(TCP_PORT);

    if (udp_server == -1 || tcp_server == -1) {
        close(epoll_fd);
        return 1;
    }

    // Добавляем серверные сокеты в epoll
    /*addToEpoll(udp_server);
    addToEpoll(tcp_server);*/

    addToEpoll(epoll_fd, udp_server, EPOLLIN | EPOLLET);
    addToEpoll(epoll_fd, tcp_server, EPOLLIN | EPOLLET);

    cout << "UDP/TCP Server running. Waiting for connections...\n";

    epoll_event events[EPOLL_MAX_EVENTS];

    while (true) {
        int nfds = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, -1);

        if (nfds == -1) {
            perror("epoll_wait() failed");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;



            if (events[i].events & EPOLLOUT) {
                // Здесь можно отправлять данные, если есть очередь
                // Для простоты пропускаем (в вашем случае отправка идёт сразу в processCompleteLine)
                continue; // Пропускаю, если это событие отправки...
            }



            if (fd == tcp_server) {
                acceptTcpConnection(tcp_server);
            }
            else if (fd == udp_server) {
                handleUdpClient(udp_server);
            }
            else {
                // Это клиентский TCP‑сокет
                handleTcpClient(fd);
            }
        }
    }

    // Очистка
    close(udp_server);
    close(tcp_server);
    close(epoll_fd);
    return 0;
}