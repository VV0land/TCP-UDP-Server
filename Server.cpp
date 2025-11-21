//  18.11.2025
//  Простеннький UDP-сервер.
//  20.11.2025
//  Добавлен TCP-сервер.
//  21.11.2025
//  gim commit - m "Небольшие изменения серверной части + возможность вводить IP/port на клиенте



#include <iostream>
#include <string>
#include <ws2tcpip.h>
#include <set>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int BUFFER_SIZE = 1024;
const int MAX_CLIENTS = 100;

// !!! ПРОБРОСЬ ПОРТ НА РОУТЕРЕ ДЛЯ ВНЕШНЕГО IP !!!
const int TCPort = 50000;
const int UDPort = 60000;

fd_set readfds, allfds;
SOCKET max_sd;
int total_connections = 0;

struct ClientData {
    SOCKET fd;
    sockaddr_in addr;
    int addr_len;
};

vector<ClientData> clients;

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

    if (listen(server_fd, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "listen() failed with error: " << WSAGetLastError() << endl;
        closesocket(server_fd);
        return INVALID_SOCKET;
    }

    cout << "TCP server started on port " << port << endl;
    return server_fd;
}

void acceptTcpConnection(SOCKET server_fd) {
    sockaddr_in client_addr = {};
    int client_len = sizeof(client_addr);
    SOCKET client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);

    if (client_fd == INVALID_SOCKET) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            cerr << "accept() failed with error: " << WSAGetLastError() << endl;
        }
        return;
    }

    if (clients.size() >= MAX_CLIENTS) {
        cerr << "Max clients limit reached. Rejecting new connection." << endl;
        closesocket(client_fd);
        return;
    }

    clients.push_back({ client_fd, client_addr, client_len });
    FD_SET(client_fd, &allfds);
    if (client_fd > max_sd) max_sd = client_fd;


    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    cout << "New TCP connection from " << client_ip << ":"
        << ntohs(client_addr.sin_port) << endl;

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
            return string(buffer) + "\r\n";
        }
        else {
            return "Error getting time";
        }

    } else if (cmd == "/stats") {
        return "Total connections: " + to_string(total_connections) + ", Active clients: " + to_string(0) + "\r\n";
        //  Active clients — только для TCP‑клиентов (так как UDP не имеет "активного соединения").
    } else if (cmd == "/shutdown") {
        cout << "Shutdown command received. Server will terminate.\n";
        exit(0);
    } else {
        return "Unknown command: " + cmd + "\r\n";
    }
}

void processCompleteLine(const string& line, SOCKET client_fd) {
    if (line.empty()) return;

    cout << "Received full line from TCP client (" << client_fd << "): " << line << endl;

    // Проверяем, начинается ли строка с '/'
    if (!line.empty() && line[0] == '/') {
        string response = processCommand(line);
        if (send(client_fd, response.c_str(), (int)response.length(), 0) == SOCKET_ERROR) {
            cerr << "send() failed with error: " << WSAGetLastError() << endl;
        }
    }
    else {
        // Эхо-ответ для обычных сообщений
        string echo = "Echo: " + line + "\r\n";
        if (send(client_fd, echo.c_str(), (int)echo.length(), 0) == SOCKET_ERROR) {
            cerr << "send() failed with error: " << WSAGetLastError() << endl;
        }
    }
}

void handleTcpClient(SOCKET client_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    string input_buffer;

    while (true) {
        bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);

        // Обработка ошибок и закрытия соединения
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                cout << "TCP client (" << client_fd << ") disconnected." << endl;
                break;
            }

            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                // Нет данных — пропускаем итерацию, не закрываем соединение
                continue;
            }
            else {
                cerr << "recv() failed with error: " << error << endl;
                break;
            }
        }

        // Добавляем данные в буфер
        input_buffer.append(buffer, bytes_read);

        // Поиск и обработка полных строк (как в предыдущем примере)
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

            break;  // Нет полных строк — ждём дальше
        }
    }

    // Закрытие соединения (как раньше)
    closesocket(client_fd);
    FD_CLR(client_fd, &allfds);
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->fd == client_fd) {
            clients.erase(it);
            break;
        }
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

    // !!! ПРОБРОСЬ ПОРТ НА РОУТЕРЕ ДЛЯ ВНЕШНЕГО IP !!!
    SOCKET udp_server = createUdpServer(UDPort);  //  8081
    SOCKET tcp_server = createTcpServer(TCPort);  //  8080

    if (tcp_server == INVALID_SOCKET || udp_server == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    FD_ZERO(&allfds);
    FD_SET(tcp_server, &allfds);
    FD_SET(udp_server, &allfds);
    max_sd = max(tcp_server, udp_server);


    cout << "UDP/TCP Server running. Waiting for connections...\n";

    while (true) {
        readfds = allfds;

        if (select((int)max_sd + 1, &readfds, nullptr, nullptr, nullptr) == SOCKET_ERROR) {
            cerr << "select() failed with error: " << WSAGetLastError() << endl;
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

        // Обработка существующих TCP-клиентов
        for (auto& client : clients) {
            if (FD_ISSET(client.fd, &readfds)) {
                handleTcpClient(client.fd);
            }
        }
    }

    //  Закрытие сокетов
    closesocket(udp_server);
    closesocket(tcp_server);
    WSACleanup();
    return 0;
}


