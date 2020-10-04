// kgsws' Doom ACE
//
// This file contains all required engine types and function prototypes.
// This file also contains all required includes.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

//
// basic

#define SCREENWIDTH	320
#define SCREENHEIGHT	200

#define FRACBITS	16
#define FRACUNIT	(1 << 16)

typedef int32_t fixed_t;
typedef uint32_t angle_t;
typedef uint8_t byte;

//
// tables

#define ANGLETOFINESHIFT	19
#define FINEANGLES	8192
#define FINEMASK	(FINEANGLES - 1)

#define ANG45	0x20000000
#define ANG90	0x40000000
#define ANG180	0x80000000
#define ANG270	0xC0000000

#define SLOPERANGE	2048
#define SLOPEBITS	11
#define DBITS	(FRACBITS - SLOPEBITS)

//
// input

enum
{
	ev_keydown,
	ev_keyup,
	ev_mouse,
	ev_joystick
};

typedef struct
{
	uint32_t type;
	int32_t data1;
	int32_t data2;
	int32_t data3;
} event_t;

enum
{
	KEY_TAB = 0x09,
	KEY_ENTER = 0x0D,
	KEY_ESCAPE = 0x1B,
	KEY_BACKSPACE = 0x7F,
	KEY_LEFTARROW = 0xAC,
	KEY_UPARROW,
	KEY_RIGHTARROW,
	KEY_DOWNARROW,
	KEY_CTRL = 0x9D,
	KEY_SHIFT = 0xB6,
	KEY_ALT = 0xB8,
	KEY_F1 = 0xBB,
	KEY_F2,
	KEY_F3,
	KEY_F4,
	KEY_F5,
	KEY_F6,
	KEY_F7,
	KEY_F8,
	KEY_F9,
	KEY_F10,
	KEY_F11 = 0xD7,
	KEY_F12,
	KEY_PAUSE = 0xFF
};

//
// zone

enum
{
	PU_STATIC = 1,
	PU_SOUND,
	PU_MUSIC,
	PU_LEVEL = 50,
	PU_LEVELSPEC,
	PU_PURGELEVEL = 100,
	PU_CACHE
};

//
// states

typedef struct
{
	uint32_t sprite;
	int32_t frame;
	int32_t tics;
	uint32_t action;
	int32_t nextstate;
	int32_t misc1, misc2;
} state_t;

//
// ticcmd

typedef struct
{
	char	forwardmove;
	char	sidemove;
	short	angleturn;
	short	consistancy;
	byte	chatchar;
	byte	buttons;
} ticcmd_t;

//
// render

typedef struct
{
	uint16_t width, height;
	int16_t x, y;
	uint32_t offs[];
} patch_t;

//
// weapons

typedef struct
{
	uint32_t ammo;
	uint32_t upstate;
	uint32_t downstate;
	uint32_t readystate;
	uint32_t atkstate;
	uint32_t flashstate;
} weaponinfo_t;

typedef struct
{
	uint32_t state;
	int tics;
	fixed_t	sx;
	fixed_t	sy;
} pspdef_t;

//
// player

#define	NUMPSPRITES	2
#define MAXPLAYERS	4

enum
{
	PST_LIVE,
	PST_DEAD,
	PST_REBORN
};

enum
{
	wp_fist,
	wp_pistol,
	wp_shotgun,
	wp_chaingun,
	wp_missile,
	wp_plasma,
	wp_bfg,
	wp_chainsaw,
	wp_supershotgun,
	NUMWEAPONS,
	// No pending weapon change.
	wp_nochange
};

enum
{
	pw_invulnerability,
	pw_strength,
	pw_invisibility,
	pw_ironfeet,
	pw_allmap,
	pw_infrared,
	NUMPOWERS
};

enum
{
	it_bluecard,
	it_yellowcard,
	it_redcard,
	it_blueskull,
	it_yellowskull,
	it_redskull,
	NUMCARDS
};

enum
{
	am_clip,	// Pistol / chaingun ammo.
	am_shell,	// Shotgun / double barreled shotgun.
	am_cell,	// Plasma rifle, BFG.
	am_misl,	// Missile launcher.
	NUMAMMO,
	am_noammo	// Unlimited for chainsaw / fist.	
};

typedef struct
{
	uint32_t *mo; // TODO
	uint32_t playerstate;
	ticcmd_t cmd;
	fixed_t viewz;
	fixed_t viewheight;
	fixed_t deltaviewheight;
	fixed_t bob;
	int health;
	int armorpoints;
	int armortype;
	int powers[NUMPOWERS];
	uint32_t cards[NUMCARDS];
	uint32_t backpack;
	int frags[MAXPLAYERS];
	uint32_t readyweapon;
	uint32_t pendingweapon;
	uint32_t weaponowned[NUMWEAPONS];
	int ammo[NUMAMMO];
	int maxammo[NUMAMMO];
	int attackdown;
	int usedown;
	int cheats;
	int refire;
	int killcount;
	int itemcount;
	int secretcount;
	char *message;
	int damagecount;
	int bonuscount;
	uint32_t *attacker; // TODO
	int extralight;
	int fixedcolormap;
	int colormap;
	pspdef_t psprites[NUMPSPRITES];
	uint32_t didsecret;
} player_t;

//
// sound

typedef enum
{
	sfx_None,
	sfx_pistol,
	sfx_shotgn,
	sfx_sgcock,
	sfx_dshtgn,
	sfx_dbopn,
	sfx_dbcls,
	sfx_dbload,
	sfx_plasma,
	sfx_bfg,
	sfx_sawup,
	sfx_sawidl,
	sfx_sawful,
	sfx_sawhit,
	sfx_rlaunc,
	sfx_rxplod,
	sfx_firsht,
	sfx_firxpl,
	sfx_pstart,
	sfx_pstop,
	sfx_doropn,
	sfx_dorcls,
	sfx_stnmov,
	sfx_swtchn,
	sfx_swtchx,
	sfx_plpain,
	sfx_dmpain,
	sfx_popain,
	sfx_vipain,
	sfx_mnpain,
	sfx_pepain,
	sfx_slop,
	sfx_itemup,
	sfx_wpnup,
	sfx_oof,
	sfx_telept,
	sfx_posit1,
	sfx_posit2,
	sfx_posit3,
	sfx_bgsit1,
	sfx_bgsit2,
	sfx_sgtsit,
	sfx_cacsit,
	sfx_brssit,
	sfx_cybsit,
	sfx_spisit,
	sfx_bspsit,
	sfx_kntsit,
	sfx_vilsit,
	sfx_mansit,
	sfx_pesit,
	sfx_sklatk,
	sfx_sgtatk,
	sfx_skepch,
	sfx_vilatk,
	sfx_claw,
	sfx_skeswg,
	sfx_pldeth,
	sfx_pdiehi,
	sfx_podth1,
	sfx_podth2,
	sfx_podth3,
	sfx_bgdth1,
	sfx_bgdth2,
	sfx_sgtdth,
	sfx_cacdth,
	sfx_skldth,
	sfx_brsdth,
	sfx_cybdth,
	sfx_spidth,
	sfx_bspdth,
	sfx_vildth,
	sfx_kntdth,
	sfx_pedth,
	sfx_skedth,
	sfx_posact,
	sfx_bgact,
	sfx_dmact,
	sfx_bspact,
	sfx_bspwlk,
	sfx_vilact,
	sfx_noway,
	sfx_barexp,
	sfx_punch,
	sfx_hoof,
	sfx_metal,
	sfx_chgun,
	sfx_tink,
	sfx_bdopn,
	sfx_bdcls,
	sfx_itmbk,
	sfx_flame,
	sfx_flamst,
	sfx_getpow,
	sfx_bospit,
	sfx_boscub,
	sfx_bossit,
	sfx_bospn,
	sfx_bosdth,
	sfx_manatk,
	sfx_mandth,
	sfx_sssit,
	sfx_ssdth,
	sfx_keenpn,
	sfx_keendt,
	sfx_skeact,
	sfx_skesit,
	sfx_skeatk,
	sfx_radio,
	NUMSFX
} sfxenum_t;

//
// Doom Engine Functions
// Since Doom uses different calling conventions, most functions have to use special GCC attribute.
// Even then functions with more than two arguments need another workaround. This is done in 'asm.S'.

void I_Error(char*, ...); // This function is variadic. No attribute required.
//int sprintf(char*, ...); // from stdio.h

// stuff
uint8_t M_Random();
uint8_t P_Random();

// menu.c
int M_StringHeight(const char *) __attribute((regparm(1)));
int M_StringWidth(const char *) __attribute((regparm(1)));
void M_WriteText(int x, int y, const char *txt) __attribute((regparm(2)));
void M_StartControlPanel();

// s_sound.c
void S_StartSound(void *origin, int id) __attribute((regparm(2)));

// z_zone.c
void *Z_Malloc(int size, int tag, void *user) __attribute((regparm(2)));
void Z_Free(void *ptr) __attribute((regparm(1)));

// w_wad.c
int W_GetNumForName(char *name) __attribute((regparm(1)));
void *W_CacheLumpNum(int lump, int tag) __attribute((regparm(2)));
void *W_CacheLumpName(char *name, int tag) __attribute((regparm(2)));

// v_video.c
void V_DrawPatchDirect(int x, int y, int zero, patch_t *patch) __attribute((regparm(2)));

// g_game.c
void G_BuildTiccmd(ticcmd_t*) __attribute((regparm(1)));

// render
void R_RenderPlayerView(player_t*) __attribute((regparm(1)));
void R_SetupFrame(player_t*) __attribute((regparm(1)));
void R_DrawPlayerSprites();

// math
fixed_t FixedDiv(fixed_t, fixed_t) __attribute((regparm(2)));

