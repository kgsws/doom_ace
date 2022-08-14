// kgsws' ACE Engine
////
// This file contains all required engine types and function prototypes.

//
// basic

#define MAXWADFILES	20

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
// zone

#define PU_STATIC	1
#define PU_LEVEL	50
#define PU_LEVELSPEC	51
#define PU_CACHE	101

//
// tics

#define BT_ATTACK	1
#define BT_USE	2

typedef struct
{
	int8_t forwardmove;
	int8_t sidemove;
	int16_t angleturn;
	int16_t consistancy;
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

typedef struct weaponinfo_s
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
	struct state_s *state;
	int tics;
	fixed_t	sx;
	fixed_t	sy;
} pspdef_t;

//
// player

#define	NUMPSPRITES	2
#define MAXPLAYERS	4

#define NUMWEAPONS	9
#define NUMAMMO		4

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

typedef struct player_s
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
// video

typedef struct patch_s
{
	uint16_t width, height;
	int16_t x, y;
	uint32_t offs[];
} patch_t;

struct column_t;

//
// WAD

typedef struct
{
	union
	{
		uint8_t name[8];
		uint32_t same[2];
		uint64_t wame;
	};
	int32_t fd;
	uint32_t offset;
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
		uint32_t same[2];
		uint64_t wame;
	};
} wadlump_t;

//
// info

#define NUMMOBJTYPES	137
#define NUMSTATES	967

typedef struct mobjinfo_s
{
	int32_t doomednum;
	int32_t spawnstate;
	int32_t spawnhealth;
	int32_t seestate;
	int32_t seesound;
	int32_t reactiontime;
	int32_t attacksound;
	int32_t painstate;
	int32_t painchance;
	int32_t painsound;
	int32_t meleestate;
	int32_t missilestate;
	int32_t deathstate;
	int32_t xdeathstate;
	int32_t deathsound;
	int32_t speed;
	int32_t radius;
	int32_t height;
	int32_t mass;
	int32_t damage;
	int32_t activesound;
	int32_t flags;
	int32_t raisestate;
} mobjinfo_t;

typedef struct state_s
{
	int32_t sprite;
	int32_t frame;
	int32_t tics;
	void *action;
	int32_t nextstate;
	int32_t misc1;
	int32_t misc2;
} state_t;

//
// MAP

typedef struct
{
	int16_t x;
	int16_t y;
	int16_t angle;
	int16_t type;
	int16_t options;
} mapthing_t;

typedef struct
{
	fixed_t x, y;
} vertex_t;

typedef struct
{
	thinker_t thinker;
	fixed_t x;
	fixed_t y;
	fixed_t z;
} degenmobj_t;

typedef struct
{
	fixed_t floorheight;
	fixed_t ceilingheight;
	uint16_t floorpic;
	uint16_t ceilingpic;
	uint16_t lightlevel;
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
	uint16_t tag;
	int16_t sidenum[2];
	fixed_t bbox[4];
	uint32_t slopetype;
	sector_t *frontsector;
	sector_t *backsector;
	int validcount;
	void *specialdata;
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
// data

typedef struct texpatch_s
{
	int32_t originx;
	int32_t originy;
	uint32_t patch;
} __attribute__((packed)) texpatch_t;

typedef struct texture_s
{
	union
	{
		uint8_t name[8];
		uint32_t same[2];
		uint64_t wame;
	};
	uint16_t width;
	uint16_t height;
	uint16_t patchcount;
	texpatch_t patch[];
} __attribute__((packed)) texture_t;

//
// mobj

#define MF_SPECIAL	0x00000001
#define MF_SOLID	0x00000002
#define MF_SHOOTABLE	0x00000004
#define MF_NOSECTOR	0x00000008
#define MF_NOBLOCKMAP	0x00000010
#define MF_AMBUSH	0x00000020
#define MF_JUSTHIT	0x00000040
#define MF_JUSTATTACKED	0x00000080
#define MF_SPAWNCEILING	0x00000100
#define MF_NOGRAVITY	0x00000200
#define MF_DROPOFF	0x00000400
#define MF_PICKUP	0x00000800
#define MF_NOCLIP	0x00001000
#define MF_SLIDE	0x00002000
#define MF_FLOAT	0x00004000
#define MF_TELEPORT	0x00008000
#define MF_MISSILE	0x00010000
#define MF_DROPPED	0x00020000
#define MF_SHADOW	0x00040000
#define MF_NOBLOOD	0x00080000
#define MF_CORPSE	0x00100000
#define MF_INFLOAT	0x00200000
#define MF_COUNTKILL	0x00400000
#define MF_COUNTITEM	0x00800000
#define MF_SKULLFLY	0x01000000
#define MF_NOTDMATCH	0x02000000
#define MF_TRANSLATION0 0x04000000
#define MF_TRANSLATION1 0x08000000

typedef struct mobj_s
{
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
	mapthing_t spawnpoint;
	struct mobj_s *tracer;
} __attribute__((packed)) mobj_t;

// main.c
extern uint8_t *ldr_alloc_message;
void gfx_progress(int32_t step);
void *ldr_malloc(uint32_t size);

// i_video
void I_InitGraphics() __attribute((regparm(2)));
void I_FinishUpdate() __attribute((regparm(2)));

// m_argv
uint32_t M_CheckParm(uint8_t*) __attribute((regparm(2)));

// st_stuff
void ST_Init() __attribute((regparm(2)));

// r_data
void R_GenerateLookup(uint32_t) __attribute((regparm(2)));
void *R_GetColumn(uint32_t,uint32_t) __attribute((regparm(2)));

// r_main
void R_RenderPlayerView(struct player_s*) __attribute((regparm(2)));

// r_things
void R_DrawMaskedColumn(struct column_t*) __attribute((regparm(2)));

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

