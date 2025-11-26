# 29.11.25 Добавлен ping и возможность ввода IP с 192.168.X.X
import socket
import subprocess
import re
import platform

# TCP-коннект
def tcp_client(host, port, message, sock=None):
    try:
        if sock is None:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((host, port))
            print(f"[TCP] Подключено к {host}:{port}")

        data = (message + "\r\n").encode('utf-8')
        sock.sendall(data)
        print(f"[TCP] Отправлено: {message}")

        # Читаем ответ до \r\n или \n (до Enter Win/*NX)
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

# UDP-коннект
def udp_client(host, port, message):
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

# Проверка IP x.x.x.x
def is_valid_ip(ip):
    pattern = re.compile(r'^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$')
    match = pattern.match(ip)
    if not match:
        return False
    for part in match.groups():
        if int(part) > 255:
            return False
    return True

# Ping  # Параметр qc (queryes count) под Windows и *NX
def ping_host(ip):
    qc = '-n' if platform.system().lower() == 'windows' else '-c'
    
    try:
        result = subprocess.run(
            ['ping', qc, '3', ip],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=3
        )
        return result.returncode == 0
    except subprocess.TimeoutExpired:
        return False
    except Exception:
        return False


def main():
    print("=== TEST-Drive TCP/UDP-Server ===\n")

    # 1. Ввод IP сервера
    """    server_ip = input("IP сервера (по умолчанию 127.0.0.1): ").strip()
    if not server_ip:
        server_ip = "127.0.0.1" """

    while True:
        print("Введите последние 2 числа IP (192.168.X.X)")
        print("Enter для 127.0.0.1")
    
        user_input = input("192.168.").strip()
    
        if not user_input:
            server_ip = "127.0.0.1"
            print(f"Выбран IP по умолчанию: {server_ip}")
        else:
            # Проверка формат ввода (два числа через точку)
            if re.match(r'^\d+\.\d+$', user_input):
                parts = user_input.split('.')
                if all(0 <= int(part) <= 255 for part in parts):
                    server_ip = f"192.168.{parts[0]}.{parts[1]}"
                else:
                    print("Ошибка: числа должны быть в диапазоне 0–255.")
                    continue
            else:
                print("Ошибка: введите два числа через точку.")
                continue
    
        # Проверяем корректность собранного IP
        if not is_valid_ip(server_ip):
            print("Ошибка: некорректный IP-адрес.")
            continue
    
        print(f"Пингую {server_ip}...")
    
        # Выполняем ping
        if ping_host(server_ip):
            print(f"IP {server_ip} доступен. Пинг-понг успешен.")
            break  # Выходим из цикла, если ping прошёл
        else:
            print(f"Ошибка: IP {server_ip} недоступен.")

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