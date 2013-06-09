all:
	gcc util.c callbacks.c nicepipe.c -g `pkg-config --cflags --libs nice` -o nicepipe
