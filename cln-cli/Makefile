CC=gcc
CFLAGS=-Wall -Wextra -O2
TARGET=cln-cli

$(TARGET): cln-cli.c
	$(CC) $(CFLAGS) -o $(TARGET) cln-cli.c

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: clean install uninstall