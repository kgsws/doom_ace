// kgsws' ACE Engine
////
// This file contains all available function prototypes.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

//
// variables

extern uint8_t **destscreen;

//
// Doom Engine Functions
// Since Doom uses different calling conventions, most functions have to use special GCC attribute.
// Even then functions with more than two arguments need another workaround. This is done in 'asm.S'.

#define O_RDONLY	0
#define O_WRONLY	1
#define O_RDWR	2
#define	O_CREAT	0x20
#define O_TRUNC	0x40
#define O_BINARY	0x200

#define doom_open(n,f,...)	doom_open_X(n, (f) | O_BINARY, __VA_ARGS__)

// asm.S
void dos_exit(uint32_t) __attribute((regparm(2)));

// Variadic functions require no attributes.
void I_Error(uint8_t*, ...);
int doom_sprintf(uint8_t*, const uint8_t*, ...);
int doom_printf(const uint8_t*, ...);
int doom_open_X(const uint8_t *, uint32_t, ...);

// SDK
void doom_close(int) __attribute((regparm(2)));
int doom_write(int,void*,uint32_t) __attribute((regparm(2)));
int doom_read(int,void*,uint32_t) __attribute((regparm(2)));
int doom_lseek(int,int,int) __attribute((regparm(2)));
void doom_free(void*) __attribute((regparm(2)));
void *doom_malloc(uint32_t) __attribute((regparm(2)));

//
void I_FinishUpdate() __attribute((regparm(2)));

