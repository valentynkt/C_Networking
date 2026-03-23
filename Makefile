CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c17 -g -fsanitize=address,undefined
SRC     = src/main.c src/common.c src/blocking.c src/nonblocking.c src/select_server.c src/kqueue_server.c
TARGET  = netpractice

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: clean
