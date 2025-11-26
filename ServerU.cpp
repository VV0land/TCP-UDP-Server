// ServerU.cpp (исправленный)
#include <iostream>
#include <string>
#include <vector>
#include <set>
//#include <cstring>
#include <unistd.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
//#include <errno.h>
#include <algorithm>
using namespace std;

const int BUFFER_SIZE = 1024;
const int MAX_CLIENTS = 100;
const int EPOLL_MAX_EVENTS = 64;

const int TCP_PORT = 50000;
const int UDP_PORT = 60000;

int total_connections = 0;  // Общее число TCP‑подключений за всё время
int udp_connections = 0;   // Общее число UDP‑подключений за всё время

struct UdpClient {
    uint32_t ip;
    uint16_t port;
    time_t last_activity;
};

set<UdpClient> known_udp_clients;

struct ClientData {
    int fd;
    sockaddr_in addr;
    string buffer;
    time_t last_activity;
};

vector<ClientData> clients;
int epoll_fd;

bool setNonBlocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return false;
    flags |= O_NONBLOCK;
    if (fcntl(sock, F_SETFL, flags) == -1) return false;
    return true;
}

int createUdpServer(int port) {
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1) { perror("socket() UDP failed"); return -1; }
    if (!setNonBlocking(server_fd)) { close(server_fd); return -1; }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind() UDP failed"); close(server_fd); return -1;
    }
    cout << "UDP server started on port " << port << endl;
    return server_fd;
}

int createTcpServer(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) { perror("socket() TCP failed"); return -1; }
    if (!setNonBlocking(server_fd)) { close(server_fd); return -1; }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind() TCP failed"); close(server_fd); return -1;
    }
    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen() TCP failed"); close(server_fd); return -1;
    }
    cout << "TCP server started on port " << port << endl;
    return server_fd;
}

bool addToEpoll(int epoll_fd, int fd, uint32_t events) {
    epoll_event event;
    event.events = events;
    //event.data.
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        perror("epoll_ctl ADD failed");
        return false;
}
return true;
}

string processCommand(const string& cmd) {
    if (cmd == "/time") {
        time_t now = time(nullptr);
        tm timeinfo;
        localtime_r(&now, &timeinfo);
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return string(buffer);
    }
    else if (cmd == "/stats") {
        int active_tcp = clients.size();
        int active_udp = known_udp_clients.size();
        return "Total TCP: " + to_string(total_connections) +
            " | Active TCP: " + to_string(active_tcp) +
            " | Active UDP: " + to_string(active_udp);
    }
    else if (cmd == "/shutdown") {
        cout << "Shutdown command received. Server will terminate.\n";
        exit(0);
    }
    else {
        return "Unknown command: " + cmd;
    }
}

void acceptTcpConnection(int tcp_server) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(tcp_server, (sockaddr*)&client_addr, &client_len);

    if (client_fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept() failed");
        }
        return;
    }

    if (clients.size() >= MAX_CLIENTS) {
        cerr << "Max clients limit reached. Rejecting new connection.\n";
        close(client_fd);
        return;
    }

    if (!setNonBlocking(client_fd)) {
        close(client_fd);
        return;
    }

    clients.push_back({ client_fd, client_addr, "", time(nullptr) });
    total_connections++;

    if (!addToEpoll(epoll_fd, client_fd, EPOLLIN | EPOLLET)) {
        close(client_fd);
        clients.pop_back();
        return;
    }

    cout << "New TCP connection from " << inet_ntoa(client_addr.sin_addr)
        << ":" << ntohs(client_addr.sin_port) << " (total: "
        << total_connections << ")\n";
}

void processCompleteLine(const string& line, int client_fd) {
    if (line.empty()) return;

    cout << "Received: " << line << endl;

    string response;
    if (!line.empty() && line[0] == '/') {
        response = processCommand(line) + "\r\n";
    }
    else {
        response = line + "\r\n";
    }

    ssize_t sent = send(client_fd, response.c_str(), response.length(), 0);
    if (sent == -1) {
        perror("send() failed");
    }
    else {
        cout << "Sent " << sent << " bytes to client\n";
    }
}

void handleTcpClient(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);

    auto it = find_if(clients.begin(), clients.end(),
        [client_fd](const ClientData& c) { return c.fd == client_fd; });
    if (it == clients.end()) return;


    if (bytes_read > 0) {
        it->buffer.append(buffer, bytes_read);
        it->last_activity = time(nullptr);


        size_t pos;
        while (true) {
            pos = string::npos;
            if ((pos = it->buffer.find("\r\n")) != string::npos) {
                string line = it->buffer.substr(0, pos);
                it->buffer.erase(0, pos + 2);
                processCompleteLine(line, client_fd);
                continue;
            }
            if ((pos = it->buffer.find('\n')) != string::npos) {
                string line = it->buffer.substr(0, pos);
                it->buffer.erase(0, pos + 1);
                processCompleteLine(line, client_fd);
                continue;
            }
            break;
        }
    }
    else if (bytes_read == 0 || (bytes_read == -1 && (errno == EPIPE || errno == ECONNRESET))) {
        // Клиент закрыл соединение или ошибка
        cout << "TCP client (" << client_fd << ") disconnected.\n";
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
        close(client_fd);
        clients.erase(it);
    }
    else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("recv() failed");
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
        close(client_fd);
        clients.erase(it);
    }
}

void handleUdpClient(int udp_fd) {
    char buffer[BUFFER_SIZE];
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    ssize_t bytes_read = recvfrom(udp_fd, buffer, BUFFER_SIZE, 0,
        (sockaddr*)&client_addr, &client_len);

    if (bytes_read > 0) {
        UdpClient client_id = { client_addr.sin_addr.s_addr, client_addr.sin_port, time(nullptr) };
        auto [it, inserted] = known_udp_clients.insert(client_id);
        if (inserted) {
            udp_connections++;
            cout << "New UDP client: ";
        }
        else {
            it->last_activity = time(nullptr); // Обновляем время активности
            cout << "UDP client: ";
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        cout << client_ip << ":" << ntohs(client_addr.sin_port)
            << " -> " << string(buffer, bytes_read) << endl;

        string msg(buffer, bytes_read);
        string response;
        if (!msg.empty() && msg[0] == '/') {
            response = processCommand(msg);
        }
        else {
            response = msg;
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

void cleanupInactiveClients() {
    const int INACTIVITY_TIMEOUT = 60;
    for (auto it = clients.begin(); it != clients.end(); ) {
        if (time(nullptr) - it->last_activity > INACTIVITY_TIMEOUT) {
            cout << "Closing inactive client FD=" << it->fd
                << " (IP: " << inet_ntoa(it->addr.sin_addr)
                << ":" << ntohs(it->addr.sin_port) << ")\n";
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, it->fd, nullptr);
            close(it->fd);
            it = clients.erase(it);
        }
        else {
            ++it;
        }
    }
}

void cleanupInactiveUdpClients() {
    const int INACTIVITY_TIMEOUT = 120; // Таймаут для UDP
event.data.fd = fd;
if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
    perror("epoll_ctl ADD failed");
    return false;
}
return true;
}

string processCommand(const string& cmd) {
    if (cmd == "/time") {
        time_t now = time(nullptr);
        tm timeinfo;
        localtime_r(&now, &timeinfo);
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return string(buffer);
    }
    else if (cmd == "/stats") {
        int active_tcp = clients.size();
        int active_udp = known_udp_clients.size();
        return "Total TCP: " + to_string(total_connections) +
            " | Active TCP: " + to_string(active_tcp) +
            " | Active UDP: " + to_string(active_udp);
    }
    else if (cmd == "/shutdown") {
        cout << "Shutdown command received. Server will terminate.\n";
        exit(0);
    }
    else {
        return "Unknown command: " + cmd;
    }
}

void acceptTcpConnection(int tcp_server) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(tcp_server, (sockaddr*)&client_addr, &client_len);

    if (client_fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept() failed");
        }
        return;
    }

    if (clients.size() >= MAX_CLIENTS) {
        cerr << "Max clients limit reached. Rejecting new connection.\n";
        close(client_fd);
        return;
    }

    if (!setNonBlocking(client_fd)) {
        close(client_fd);
        return;
    }

    clients.push_back({ client_fd, client_addr, "", time(nullptr) });
    total_connections++;

    if (!addToEpoll(epoll_fd, client_fd, EPOLLIN | EPOLLET)) {
        close(client_fd);
        clients.pop_back();
        return;
    }

    cout << "New TCP connection from " << inet_ntoa(client_addr.sin_addr)
        << ":" << ntohs(client_addr.sin_port) << " (total: "
        << total_connections << ")\n";
}

void processCompleteLine(const string& line, int client_fd) {
    if (line.empty()) return;

    cout << "Received: " << line << endl;

    string response;
    if (!line.empty() && line[0] == '/') {
        response = processCommand(line) + "\r\n";
    }
    else {
        response = line + "\r\n";
    }

    ssize_t sent = send(client_fd, response.c_str(), response.length(), 0);
    if (sent == -1) {
        perror("send() failed");
    }
    else {
        cout << "Sent " << sent << " bytes to client\n";
    }
}

void handleTcpClient(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);

    auto it = find_if(clients.begin(), clients.end(),
        [client_fd](const ClientData& c) { return c.fd == client_fd; });
    if (it == clients.end()) return;


    if (bytes_read > 0) {
        it->buffer.append(buffer, bytes_read);
        it->last_activity = time(nullptr);


        size_t pos;
        while (true) {
            pos = string::npos;
            if ((pos = it->buffer.find("\r\n")) != string::npos) {
                string line = it->buffer.substr(0, pos);
                it->buffer.erase(0, pos + 2);
                processCompleteLine(line, client_fd);
                continue;
            }
            if ((pos = it->buffer.find('\n')) != string::npos) {
                string line = it->buffer.substr(0, pos);
                it->buffer.erase(0, pos + 1);
                processCompleteLine(line, client_fd);
                continue;
            }
            break;
        }
    }
    else if (bytes_read == 0 || (bytes_read == -1 && (errno == EPIPE || errno == ECONNRESET))) {
        // Клиент закрыл соединение или ошибка
        cout << "TCP client (" << client_fd << ") disconnected.\n";
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
        close(client_fd);
        clients.erase(it);
    }
    else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("recv() failed");
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
        close(client_fd);
        clients.erase(it);
    }
}

void handleUdpClient(int udp_fd) {
    char buffer[BUFFER_SIZE];
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    ssize_t bytes_read = recvfrom(udp_fd, buffer, BUFFER_SIZE, 0,
        (sockaddr*)&client_addr, &client_len);

    if (bytes_read > 0) {
        UdpClient client_id = { client_addr.sin_addr.s_addr, client_addr.sin_port, time(nullptr) };
        auto [it, inserted] = known_udp_clients.insert(client_id);
        if (inserted) {
            udp_connections++;
            cout << "New UDP client: ";
        }
        else {
            it->last_activity = time(nullptr); // Обновляем время активности
            cout << "UDP client: ";
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        cout << client_ip << ":" << ntohs(client_addr.sin_port)
            << " -> " << string(buffer, bytes_read) << endl;

        string msg(buffer, bytes_read);
        string response;
        if (!msg.empty() && msg[0] == '/') {
            response = processCommand(msg);
        }
        else {
            response = msg;
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

void cleanupInactiveClients() {
    const int INACTIVITY_TIMEOUT = 60;
    for (auto it = clients.begin(); it != clients.end(); ) {
        if (time(nullptr) - it->last_activity > INACTIVITY_TIMEOUT) {
            cout << "Closing inactive client FD=" << it->fd
                << " (IP: " << inet_ntoa(it->addr.sin_addr)
                << ":" << ntohs(it->addr.sin_port) << ")\n";
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, it->fd, nullptr);
            close(it->fd);
            it = clients.erase(it);
        }
        else {
            ++it;
        }
    }
}

void cleanupInactiveUdpClients() {
    const int INACTIVITY_TIMEOUT = 120; // Таймаут для UDP
time_t now = time(nullptr);
for (auto it = known_udp_clients.begin(); it != known_udp_clients.end(); ) {
    if (now - it->last_activity > INACTIVITY_TIMEOUT) {
        cout << "Removing inactive UDP client: "
            << inet_ntoa(*(struct in_addr*)&it->ip)
            << ":" << ntohs(it->port) << endl;
        it = known_udp_clients.erase(it);
    }
    else {
        ++it;
    }
}
}

int main() {
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1() failed");
        return 1;
    }

    int udp_server = createUdpServer(UDP_PORT);
    int tcp_server = createTcpServer(TCP_PORT);

    if (udp_server == -1 || tcp_server == -1) {
        close(epoll_fd);
        return 1;
    }

    if (!addToEpoll(epoll_fd, udp_server, EPOLLIN | EPOLLET) ||
        !addToEpoll(epoll_fd, tcp_server, EPOLLIN | EPOLLET)) {
        close(udp_server);
        close(tcp_server);
        close(epoll_fd);
        return 1;
    }

    cout << "UDP/TCP Server running. Waiting for connections...\n";

    epoll_event events[EPOLL_MAX_EVENTS];

    while (true) {
        int nfds = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, 1000);
        if (nfds == -1) {
            perror("epoll_wait() failed");
            break;
        }

        // Обработка событий
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == udp_server) {
                handleUdpClient(udp_server);
            }
            else if (fd == tcp_server) {
                acceptTcpConnection(tcp_server);
            }
            else {
                // Это клиентский fd
                if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                    cout << "Client FD=" << fd << " disconnected (EPOLLHUP/EPOLLERR)\n";
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);

                    // Удаляем из clients
                    auto it = find_if(clients.begin(), clients.end(),
                        [fd](const ClientData& c) { return c.fd == fd; });
                    if (it != clients.end()) {
                        clients.erase(it);
                    }
                }
                else if (events[i].events & EPOLLIN) {
                    handleTcpClient(fd);
                }
            }
        }

        // Регулярная очистка
        cleanupInactiveClients();
        cleanupInactiveUdpClients();
    }

    close(udp_server);
    close(tcp_server);
    close(epoll_fd);
    return 0;
}