import socket

'''def tcp_client(host, port, message, sock=None):
    """
    Отправка сообщения через TCP.
    Если sock=None — создаёт новое соединение.
    Иначе использует переданный сокет.
    Возвращает сокет для повторного использования.
    """
    try:  
        if sock is None:
            # Создаём новый сокет и подключаемся
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((host, port))
            print(f"[TCP] Подключено к {host}:{port}")

        # Отправляем сообщение с CRLF
        data = (message + "\r\n").encode('utf-8')
        sock.sendall(data)
        print(f"[TCP] Отправлено: {message}")

        # Получаем ответ (таймаут 5 сек)
        sock.settimeout(5.0)
        response = sock.recv(1024)
        if response:
            print(f"[TCP] Ответ: {response.decode('utf-8', errors='replace')}")
        else:
            print("[TCP] Сервер закрыл соединение без ответа")

        return sock  # Возвращаем сокет для дальнейших вызовов


    except Exception as e:
        print(f"[TCP] Ошибка: {e}")
        if sock:
            sock.close()
        return None
'''
#
#
'''def tcp_client(host, port, message, sock=None):
    try:
        if sock is None:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((host, port))
            print(f"[TCP] Подключено к {host}:{port}")

        # Отправляем сообщение
        data = (message + "\r\n").encode('utf-8')
        sock.sendall(data)
        print(f"[TCP] Отправлено: {message}")

        # Читаем ответ до первой новой строки (\r\n или \n)
        response_buffer = ""
        while True:
            chunk = sock.recv(1).decode('utf-8', errors='replace')
            if not chunk:  # Сервер закрыл соединение
                break
            response_buffer += chunk
            # Проверяем, закончился ли ответ
            if response_buffer.endswith("\r\n") or response_buffer.endswith("\n"):
                break

        if response_buffer:
            print(f"[TCP] Ответ: {response_buffer.rstrip()}")
        else:
            print("[TCP] Сервер закрыл соединение без ответа")

        return sock

    except Exception as e:
        print(f"[TCP] Ошибка: {e}")
        if sock:
            sock.close()
        return None
'''

def tcp_client(host, port, message, sock=None):
    try:
        if sock is None:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((host, port))
            print(f"[TCP] Подключено к {host}:{port}")

        data = (message + "\r\n").encode('utf-8')
        sock.sendall(data)
        print(f"[TCP] Отправлено: {message}")

        # Читаем ответ до \r\n или \n
        response_buffer = ""
        while True:
            chunk = sock.recv(1).decode('utf-8', errors='replace')
            if not chunk:
                break
            response_buffer += chunk
            if response_buffer.endswith("\r\n") or response_buffer.endswith("\n"):
                break

        if response_buffer:
            print(f"[TCP] Ответ: {response_buffer.rstrip()}")
        else:
            print("[TCP] Сервер закрыл соединение без ответа")

        return sock

    except Exception as e:
        print(f"[TCP] Ошибка: {e}")
        if sock:
            sock.close()
        return None


def udp_client(host, port, message):
    """Отправка сообщения через UDP"""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.bind(("0.0.0.0", 0))  # Любой свободный локальный порт
            sock.settimeout(5.0)


            data = message.encode('utf-8')
            sock.sendto(data, (host, port))
            print(f"[UDP] Отправлено на {host}:{port}: {message}")

            try:
                response, server = sock.recvfrom(1024)
                print(f"[UDP] Ответ от {server}: {response.decode('utf-8', errors='replace')}")
            except socket.timeout:
                print("[UDP] Таймаут: ответ не получен")
    except Exception as e:
        print(f"[UDP] Ошибка: {e}")


def main():
    print("=== TEST-Drive TCP/UDP-Server ===\n")


    # 1. Ввод IP сервера
    server_ip = input("IP сервера (по умолчанию 127.0.0.1): ").strip()
    if not server_ip:
        server_ip = "127.0.0.1"


    # 2. Выбор протокола
    print("\nВыберите протокол:")
    print("1 - TCP (порт по умолчанию 50000)")
    print("2 - UDP (порт по умолчанию 60000)")
    proto_choice = input("Ваш выбор (1 или 2, по умолчанию 1): ").strip()

    if proto_choice == "2":
        protocol = "UDP"
        default_port = 60000
    else:
        protocol = "TCP"
        default_port = 50000


    # 3. Ввод порта
    port_str = input(f"Порт сервера (по умолчанию {default_port}): ").strip()
    if not port_str:
        port = default_port
    else:
        try:
            port = int(port_str)
            if port < 1 or port > 65535:
                print("Порт должен быть от 1 до 65535. Используем значение по умолчанию.")
                port = default_port
        except ValueError:
            print("Некорректный порт. Используем значение по умолчанию.")
            port = default_port

    print(f"\nПодключение: {protocol} -> {server_ip}:{port}")
    print("Введите сообщение (или 'quit' для выхода):\n")


    # 4. Основной цикл общения
    sock = None  # Храним TCP-сокет между вызовами
    try:
        while True:
            message = input("> ").strip()
            if message.lower() == 'quit':
                if sock:
                    sock.close()
                    print("[TCP] Соединение закрыто.")
                break

            if not message:
                continue  # Пропускаем пустые строки

            if protocol == "TCP":
                sock = tcp_client(server_ip, port, message, sock)
                if sock is None:
                    print("[TCP] Не удалось отправить сообщение. Проверьте соединение.")
            else:  # UDP
                udp_client(server_ip, port, message)

    except KeyboardInterrupt:
        print("\nПрервано пользователем.")
        if sock:
            sock.close()
    except Exception as e:
        print(f"Неожиданная ошибка: {e}")
        if sock:
            sock.close()

    print("Клиент закрыт.")

if __name__ == "__main__":
    main()