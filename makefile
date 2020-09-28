program=loader
OBJ = loader.o
CC=gcc -m32 -nostdlib
CFLAGS=${OPT}

build: ${program}

clean:
	rm -f *.o ${program} ${program}.bin

${program}: ${OBJ}
	${CC} ${OBJ} ${LIBS} -o ${program} ${OPT}
	objcopy -O binary --only-section=.text ${program} ${program}.bin

