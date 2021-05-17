CC=gcc
INC=-I./src
CFLAGS=
DEPS=
OBJ = src/fat.o src/ramdisk.o src/vfs.o demo/main.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(INC) $(CFLAGS)

demo-fat: $(OBJ)
	$(CC) -o $@ $^ $(INC) $(CFLAGS)

clean :
	rm $(OBJ) demo-fat
