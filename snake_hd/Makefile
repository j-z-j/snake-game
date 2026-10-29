CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -finput-charset=UTF-8
LDLIBS  ?= -lwinmm
TARGET   = snake_hd.exe
OBJS     = game.o render.o main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

game.o: game.c game.h
	$(CC) $(CFLAGS) -c game.c

render.o: render.c render.h game.h
	$(CC) $(CFLAGS) -c render.c

main.o: main.c game.h render.h
	$(CC) $(CFLAGS) -c main.c

clean:
	del /Q $(OBJS) $(TARGET) 2>nul

.PHONY: all clean
