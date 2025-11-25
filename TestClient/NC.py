import socket

def tcp_client(host, port, message):
    """Отправка сообщения через TCP"""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((host, port))
            print(f"[TCP] Подключено к {host}:{port}")

            # Отправляем с \r\n (как ожидает сервер)
            data = (message + "\r\n").encode('utf-8')
            sock.sendall(data)
            print(f"[TCP] Отправлено: {message}")

            # Получаем ответ
            sock.settimeout(5.0)
            response = sock.recv(1024)
            if response:
                print(f"[TCP] Ответ: {response.decode('utf-8', errors='replace')}")
            else:
                print("[TCP] Сервер закрыл соединение без ответа")
    except Exception as e:
        print(f"[TCP] Ошибка: {e}")


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

    ###########################################################
    ###   !!! ПРОБРОСЬ ПОРТ НА РОУТЕРЕ ДЛЯ ВНЕШНЕГО IP !!!  ###
    ###########################################################

    # 1. IP и порт у юзера (127.0.0.1) 
    # 192.168.56.1 - local;
    # 37.122.97.89 - это мой внешний; 
    # 37.122.97.89
    # 192.168.3.240 - ubuntu.

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
    while True:
        try:
            message = input("> ").strip()
            if message.lower() == 'quit':
                break

            if not message:
                continue  # Пропускаем пустые строки

            # Отправка в зависимости от протокола
            if protocol == "TCP":
                tcp_client(server_ip, port, message)
            else:  # UDP
                udp_client(server_ip, port, message)

        except KeyboardInterrupt:
            print("\nПрервано пользователем.")
            break
        except Exception as e:
            print(f"Неожиданная ошибка: {e}")

    print("Клиент закрыт.")

if __name__ == "__main__":
    main()


# Как включить Telnet Client в Windows 10/11:
#    powershell:
#    Add-WindowsCapability -Online -Name TelnetClient~~~~0.0.1.0
#        или
#    dism /online /Enable-Feature /FeatureName:TelnetClient


#################################
#################################
 ###    telnet               ###
 ###    open localhost 50000 ###
#################################
#################################