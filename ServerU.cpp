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


// Структура клиента: теперь включает буфер для неполных строк
struct ClientData {
    int fd;
    sockaddr_in addr;
    string buffer;  // Буфер для накопления неполных строк
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

// Добавление сокета в epoll (теперь с явными параметрами)
bool addToEpoll(int epoll_fd, int fd, uint32_t events) {
    struct epoll_event event;
    event.events = events;
    event.data.fd = fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        perror("epoll_ctl ADD failed");
        return false;
    }
    return true;
}

// Парсер команд
string processCommand(const string& cmd) {
    if (cmd == "/time") {
        time_t now = time(nullptr);
        tm timeinfo;
        localtime_r(&now, &timeinfo);
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return string(buffer) + "\r\n";
    }
    else if (cmd == "/stats") {
        int active_clients = clients.size();
        return "Total connections: " + to_string(total_connections) +
            ", Active clients: " + to_string(active_clients) + "\r\n";
    }
    else if (cmd == "/shutdown") {
        cout << "Shutdown command received. Server will terminate.\n";
        exit(0);
    }
    else {
        return "Unknown command: " + cmd + "\r\n";
    }
}

// Принятие нового TCP‑подключения
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

    // Проверяем лимит клиентов
    if (clients.size() >= MAX_CLIENTS) {
        cerr << "Max clients limit reached. Rejecting new connection.\n";
        close(client_fd);
        return;
    }

    // Добавляем клиентский fd в epoll
    if (!addToEpoll(epoll_fd, client_fd, EPOLLIN | EPOLLET)) {
        close(client_fd);
        return;
    }

    // Отправляем приветствие
    const char* welcome = "Server ready. Type your message:\r\n";
    send(client_fd, welcome, strlen(welcome), 0);

    // Увеличиваем счётчик подключений
    total_connections++;

    // Добавляем клиента в список (с пустым буфером)
    clients.push_back({ client_fd, client_addr, "" });
}

// Обработка полной строки от клиента
void processCompleteLine(const string& line, int client_fd) {
    if (line.empty()) return;

    cout << "Received: " << line << endl;

    string response;
    if (!line.empty() && line[0] == '/') {
        response = processCommand(line);  // Уже содержит \r\n
    }
    else {
        response = "Echo: " + line + "\r\n";
    }

    ssize_t sent = send(client_fd, response.c_str(), response.length(), 0);
    if (sent == -1) {
        perror("send() failed");
    }
    else {
        cout << "Sent " << sent << " bytes to client" << endl;
        cout.flush();
    }
}

// Чтение данных от TCP‑клиента (исправлено: без while(true), с буфером клиента)
void handleTcpClient(int client_fd) {
    // Находим клиента в списке
    auto it = std::find_if(clients.begin(), clients.end(),
        [client_fd](const ClientData& c) { return c.fd == client_fd; });
    if (it == clients.end()) return;


    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);

    if (bytes_read > 0) {
        it->buffer.append(buffer, bytes_read);  // Добавляем в буфер клиента


        // Обработка полных строк
        // Поиск полных строк (\r\n или \n)
        size_t pos;
        while (true) {
            pos = string::npos;

            // Ищем \r\n (стандартный CRLF)
            if ((pos = it->buffer.find("\r\n")) != string::npos) {
                string line = it->buffer.substr(0, pos);
                it->buffer.erase(0, pos + 2);  // Удаляем обработанную строку + \r\n
                processCompleteLine(line, client_fd);
                continue;
            }

            // Ищем одиночный \n (на случай нестандартных клиентов)
            if ((pos = it->buffer.find('\n')) != string::npos) {
                string line = it->buffer.substr(0, pos);
                it->buffer.erase(0, pos + 1);  // Удаляем обработанную строку + \n
                processCompleteLine(line, client_fd);
                continue;
            }

            break;  // Нет полных строк — ждём следующих данных
        }
    }
    else if (bytes_read == 0) {
        // Клиент закрыл соединение
        cout << "TCP client (" << client_fd << ") disconnected." << endl;

        // Удаляем из epoll
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);

        // Закрываем дескриптор
        close(client_fd);

        // Удаляем клиента из списка
        clients.erase(it);
    }
    else {  // bytes_read < 0 — ошибка
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Нет данных — нормально для edge‑triggered режима
            return;
        }
        else {
            perror("recv() failed");

            // Удаляем из epoll
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);

            // Закрываем дескриптор
            close(client_fd);

            // Удаляем клиента из списка
            clients.erase(it);
        }
    }
}

// Главная функция сервера
int main() {
    // Создаём TCP и UDP серверы
    int tcp_server = createTcpServer(TCP_PORT);
    if (tcp_server == -1) {
        return 1;
    }

    int udp_server = createUdpServer(UDP_PORT);
    if (udp_server == -1) {
        close(tcp_server);
        return 1;
    }

    // Создаём epoll-дескриптор
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1() failed");
        close(tcp_server);
        close(udp_server);
        return 1;
    }

    // Добавляем TCP и UDP сокеты в epoll
    if (!addToEpoll(epoll_fd, tcp_server, EPOLLIN | EPOLLET)) {
        close(epoll_fd);
        close(tcp_server);
        close(udp_server);
        return 1;
    }

    if (!addToEpoll(epoll_fd, udp_server, EPOLLIN | EPOLLET)) {
        close(epoll_fd);
        close(tcp_server);
        close(udp_server);
        return 1;
    }

    // Массив для хранения событий epoll
    struct epoll_event events[EPOLL_MAX_EVENTS];

    cout << "Server running. Waiting for connections..." << endl;

    while (true) {
        // Ожидание событий (тайм‑аут -1 = бесконечное ожидание)
        int nfds = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait() failed");
            break;
        }

        // Обработка всех произошедших событий
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            // Если событие от TCP‑сервера — принимаем новое подключение
            if (fd == tcp_server) {
                acceptTcpConnection(tcp_server);
            }
            // Если событие от UDP‑сервера — обрабатываем UDP‑клиента
            else if (fd == udp_server) {
                // TODO: реализовать handleUdpClient()
                cout << "UDP event received (not implemented)" << endl;
            }
            // Иначе — событие от клиентского TCP‑сокета
            else {
                handleTcpClient(fd);
            }
        }
    }

    // Очистка ресурсов перед завершением
    close(epoll_fd);
    close(tcp_server);
    close(udp_server);

    return 0;
}