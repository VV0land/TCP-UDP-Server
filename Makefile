# Компилятор
CC = g++

# Флаги компиляции (C++17, оптимизация, предупреждения)
CFLAGS = -std=c++17 -O2 -Wall -Wextra


# Имя исполняемого файла
TARGET = serveru


# Исходные файлы
SRC = ServerU.cpp

# Объектные файлы
OBJ = $(SRC:.cpp=.o)


# Правила
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) -pthread


%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@


# Очистка
clean:
	rm -f $(OBJ) $(TARGET)


# Установка в /usr/local/bin
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)


.PHONY: all clean install