CC = gcc
CFLAGS = -static -O2
SRC = umask-wrapper.c
BIN = umask-wrapper

.PHONY: all clean test

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BIN)

test: $(BIN)
	APP_UMASK=0077 ./$(BIN) touch /tmp/testfile && ls -l /tmp/testfile
