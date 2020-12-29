// kgsws' Doom ACE
//
// This file contains all required engine types and function prototypes.
// This file also contains all required includes.
// Some structures and constatns are modified for ACE ENGINE.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

//
// basic

#define SCREENWIDTH	320
#define SCREENHEIGHT	200
#define SCREENSIZE	(SCREENWIDTH*SCREENHEIGHT)

#define FRACBITS	16
#define FRACUNIT	(1 << 16)

typedef int32_t fixed_t;
typedef uint32_t angle_t;

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

typedef struct zoneblock_s
{
	int32_t size;
	void **user;
	int32_t tag;
	uint32_t id;
	struct zoneblock_s *next;
	struct zoneblock_s *prev;
} zoneblock_t;

typedef struct
{
	int32_t size;
	zoneblock_t block;
	zoneblock_t *rover;
} mainzone_t;

//
// tics

#define BT_ATTACK	1
#define BT_USE	2

typedef struct
{
	char forwardmove;
	char sidemove;
	short angleturn;
	short consistancy;
	uint8_t chatchar;
	uint8_t buttons;
} ticcmd_t;

typedef struct thinker_s
{
	struct thinker_s *prev;
	struct thinker_s *next;
	void *function;
} thinker_t;

//
// weapons

typedef struct
{
	struct state_s *state;
	int tics;
	fixed_t	sx;
	fixed_t	sy;
} pspdef_t;

//
// player

#define	NUMPSPRITES	2
#define MAXPLAYERS	4

#define NUMWEAPONS	9	// original

#define WEAPONBOTTOM	(128 << FRACBITS)
#define WEAPONTOP	(32 << FRACBITS)
#define WEAPONRAISE	(6 << FRACBITS)

#define USERANGE	(64 << FRACBITS)
#define MELEERANGE	(64 << FRACBITS)
#define MISSILERANGE	(2048 << FRACBITS)
#define AIMRANGE	(1024 << FRACBITS)

enum
{
	PST_LIVE,
	PST_DEAD,
	PST_REBORN
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

typedef struct player_s
{ // modified
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
	uint32_t __free_0;
	uint32_t __free_1;
	uint32_t __free_2;
	struct dextra_playerclass_s *class;
	uint32_t readyweapon;
	uint32_t pendingweapon;
	uint32_t weaponowned[4]; // now a bit fields; up to 128 weapons
	uint32_t __free_3[5];
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
		uint32_t same;
	};
	int32_t handle;
	uint32_t position;
	uint32_t size;
} lumpinfo_t;

typedef struct
{
	uint32_t id;
	uint32_t numlumps;
	uint32_t diroffs;
} wadhead_t;

typedef struct
{
	uint32_t offset;
	uint32_t size;
	union
	{
		uint8_t name[8];
		uint64_t wame;
	};
} wadlump_t;

//
// info

#define NUMMOBJTYPES	137
#define NUMSTATES	967

typedef struct
{ // extra mobjinfo
	uint32_t state_heal; // TODO: fix A_VileChase
	uint32_t state_death_fire;
	uint32_t state_death_ice;
	uint32_t state_death_disintegrate;
} mobj_extra_info_t;

typedef struct
{ // modified
	int16_t doomednum;
	uint16_t spawnid;
	uint32_t spawnstate;
	int32_t spawnhealth;
	uint32_t seestate;
	uint16_t seesound;
	uint16_t attacksound;
	uint32_t reactiontime;
	mobj_extra_info_t *extrainfo;
	uint32_t painstate;
	uint16_t painchance;
	uint16_t activesound;
	uint16_t painsound;
	uint16_t deathsound;
	uint32_t meleestate;
	uint32_t missilestate;
	uint32_t deathstate;
	uint32_t xdeathstate;
	union decorate_extra_info_u *extra;
	int32_t speed;
	uint32_t radius;
	uint32_t height;
	uint32_t mass;
	int32_t damage;
	uint32_t flags2;
	uint32_t flags;
	uint32_t raisestate;
} mobjinfo_t;

typedef struct state_s
{ // modified
	uint16_t frame;
	uint16_t sprite;
	void *extra;
	int32_t tics;
	union
	{
		void *action;
		void (*acp)(struct mobj_s*) __attribute((regparm(2)));
	};
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
	int16_t type;
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
#define MF_SLIDE	0x2000	// used in hook
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
// more flags
#define	MF_NOTELEPORT	0x4000000
#define	MF_ISMONSTER	0x8000000
#define	MF_BOSS		0x10000000	// also used in hooks!
#define MF_SEEKERMISSILE	0x20000000
#define MF_RANDOMIZE	0x40000000	// TODO: implement everywhere (projectiles only)
#define MF_NOTARGET	0x80000000	// TODO: replace MT_VILE check in P_DamageMobj

// new flags
#define	MF2_INACTIVE	0x00000001
#define MF2_TELESTOMP	0x00000002	// used in hooks!


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
{ // modified
	thinker_t thinker;
	fixed_t x;
	fixed_t y;
	fixed_t z;
	struct mobj_s *snext;
	struct mobj_s *sprev;
	angle_t angle;
	int32_t dehacked_sprite;
	uint16_t frame;
	uint16_t sprite;
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
	mapthing_t spawnpoint;
	uint16_t spawnz;
	uint8_t animation;
	uint8_t tid;
	uint8_t special;
	uint8_t arg0;
	union
	{
		struct
		{
			uint8_t arg1;
			uint8_t arg2;
			uint8_t arg3;
			uint8_t arg4;
		};
		uint32_t args;
	};
	uint32_t id;
	uint32_t flags2;
	void *translation;
} __attribute__((packed)) mobj_t; // must be divisible by 4

//
// render

#define FF_FULLBRIGHT	0x8000
#define FF_FRAMEMASK	0x7FFF

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

typedef struct
{
	int32_t x, y, p;
} texpatch_t;

typedef struct
{
	union
	{
		char name[8];
		uint64_t wame;
	};
	uint16_t width, height;
	uint16_t count;
	texpatch_t patch[];
} __attribute__((packed)) texture_t;


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

typedef struct
{
	uint32_t rotate;
	uint16_t lump[8];
	uint8_t flip[8];
} spriteframe_t;

typedef struct
{
	uint32_t count;
	spriteframe_t *frames;
} spritedef_t;

typedef struct
{
	seg_t *curline;
	int32_t x1, x2;
	fixed_t scale1, scale2, scalestep;
	int32_t silhouette;
	fixed_t bsilheight;
	fixed_t tsilheight;
	int16_t *sprtopclip;
	int16_t *sprbottomclip;
	int16_t *maskedtexturecol;
} drawseg_t;

typedef struct
{
	fixed_t height;
	int32_t picnum;
	int32_t lightlevel;
	int32_t minx, maxx;
	uint8_t pad0;
	uint8_t top[SCREENWIDTH];
	uint8_t pad1[2];
	uint8_t bottom[SCREENWIDTH];
	uint8_t pad2;
} __attribute__((packed)) visplane_t;

//
// sound

typedef struct
{
	char *name;
	uint32_t single;
	uint32_t priority;
	void *link;
	int32_t pitch;
	uint32_t volume;
	void *data;
	int32_t used;
	int32_t lumpnum;
} sfx_info_t;

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
// stuff
typedef union
{
	uint32_t ex;
	uint16_t x;
	struct
	{
		uint8_t l, h;
	};
} reg32_t;

typedef struct
{
	reg32_t edi;
	reg32_t esi;
	reg32_t ebp;
	uint32_t zero0;
	reg32_t ebx;
	reg32_t edx;
	reg32_t ecx;
	reg32_t eax;
	uint16_t flags;
	uint16_t es;
	uint16_t ds;
	uint16_t fs;
	uint16_t gs;
	uint16_t ip;
	uint16_t cs;
	uint16_t sp;
	uint16_t ss;
} dpmi_regs_t;

//
// extra special variables
extern fixed_t *viewx;
extern fixed_t *viewy;
extern fixed_t *viewz;

//
// Doom Engine Functions
// Since Doom uses different calling conventions, most functions have to use special GCC attribute.
// Even then functions with more than two arguments need another workaround. This is done in 'asm.S'.

void dos_exit(); // extra for early error handling
void I_Error(char*, ...); // This function is variadic. No attribute required.
int doom_sprintf(char*, const char*, ...);
int doom_printf(const char*, ...);

// SDK
int write(int,void*,uint32_t) __attribute((regparm(2)));
int read(int,void*,uint32_t) __attribute((regparm(2)));
int lseek(int,uint32_t,int) __attribute((regparm(2)));
void doom_free(void*) __attribute((regparm(2)));
void *doom_malloc(uint32_t) __attribute((regparm(2)));

void dpmi_irq(int,dpmi_regs_t*);

// ASM hooks - never called directly
void medusa_cache_fix();

// to be removed
void P_InitSwitchList();

// stuff
void I_SetPalette(void*) __attribute((regparm(2)));
void I_FinishUpdate();
void I_UpdateBox(int x, int y, int w, int h) __attribute((regparm(2)));
int32_t M_Random();
int32_t P_Random();
void HU_Init();
void ST_Init();
void D_StartTitle();

// menu.c
int M_StringHeight(const char *) __attribute((regparm(2)));
int M_StringWidth(const char *) __attribute((regparm(2)));
void M_WriteText(int x, int y, const char *txt) __attribute((regparm(2)));
void M_StartControlPanel();
void M_StartMessage(char*, void*, uint32_t) __attribute((regparm(2)));

// s_sound.c
void S_StartSound(void *origin, int id) __attribute((regparm(2)));

// z_zone.c
void *Z_Malloc(int size, int tag, void *user) __attribute((regparm(2)));
void Z_Free(void *ptr) __attribute((regparm(2)));

// w_wad.c
int W_CheckNumForName(const char *name) __attribute((regparm(2)));
int W_GetNumForName(const char *name) __attribute((regparm(2)));
void *W_CacheLumpNum(int lump, int tag) __attribute((regparm(2)));
void *W_CacheLumpName(const char *name, int tag) __attribute((regparm(2)));
int W_LumpLength(int lump) __attribute((regparm(2)));
void W_ReadLump(int lump, void *dst) __attribute((regparm(2)));

// v_video.c
void V_DrawPatchDirect(int x, int y, int zero, patch_t *patch) __attribute((regparm(2)));
void V_DrawPatch(int x, int y, int zero, patch_t *patch) __attribute((regparm(2)));

// g_game.c
void G_BuildTiccmd(ticcmd_t*) __attribute((regparm(2)));
void G_ExitLevel();
void G_DoPlayDemo();

// p_mobj.c
mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, uint32_t type) __attribute((regparm(2)));
void P_SpawnPlayer(void *mt) __attribute((regparm(2)));

// render
void R_GenerateLookup(int) __attribute((regparm(2)));
void R_GenerateComposite(int) __attribute((regparm(2)));
void R_InstallSpriteLump(int32_t,uint32_t,uint32_t,uint32_t) __attribute((regparm(2)));
void R_RenderPlayerView(player_t*) __attribute((regparm(2)));
void R_SetupFrame(player_t*) __attribute((regparm(2)));
void R_DrawPlayerSprites();
void R_ExecuteSetViewSize();
void R_InitLightTables();
void R_InitSkyMap();
angle_t R_PointToAngle(fixed_t,fixed_t) __attribute((regparm(2)));

static inline angle_t R_PointToAngle2(fixed_t x0, fixed_t y0, fixed_t x1, fixed_t y1)
{
	*viewx = x0;
	*viewy = y0;
	return R_PointToAngle(x1, y1);
}

// p_ stuff
void P_SpawnSpecials();
void P_SetThingPosition(mobj_t*) __attribute((regparm(2)));
void P_UnsetThingPosition(mobj_t*) __attribute((regparm(2)));
void P_DamageMobj(mobj_t*, mobj_t*, mobj_t*, int) __attribute((regparm(2)));
void P_RemoveMobj(mobj_t*) __attribute((regparm(2)));
void P_PlayerInSpecialSector(player_t*) __attribute((regparm(2)));
void P_TouchSpecialThing(mobj_t*, mobj_t*) __attribute((regparm(2)));
void P_ChangeSwitchTexture(line_t*,int) __attribute((regparm(2)));
void P_AddThinker(thinker_t*) __attribute((regparm(2)));
void P_RemoveThinker(thinker_t*) __attribute((regparm(2)));
int P_ChangeSector(sector_t*,int) __attribute((regparm(2)));
void P_SpawnPuff(fixed_t,fixed_t,fixed_t) __attribute((regparm(2)));
uint32_t P_TryMove(mobj_t*,fixed_t,fixed_t) __attribute((regparm(2)));
void P_NoiseAlert(mobj_t*,mobj_t*) __attribute((regparm(2)));
void P_LineAttack(mobj_t*,angle_t,fixed_t,fixed_t,int32_t) __attribute((regparm(2)));
fixed_t P_AimLineAttack(mobj_t*,angle_t,fixed_t) __attribute((regparm(2)));

// p_ height search
fixed_t P_FindLowestCeilingSurrounding(sector_t*) __attribute((regparm(2)));

// math
fixed_t FixedDiv(fixed_t, fixed_t) __attribute((regparm(2)));
#define FixedMul(a,b)	(((uint64_t)(a) * (uint64_t)(b)) >> FRACBITS)

static inline fixed_t P_AproxDistance(fixed_t dx, fixed_t dy)
{
	dx = abs(dx);
	dy = abs(dy);
	if(dx < dy)
		return dx + dy - (dx >> 1);
	return dx + dy - (dy >> 1);
}

