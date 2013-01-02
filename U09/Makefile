CFLAGS = -Wall -std=gnu99

http-server: http-server.c errors.h
	$(CC) $(CFLAGS) -o http-server http-server.c errors.h `pkg-config --cflags --libs glib-2.0`
