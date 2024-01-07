CC = gcc
CFLAGS = -Wall -Wextra -g
FRAMEWORKS = -framework Carbon -framework CoreFoundation -framework CoreGraphics
TARGET = macos-keylogger
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(FRAMEWORKS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean
