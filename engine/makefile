program = code
OBJ = asm.o main.o utils.o bugfix.o wadfile.o config.o controls.o ldr_texture.o ldr_flat.o ldr_sprite.o textpars.o filebuf.o think.o generic.o sound.o mobj.o player.o decorate.o action.o dehacked.o animate.o inventory.o weapon.o map.o hitscan.o special.o sight.o draw_c.o draw_a.o polyobj.o render.o extra3d.o stbar.o menu.o saveload.o cheat.o rng.o wipe.o demo.o doom_vars.o
CC = gcc -m32 -march=i386
OPT = -O2 -g0
BOTH = -fno-pie
CFLAGS = ${BOTH} ${OPT} -mgeneral-regs-only -fdata-sections -ffunction-sections -fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector -fno-reorder-blocks-and-partition -fcf-protection=none -Wno-address-of-packed-member -U_FORTIFY_SOURCE

build: ${program}.lmp

clean:
	rm -f *.o ${program} ${program}.rel ${program}.bin

${program}.lmp: ${OBJ}
	${CC} ${OBJ} ${LIBS} ${BOTH} -o ${program} -nostdlib -Wl,--gc-sections -Wl,--no-ld-generated-unwind-info -Wl,--no-dynamic-linker -Wl,--omagic -Wl,-T linker.ld
	objcopy -O binary --only-section=.text --only-section=.hooks --only-section=.rodata --only-section=.data ${program} ${program}.bin
	objcopy -O binary --only-section=.rel.dyn ${program} ${program}.rel
	./lumpgen.py ${program}.bin ${program}.rel ${program}.lmp

