program = exploit
OBJ = exploit.o
CC=gcc -m32 -nostdlib
CFLAGS=${OPT}

build: ${program}

clean:
	rm -f *.o ${program} ${program}.bin

${program}: ${OBJ}
	${CC} ${OBJ} ${LIBS} -o ${program} -Wl,--section-start=.text=0x1F5B20
	objcopy -O binary --only-section=.text ${program} ${program}.bin

