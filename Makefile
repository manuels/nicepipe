nicepipe:
	gcc nice.c util.c callbacks.c nicepipe.c -g `pkg-config --cflags --libs nice` -o nicepipe

niceport:
	gcc nice.c util.c callbacks.c niceport.c -g `pkg-config --cflags --libs nice` -o niceport
