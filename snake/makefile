program=snake
OBJ = asm.o main.o utils.o snake.o
CC=gcc -m32 -march=i386
OPT=-O2 -g0
BOTH=--static-pie
CFLAGS=${BOTH} ${OPT} -fpie -fPIE -fdata-sections -ffunction-sections -fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables

build: ${program}

clean:
	rm -f *.o ${program} ${program}.bin

${program}: ${OBJ}
	${CC} ${OBJ} ${LIBS} ${BOTH} -o ${program} -nostdlib -Wl,--gc-sections -Wl,--no-ld-generated-unwind-info -Wl,--omagic -Wl,-T linker.ld
	objcopy -O binary --only-section=.text --only-section=.rodata --only-section=.data ${program} ${program}.bin
