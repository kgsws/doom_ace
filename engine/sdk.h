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
extern struct mobjinfo_s *mobjinfo;
extern struct state_s *states;
extern struct weaponinfo_s *weaponinfo;

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

// stuff
struct player_s;
struct patch_s;

// asm.S
void dos_exit(uint32_t) __attribute((regparm(2)));

// Variadic functions require no attributes.
void I_Error(uint8_t*, ...);
int32_t doom_printf(const uint8_t*, ...);
int32_t doom_sprintf(uint8_t*, const uint8_t*, ...);
int32_t doom_sscanf(const uint8_t*, const uint8_t*, ...);
int32_t doom_open(const uint8_t *, uint32_t, ...);

// SDK
void doom_close(int32_t) __attribute((regparm(2)));
int32_t doom_write(int32_t,void*,uint32_t) __attribute((regparm(2)));
int32_t doom_read(int32_t,void*,uint32_t) __attribute((regparm(2)));
int32_t doom_lseek(int32_t,int32_t,int32_t) __attribute((regparm(2)));
void doom_free(void*) __attribute((regparm(2)));
void *doom_malloc(uint32_t) __attribute((regparm(2)));
void *doom_realloc(void*,uint32_t) __attribute((regparm(2)));

// st_stuff
void ST_Init() __attribute((regparm(2)));

// r_main
void R_RenderPlayerView(struct player_s*) __attribute((regparm(2)));

// v_video
void V_DrawPatchDirect(int32_t, int32_t, uint32_t, struct patch_s*) __attribute((regparm(2)));

// w_wad
int32_t W_CheckNumForName(uint8_t *name) __attribute((regparm(2)));
void *W_CacheLumpName(uint8_t *name, uint32_t tag) __attribute((regparm(2)));
void *W_CacheLumpNum(int32_t lump, uint32_t tag) __attribute((regparm(2)));
uint32_t W_LumpLength(int32_t lump) __attribute((regparm(2)));
void W_ReadLump(int32_t lump, void *dst) __attribute((regparm(2)));

// z_zone
void *Z_Malloc(uint32_t size, uint32_t tag, void *owner) __attribute((regparm(2)));
void Z_Free(void *ptr) __attribute((regparm(2)));

