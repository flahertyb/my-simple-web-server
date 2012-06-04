TARGET = 3600http

$(TARGET): $(TARGET).c
	gcc -std=c99 -pthread -O0 -g -lm -Wall `pkg-config --cflags --libs glib-2.0` -pedantic -Wextra -o $@ $<

all: $(TARGET)

test: all
	./test

clean:
	rm -f $(TARGET)
	make

