CC = gcc
CFALGS = -g -Wall

TARGET = toy_system
INCLUDES = -I./ -I./system -I./ui -I./web_server

OBJS = main.o \
       system_server.o \
	   gui.o \
       input.o \
	   web_server.o

.PHONY: clean

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS)

main.o: main.c
	$(CC) $(CFALGS) $(INCLUDES) -c main.c

system_server.o: ./system/system_server.h ./system/system_server.c
	$(CC) $(CFALGS) $(INCLUDES) -c ./system/system_server.c

gui.o: ./ui/gui.h ./ui/gui.c
	$(CC) $(CFLAGS) $(INCLUDES) -c ./ui/gui.c

input.o: ./ui/input.h ./ui/input.c
	$(CC) $(CFLAGS) $(INCLUDES) -c ./ui/input.c

web_server.o: ./web_server/web_server.h ./web_server/web_server.c
	$(CC) $(CFLAGS) $(INCLUDES) -c ./web_server/web_server.c

clean:
	rm -rf *.o
	rm -rf $(TARGET)