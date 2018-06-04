clean:
	rm main

build: main.c
	gcc main.c -o main -I/usr/X11/include -I/usr/X11/include/X11 -L/usr/X11/lib -L/usr/X11/lib/X11 -lX11

run: build
	./main

run-coordinates: build
	./main -2 0 -1 1
