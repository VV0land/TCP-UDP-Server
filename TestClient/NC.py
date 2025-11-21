
import socket
#import time

#s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
###s.bind(("127.0.0.1", 9999))  # Клиент слушает на порту 9999x
#s.sendto(b"Hello UDP", ("127.0.0.1", 8081))
#print("UDP packet sent")

### Ждём 5 секунд, чтобы сервер успел ответить
####time.sleep(5)

# Ждём ответ от сервера
#response, addr = s.recvfrom(1024)
#print("Server response:", response.decode())

#s.close()




def main():
    ###########################################################
    ###   !!! ПРОБРОСЬ ПОРТ НА РОУТЕРЕ ДЛЯ ВНЕШНЕГО IP !!!  ###
    ###########################################################

    # 1. Спрашиваем IP и порт у пользователя (с дефолтами)  37.122.97.89 - это мой внешний
    server_ip = input("IP сервера (по умолчанию 127.0.0.1): ").strip()
    if not server_ip:
        server_ip = "127.0.0.1"

    server_port_str = input("Порт сервера (по умолчанию 60000): ").strip()
    if not server_port_str:
        server_port = 60000
    else:
        try:
            server_port = int(server_port_str)
            if server_port < 1 or server_port > 65535:
                print("Порт должен быть от 1 до 65535. Используем 60000.")
                server_port = 60000
        except ValueError:
            print("Некорректный порт. Используем 60000.")
            server_port = 60000

    # 2. Формируем адрес сервера
    server_addr = (server_ip, server_port)
    print(f"\nПодключение к серверу: {server_ip}:{server_port}")

    # 3. Создаём UDP‑сокет
    client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Привязываем к любому локальному порту (0 = автоматически)
    client.bind(("0.0.0.0", 0))


    print("UDP-клиент запущен. Введите сообщение (или 'quit' для выхода):")

    while True:
        # 1. Ввод сообщения с клавиатуры
        message = input("> ")
        
        if message.lower() == 'quit':
            print("Завершение работы...")
            break

        # 2. Отправляем сообщение серверу
        client.sendto(message.encode('utf-8'), server_addr)
        print(f"Отправлено на сервер: {message}")


        # Устанавливаем таймаут 5 секунд перед чтением
        client.settimeout(5.0)

        try:
            response, server = client.recvfrom(1024)
            print(f"Ответ от сервера {server}: {response.decode('utf-8')}")
        except socket.timeout:
            print("Ответ от сервера не получен (таймаут)")
        except Exception as e:
            print(f"Ошибка при приёме ответа: {e}")


    client.close()
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
 ###    telnet              ###
 ###    open localhost 8080 ###
#################################
#################################