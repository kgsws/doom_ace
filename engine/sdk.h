// kgsws' ACE Engine
////
// This file contains all available function prototypes.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

typedef union
{
	uint64_t u64;
	uint32_t u32[2];
	uint16_t u16[4];
	uint8_t u8[8];
	int64_t s64;
	int32_t s32[2];
	int16_t s16[4];
	int8_t s8[8];
} num64_t;

typedef union
{
	uint32_t u32;
	uint16_t u16[2];
	uint8_t u8[4];
	int32_t s32;
	int16_t s16[2];
	int8_t s8[4];
} num32_t;

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

#define doom_open_RD(n)	doom_open(n, O_RDONLY | O_BINARY)
#define doom_open_WR(n)	doom_open(n, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666)

// vars
extern uint32_t old_game_mode;

// asm.S
void dos_exit(uint32_t) __attribute((regparm(2),noreturn));

// word copy
static inline void dwcopy(void *dst, void *src, uint32_t count)
{
	asm volatile ("rep movsl" : "+S" (src), "+D" (dst), "+c" (count) : : "memory");
}

// Variadic functions require no attributes.
void I_Error(uint8_t*, ...) __attribute((noreturn));
int32_t doom_printf(const uint8_t*, ...);
int32_t doom_sprintf(uint8_t*, const uint8_t*, ...);
int32_t doom_sscanf(const uint8_t*, const uint8_t*, ...);
int32_t doom_open(const uint8_t *, uint32_t, ...);
int32_t doom_fprintf(void*, const uint8_t*, ...);

// SDK
void doom_close(int32_t) __attribute((regparm(2),no_caller_saved_registers));
int32_t doom_write(int32_t,void*,uint32_t) __attribute((regparm(2),no_caller_saved_registers));
int32_t doom_read(int32_t,void*,uint32_t) __attribute((regparm(2),no_caller_saved_registers));
int32_t doom_lseek(int32_t,int32_t,int32_t) __attribute((regparm(2),no_caller_saved_registers));
int32_t doom_filelength(int32_t) __attribute((regparm(2),no_caller_saved_registers));
void *doom_fopen(const uint8_t*,const uint8_t*) __attribute((regparm(2),no_caller_saved_registers));
void doom_fclose(void*) __attribute((regparm(2),no_caller_saved_registers));

void doom_free(void*) __attribute((regparm(2),no_caller_saved_registers));
void *doom_malloc(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void *doom_realloc(void*,uint32_t) __attribute((regparm(2),no_caller_saved_registers));

