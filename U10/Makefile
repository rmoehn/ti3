CFLAGS = -Wall -std=gnu99

test: crc
	rm test.crc
	cp test.bak test
	./crc calculate test
	xxd test.bak
	xxd test.crc

crc: crc.c errors.h
