program = hellorld
OBJ = hellorld.o
CC=gcc -m32 -nostdlib
CFLAGS=${OPT}

build: clean ${program}.wad

clean:
	rm -f *.o ${program} ${program}.bin ${program}.wad

${program}: ${OBJ}
	${CC} ${OBJ} -o ${program} -Wl,--section-start=.text=0x00034615

${program}.wad: ${program}
	objcopy -O binary --only-section=.text ${program} ${program}.bin
	./wadgen.py ${program}.bin ${program}.wad

