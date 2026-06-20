CC = gcc
CFLAGS = -Wall -Wextra -O3 $(shell pkg-config --cflags gtk+-3.0 gtk-layer-shell-0 glib-2.0)
LIBS = $(shell pkg-config --libs gtk+-3.0 gtk-layer-shell-0 glib-2.0)

TARGET = osd
SRC = osd.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
