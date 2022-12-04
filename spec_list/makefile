program = test
OBJ = main.o
LIBS =
OPT = -O2 -g
CC = gcc
CFLAGS = ${OPT}

build: ${program}

clean:
	rm -f *.o ${program}

${program}: ${OBJ}
	${CC} ${OBJ} ${LIBS} -o ${program} ${OPT}

