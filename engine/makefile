program = code
OBJ = asm.o main.o utils.o bugfix.o wadfile.o ldr_texture.o ldr_flat.o ldr_sprite.o textpars.o filebuf.o sound.o mobj.o player.o decorate.o action.o dehacked.o animate.o inventory.o weapon.o map.o stbar.o saveload.o cheat.o
CC = gcc -m32 -march=i386
OPT = -O2 -g0
BOTH = -fno-pie
CFLAGS = ${BOTH} ${OPT} -mgeneral-regs-only -fdata-sections -ffunction-sections -fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector -Wno-address-of-packed-member

build: ${program}

clean:
	rm -f *.o ${program} ${program}.bin

${program}: ${OBJ}
	${CC} ${OBJ} ${LIBS} ${BOTH} -o ${program} -nostdlib -Wl,--gc-sections -Wl,--no-ld-generated-unwind-info -Wl,--no-dynamic-linker -Wl,--omagic -Wl,-T linker.ld
	objcopy -O binary --only-section=.text --only-section=.rel.dyn --only-section=.hooks --only-section=.rodata --only-section=.data ${program} ${program}.bin
