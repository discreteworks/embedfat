CC=gcc
CFLAGS=-I.
DEPS = fnctl.h ramdisk.h vfs.h
OBJ = fat.o ramdisk.o vfs.o demo/main.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

fat: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

clean :
	rm $(OBJ)
