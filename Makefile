all:
	gcc main.c -g `pkg-config --cflags --libs nice` -o nicepipe
