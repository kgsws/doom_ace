// kgsws' Doom ACE
//
// This file contains all required engine types and function prototypes.
// This file also contains all required includes.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

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
// tics

typedef struct
{
	char	forwardmove;
	char	sidemove;
	short	angleturn;
	short	consistancy;
	byte	chatchar;
	byte	buttons;
} ticcmd_t;

typedef struct thinker_s
{
	struct thinker_s *prev;
	struct thinker_s *next;
	void *function;
} thinker_t;

//
// render

enum
{
	ST_HORIZONTAL,
	ST_VERTICAL,
	ST_POSITIVE,
	ST_NEGATIVE
};

enum
{
	BOXTOP,
	BOXBOTTOM,
	BOXLEFT,
	BOXRIGHT
};

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
	struct mobj_s *mo;
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
	struct mobj_s *attacker;
	int extralight;
	int fixedcolormap;
	int colormap;
	pspdef_t psprites[NUMPSPRITES];
	uint32_t didsecret;
} player_t;

//
// wad

typedef struct
{
	union
	{
		char name[8];
		uint64_t wame;
	};
	int handle;
	uint32_t position;
	uint32_t size;
} lumpinfo_t;

//
// info

#define NUMMOBJTYPES	137

typedef struct
{
	uint32_t doomednum;
	uint32_t spawnstate;
	int32_t spawnhealth;
	uint32_t seestate;
	uint32_t seesound;
	uint32_t reactiontime;
	uint32_t attacksound;
	uint32_t painstate;
	uint32_t painchance;
	uint32_t painsound;
	uint32_t meleestate;
	uint32_t missilestate;
	uint32_t deathstate;
	uint32_t xdeathstate;
	uint32_t deathsound;
	int32_t speed;
	uint32_t radius;
	uint32_t height;
	uint32_t mass;
	int32_t damage;
	uint32_t activesound;
	uint32_t flags;
	uint32_t raisestate;
} mobjinfo_t;

typedef struct
{
	uint32_t sprite;
	int32_t frame;
	int32_t tics;
	void *action;
	uint32_t nextstate;
	int32_t misc1;
	int32_t misc2;
} state_t;

//
// map

#define MTF_EASY	1
#define MTF_MEDIUM	2
#define MTF_HARD	4
#define MTF_AMBUSH	8
#define MTF_MULTIPLR	16

#define ML_BLOCKING	1
#define ML_BLOCKMONSTERS	2
#define ML_TWOSIDED	4
#define ML_DONTPEGTOP	8
#define ML_DONTPEGBOTTOM	16
#define ML_SECRET	32
#define ML_SOUNDBLOCK	64
#define ML_DONTDRAW	128
#define ML_MAPPED	256

enum
{
	ML_LABEL,
	ML_THINGS,
	ML_LINEDEFS,
	ML_SIDEDEFS,
	ML_VERTEXES,
	ML_SEGS,
	ML_SSECTORS,
	ML_NODES,
	ML_SECTORS,
	ML_REJECT,
	ML_BLOCKMAP,
	ML_BEHAVIOR // [kg] fun stuff
};

typedef struct
{
	int16_t x, y;
	uint16_t angle;
	uint16_t type;
	uint16_t flags;
} mapthing_t;

typedef struct
{
	thinker_t unused;
	fixed_t x, y, z;
} degenmobj_t;

typedef struct
{
	fixed_t x, y;
} vertex_t;

typedef struct
{
	fixed_t floorheight;
	fixed_t ceilingheight;
	uint16_t floorpic;
	uint16_t ceilingpic;
	union
	{
		uint16_t lightlevel;
		struct
		{	// [kg] fun stuff
			uint8_t light;
			uint8_t colormap;
		};
	};
	uint16_t special;
	uint16_t tag;
	int32_t sndtraversed;
	struct mobj_s *soundtarget;
	int32_t blockbox[4];
	degenmobj_t soundorg;
	int validcount;
	struct mobj_s *thinglist;
	void *specialdata;
	uint32_t linecount;
	struct line_s **lines;
} __attribute__((packed)) sector_t;

typedef struct line_s
{
	vertex_t *v1, *v2;
	fixed_t dx, dy;
	uint16_t flags;
	uint16_t special;
	union
	{
		uint16_t tag;
		struct
		{	// [kg] fun stuff
			uint8_t arg0;
			uint8_t unused;
		};
	};
	int16_t sidenum[2];
	fixed_t bbox[4];
	uint32_t slopetype;
	sector_t *frontsector;
	sector_t *backsector;
	int validcount;
	union
	{
		void *specialdata;
		struct
		{	// [kg] fun stuff
			uint8_t arg1;
			uint8_t arg2;
			uint8_t arg3;
			uint8_t arg4;
		};
	};
} __attribute__((packed)) line_t;

typedef struct
{
	fixed_t textureoffset;
	fixed_t rowoffset;
	uint16_t toptexture;
	uint16_t bottomtexture;
	uint16_t midtexture;
	sector_t *sector;
} __attribute__((packed)) side_t;

typedef struct subsector_s
{
	sector_t *sector;
	uint16_t numlines;
	uint16_t firstline;
} subsector_t;

//
// mobj

#define MF_SPECIAL	0x1
#define MF_SOLID	0x2
#define MF_SHOOTABLE	0x4
#define MF_NOSECTOR	0x8
#define MF_NOBLOCKMAP	0x10
#define MF_AMBUSH	0x20
#define MF_JUSTHIT	0x40
#define MF_JUSTATTACKED	0x80
#define MF_SPAWNCEILING	0x100
#define MF_NOGRAVITY	0x200
#define MF_DROPOFF	0x400
#define MF_PICKUP	0x800
#define MF_NOCLIP	0x1000
#define MF_SLIDE	0x2000
#define MF_FLOAT	0x4000
#define MF_TELEPORT	0x8000
#define MF_MISSILE	0x10000
#define MF_DROPPED	0x20000
#define MF_SHADOW	0x40000
#define MF_NOBLOOD	0x80000
#define MF_CORPSE	0x100000
#define MF_INFLOAT	0x200000
#define MF_COUNTKILL	0x400000
#define MF_COUNTITEM	0x800000
#define MF_SKULLFLY	0x1000000
#define MF_NOTDMATCH	0x2000000

// not used with rendering changes
#define MF_TRANSLATION	0xc000000
#define MF_TRANSSHIFT	26

typedef struct
{	// [kg] fun stuff
	// partialy overlaps mapthing_t
	int16_t x, y;
	uint16_t angle;
	uint16_t type;
	uint8_t special;
	union
	{
		uint8_t arg[5];
		struct
		{
			uint8_t arg0;
			uint32_t args;
		} __attribute__((packed));
	};
	uint8_t tag;
	uint8_t flags;
	void *translation;
} __attribute__((packed)) mobj_extra_t;

typedef struct mobj_s
{
	thinker_t thinker;
	fixed_t x;
	fixed_t y;
	fixed_t z;
	struct mobj_s *snext;
	struct mobj_s *sprev;
	angle_t angle;
	int32_t sprite;
	int32_t frame;
	struct mobj_s *bnext;
	struct mobj_s *bprev;
	struct subsector_s *subsector;
	fixed_t floorz;
	fixed_t ceilingz;
	fixed_t radius;
	fixed_t height;
	fixed_t momx;
	fixed_t momy;
	fixed_t momz;
	int validcount;
	uint32_t type;
	mobjinfo_t *info;
	int tics;
	state_t *state;
	uint32_t flags;
	int health;
	int movedir;
	int movecount;
	struct mobj_s *target;
	int reactiontime;
	int threshold;
	struct player_s *player;
	int lastlook;
	struct mobj_s *tracer;
	union
	{
		mapthing_t spawnpoint;
		mobj_extra_t extra; // [kg] fun stuff
	};
} __attribute__((packed)) mobj_t;

//
// render

#define FF_FULLBRIGHT	0x8000

#define LIGHTLEVELS	16
#define LIGHTSEGSHIFT	4
#define MAXLIGHTSCALE	0x30
#define	LIGHTSCALESHIFT	12
#define MAXLIGHTZ	128

typedef struct vissprite_s
{
	struct vissprite_s *prev;
	struct vissprite_s *next;
	int x1, x2;
	fixed_t gx, gy;
	fixed_t gz, gzt;
	fixed_t startfrac;
	fixed_t scale;
	fixed_t xiscale;
	fixed_t texturemid;
	int patch;
	void *colormap;
	union
	{
		uint32_t mobjflags;
		mobj_t *mo; // [kg] rendering changes
	};
} vissprite_t;

typedef struct
{
	vertex_t *v1, *v2;
	fixed_t offset;
	angle_t angle;
	side_t *sidedef;
	line_t *linedef;
	sector_t *frontsector;
	sector_t *backsector;
} seg_t;

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
int32_t M_Random();
int32_t P_Random();

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
int W_LumpLength(int lump) __attribute((regparm(1)));

// v_video.c
void V_DrawPatchDirect(int x, int y, int zero, patch_t *patch) __attribute((regparm(2)));

// g_game.c
void G_BuildTiccmd(ticcmd_t*) __attribute((regparm(1)));

// p_mobj.c
mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, uint32_t type) __attribute((regparm(2)));
void P_SpawnPlayer(void *mt) __attribute((regparm(1)));

// g_game.c
void G_ExitLevel();

// render
void R_RenderPlayerView(player_t*) __attribute((regparm(1)));
void R_SetupFrame(player_t*) __attribute((regparm(1)));
void R_DrawPlayerSprites();

// p_ stuff
void P_SpawnSpecials();
void P_SetThingPosition(mobj_t*) __attribute((regparm(1)));
void P_UnsetThingPosition(mobj_t*) __attribute((regparm(1)));
void P_DamageMobj(mobj_t*, mobj_t*, mobj_t*, int) __attribute((regparm(2)));
void P_SetMobjState(mobj_t*,int) __attribute((regparm(2)));
void P_PlayerInSpecialSector(player_t*) __attribute((regparm(1)));
void P_TouchSpecialThing(mobj_t*, mobj_t*) __attribute((regparm(2)));
void P_ChangeSwitchTexture(line_t*,int) __attribute((regparm(2)));
void P_AddThinker(thinker_t*) __attribute((regparm(1)));
void P_RemoveThinker(thinker_t*) __attribute((regparm(1)));
int P_ChangeSector(sector_t*,int) __attribute((regparm(2)));
void P_SpawnPuff(fixed_t,fixed_t,fixed_t) __attribute((regparm(2)));

// p_ height search
fixed_t P_FindLowestCeilingSurrounding(sector_t*) __attribute((regparm(1)));

// math
fixed_t FixedDiv(fixed_t, fixed_t) __attribute((regparm(2)));

