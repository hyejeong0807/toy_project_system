CC = gcc
CFALGS = -g -Wall
CXXLIBS = -lpthread -lm -lrt
CXX = g++
CXXFLAGS = $(INCLUDEDIRS) -g -O0 -std=c++14

TARGET = toy_system

HAL = ./hal
INCLUDES = -I./ -I./system -I./ui -I./web_server -I$(HAL)

OBJS = main.o \
       system_server.o \
	   gui.o \
       input.o \
	   web_server.o

CXX_OBJS = camera_HAL.o ControlThread.o

.PHONY: clean

$(TARGET): $(OBJS) $(CXX_OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CXX_OBJS) $(CXXLIBS)

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

camera_HAL.o: $(HAL)/camera_HAL.cpp
	$(CXX) -g $(INCLUDES) $(CXXFLAGS) -c $(HAL)/camera_HAL.cpp

ControlThread.o: $(HAL)/ControlThread.cpp
	$(CXX) -g $(INCLUDES) $(CXXFLAGS) -c $(HAL)/ControlThread.cpp

clean:
	rm -rf *.o
	rm -rf $(TARGET)