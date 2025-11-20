//  18.11.2025
//  Простеннький UDP-сервер.
//  20.11.2025
//  Добавлен TCP-сервер.


#include <iostream>
#include <string>
#include <ws2tcpip.h>
#include <set>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int BUFFER_SIZE = 1024;
const int MAX_CLIENTS = 100;

fd_set readfds, allfds;
SOCKET max_sd;
int total_connections = 0;

struct ClientData {
    SOCKET fd;
    sockaddr_in addr;
    int addr_len;
};

std::vector<ClientData> clients;

bool initWinsock() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed with error: " << WSAGetLastError() << endl;
        return false;
    }
    return true;
}

bool setNonBlocking(SOCKET sock) {
    u_long nonBlocking = 1;
    if (ioctlsocket(sock, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
        cerr << "ioctlsocket failed with error: " << WSAGetLastError() << endl;
        return false;
    }
    return true;
}

SOCKET createUdpServer(int port) {
    SOCKET server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_fd == INVALID_SOCKET) {
        cerr << "socket() failed with error: " << WSAGetLastError() << endl;
        return INVALID_SOCKET;
    }

    if (!setNonBlocking(server_fd)) {
        closesocket(server_fd);
        return INVALID_SOCKET;
    }

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "bind() failed with error: " << WSAGetLastError() << endl;
        closesocket(server_fd);
        return INVALID_SOCKET;
    }

    cout << "UDP server started on port " << port << endl;
    return server_fd;
}

SOCKET createTcpServer(int port) {
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "socket() failed with error: " << WSAGetLastError() << std::endl;
        return INVALID_SOCKET;
    }

    if (!setNonBlocking(server_fd)) {
        closesocket(server_fd);
        return INVALID_SOCKET;
    }

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        return INVALID_SOCKET;
    }

    if (listen(server_fd, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        return INVALID_SOCKET;
    }

    std::cout << "TCP server started on port " << port << std::endl;
    return server_fd;
}

void acceptTcpConnection(SOCKET server_fd) {
    sockaddr_in client_addr = {};
    int client_len = sizeof(client_addr);
    SOCKET client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);

    if (client_fd == INVALID_SOCKET) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            std::cerr << "accept() failed with error: " << WSAGetLastError() << std::endl;
        }
        return;
    }

    if (clients.size() >= MAX_CLIENTS) {
        std::cerr << "Max clients limit reached. Rejecting new connection." << std::endl;
        closesocket(client_fd);
        return;
    }

    clients.push_back({ client_fd, client_addr, client_len });
    FD_SET(client_fd, &allfds);
    if (client_fd > max_sd) max_sd = client_fd;


    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    std::cout << "New TCP connection from " << client_ip << ":"
        << ntohs(client_addr.sin_port) << std::endl;

    total_connections++;
}

// Парсер команд и формирование ответа
string processCommand(const string& cmd) {
    if (cmd == "/time") {
        time_t now = time(nullptr);
        struct tm timeinfo;
        if (localtime_s(&timeinfo, &now) == 0) {
            char buffer[20];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
            return string(buffer);
        }
        else {
            return "Error getting time";
        }

    } else if (cmd == "/stats") {
        return "Total connections: " + to_string(total_connections) + ", Active clients: " + to_string(0);
        //  Active clients — только для TCP‑клиентов (так как UDP не имеет "активного соединения").
    } else if (cmd == "/shutdown") {
        cout << "Shutdown command received. Server will terminate.\n";
        exit(0);
    } else {
        return "Unknown command: " + cmd;
    }
}

void handleUdpClient(SOCKET udp_fd) {
    char buffer[BUFFER_SIZE];
    sockaddr_in client_addr{};
    int client_len = sizeof(client_addr);

    int bytes_read = recvfrom(udp_fd, buffer, BUFFER_SIZE, 0,
        (sockaddr*)&client_addr, &client_len);

    if (bytes_read > 0) {
        string msg(buffer, bytes_read);

        static set<pair<uint32_t, uint16_t>> known_clients;
        //  Уникальный идентификатор клиента формируется как комбинация IP-адреса и номера порта
        auto client_id = make_pair(client_addr.sin_addr.s_addr, client_addr.sin_port);

        if (known_clients.find(client_id) == known_clients.end()) {
        //  Проверка наличия клиента в списке известных клиентов,      //  Eсли это первое сообщение от этого клиента
            total_connections++;
            known_clients.insert(client_id);
        }

        cout << "Received from UDP client ";

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        cout << client_ip << ":" << ntohs(client_addr.sin_port)
            << ": " << msg << endl;

        //  Проверка на слеш '/'
        if (!msg.empty() && msg[0] == '/') {
            string response = processCommand(msg);
            if (sendto(udp_fd, response.c_str(), (int)response.length(), 0,
                (sockaddr*)&client_addr, client_len) == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error != WSAECONNRESET)     //  10054L
                {  // Игнорируем 10054
                    cerr << "sendto() failed with error: " << error << endl;
                }
            }
        }
        else {
            //  Зеркалка для обычных сообщений
            if (sendto(udp_fd, buffer, bytes_read, 0,
                (sockaddr*)&client_addr, client_len) == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error != WSAECONNRESET) {
                    cerr << "sendto() failed with error: " << error << endl;
                }
            }
        }
    }
    else if (bytes_read == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            cerr << "recvfrom() failed with error: " << error << endl;
        }
    }
}

int main() {
    if (!initWinsock()) {
        return 1;
    }
    SOCKET udp_server = createUdpServer(50000);
    SOCKET tcp_server = createTcpServer(8080);

    FD_ZERO(&allfds);
    FD_SET(tcp_server, &allfds);
    FD_SET(udp_server, &allfds);
    max_sd = max(tcp_server, udp_server);

    cout << "UDP/TCP Server running. Waiting for connections...\n";

    while (true) {
        readfds = allfds;

        if (select((int)max_sd + 1, &readfds, nullptr, nullptr, nullptr) == SOCKET_ERROR) {
            std::cerr << "select() failed with error: " << WSAGetLastError() << std::endl;
            break;
        }

        // Обработка TCP-сервера (новые подключения)
        if (FD_ISSET(tcp_server, &readfds)) {
            acceptTcpConnection(tcp_server);
        }

        //  Обработка UDP-сервера
        if (FD_ISSET(udp_server, &readfds)) {
            handleUdpClient(udp_server);
        }
    }

    //  Закрытие сокетов
    closesocket(udp_server);
    closesocket(tcp_server);
    WSACleanup();
    return 0;
}


