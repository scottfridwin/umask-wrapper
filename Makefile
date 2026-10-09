CC ?= gcc
CFLAGS ?= -Os -Wall -Wextra -Werror
PLATFORMS = linux/amd64,linux/arm64,linux/arm/v7,linux/386

.PHONY: all test dist clean

all: umask-wrapper

umask-wrapper: umask-wrapper.c
	$(CC) -static $(CFLAGS) -o $@ $<

test: umask-wrapper
	sh test.sh ./umask-wrapper

# Release binaries for every platform, each tested under emulation (needs Docker Buildx + QEMU)
dist:
	docker buildx build --platform $(PLATFORMS) -f Dockerfile.build -o dist .

clean:
	rm -rf umask-wrapper dist
