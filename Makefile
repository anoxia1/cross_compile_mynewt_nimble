# Top-level Makefile — build NimBLE static library for Linux
# Usage: make [-jN]
#   make          — default build
#   make clean    — remove all build artifacts
#   make deploy   — build + scp to NanoPi (customize DST for your target)

DST ?= pi@192.168.137.39:/home/pi/

all:
	$(MAKE) -C porting/examples/linux

clean:
	$(MAKE) -C porting/examples/linux clean

deploy:
	$(MAKE) -C porting/examples/linux
	sshpass -p 'pi' scp -o StrictHostKeyChecking=no \
		porting/examples/linux/output/lib/libnimble.a $(DST)

.PHONY: all clean deploy
