CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O3
LDFLAGS =
FRAMEWORKS = -framework Carbon -framework CoreFoundation -framework CoreGraphics
TARGET = macos-keylogger
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: $(TARGET)

run: all
	./$(TARGET) -o ~/keylog.bin -f -s

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(FRAMEWORKS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all run clean
