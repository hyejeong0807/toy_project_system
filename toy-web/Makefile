CC = gcc

CFLAGS = -Wall -std=c99

SRC = web_server.c
HEADER = web_server.h

TARGET = web_server

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)