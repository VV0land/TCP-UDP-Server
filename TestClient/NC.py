
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
    # Создаём UDP‑сокет
    client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # Привязываем сокет к локальному порту (чтобы получать ответы)
    #client.bind(("127.0.0.1", 0))  # 0 = любой свободный порт
    
    # Привязываем сокет к любому локальному адресу и порту
    # Это позволит получать ответы с любого интерфейса
    client.bind(("0.0.0.0", 0))  # 0 = любой свободный порт
    # Или просто не привязывать — ОС сама выберет локальный порт при первой отправке

    server_addr = ("127.0.0.1", 8081)  # 37.122.97.89    127.0.0.1  Адрес сервера


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