CC = gcc

main: raytrace.c
	$(CC) $? -o $@