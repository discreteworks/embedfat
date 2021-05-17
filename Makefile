CC=gcc
INC=-I./src
CFLAGS=
DEPS=
DEMO=demo-fat
OBJ = src/fat.o src/ramdisk.o src/vfs.o demo/main.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(INC) $(CFLAGS)

${DEMO}: $(OBJ)
	$(CC) -o $@ $^ $(INC) $(CFLAGS)

clean :
	rm $(OBJ) ${DEMO}
