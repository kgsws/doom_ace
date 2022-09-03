// kgsws' ACE Engine
////
// This file contains required engine types and function prototypes.

//
// basic

// extra include to manage amount of text
#include "e_mobj.h"

#define MAXWADFILES	20

#define SCREENWIDTH	320
#define SCREENHEIGHT	200
#define SCREENSIZE	(SCREENWIDTH*SCREENHEIGHT)

#define FRACBITS	16
#define FRACUNIT	(1 << 16)

typedef int32_t fixed_t;
typedef uint32_t angle_t;

//
// ACE Engine stuff

// do not change
#define NUM_WPN_SLOTS	10

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
// skill

#define sk_baby	0
#define sk_easy	1
#define sk_medium	2
#define sk_hard	3
#define sk_nightmare	4

//
// cheats

#define CF_NOCLIP	1
#define CF_GODMODE	2
#define CF_NOMOMENTUM	4

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
#define NUMCARDS		6

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

struct column_s;

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

enum
{
	ETYPE_NONE, // must be first
	ETYPE_PLAYERPAWN,
	ETYPE_HEALTH,
	ETYPE_WEAPON,
	ETYPE_AMMO,
	ETYPE_AMMO_LINK,
	ETYPE_KEY,
	ETYPE_ARMOR,
	ETYPE_ARMOR_BONUS,
	//
	NUM_EXTRA_TYPES,
};

typedef struct
{
	uint16_t type;
	int16_t chance;
	uint16_t amount;
	uint16_t __padding;
} mobj_dropitem_t;

typedef struct
{
	uint16_t type;
	uint16_t count;
} plrp_start_item_t;

typedef struct
{
	uint16_t *wpn_slot[NUM_WPN_SLOTS];
	fixed_t view_height;
	fixed_t attack_offs;
} ei_player_t;

typedef struct
{
	uint16_t count;
	uint16_t max_count;
	uint16_t hub_count;
	uint16_t sound_use;
	uint16_t sound_pickup;
	uint16_t special; // backpack, ammo parent ...
	uint8_t *message;
} ei_inventory_t;

typedef struct
{
	ei_inventory_t inventory;
	uint16_t selection_order;
	uint16_t kickback;
	uint16_t ammo_use[2];
	uint16_t ammo_give[2];
	uint16_t ammo_type[2];
} ei_weapon_t;

typedef struct
{
	ei_inventory_t inventory;
	uint16_t count;
	uint16_t max_count;
} ei_ammo_t;

typedef struct
{
	ei_inventory_t inventory;
	uint16_t count;
	uint16_t max_count;
	uint16_t percent;
} ei_armor_t;

typedef struct mobjinfo_s
{ // this structure has been changed
	uint16_t doomednum;
	uint16_t spawnid;
	uint32_t state_spawn;
	int32_t spawnhealth;
	uint32_t state_see;
	uint16_t seesound;
	uint16_t __free_18;
	int32_t reactiontime;
	uint16_t attacksound;
	uint16_t __free_24;
	uint32_t state_pain;
	int32_t painchance;
	uint16_t painsound;
	uint16_t __free_34;
	uint32_t state_melee;
	uint32_t state_missile;
	uint32_t state_death;
	uint32_t state_xdeath;
	uint16_t deathsound;
	uint16_t __free_4C;
	int32_t speed;
	int32_t radius;
	int32_t height;
	int32_t mass;
	int32_t damage;
	uint16_t activesound;
	uint16_t __free_68;
	uint32_t flags;
	uint32_t state_raise;
	// new stuff
	uint64_t actor_name;
	uint32_t state_idx_first;
	uint32_t state_idx_limit;
	uint32_t replacement;
	uint32_t flags1;
	uint32_t eflags;
	// extra stuff list
	union
	{
		void *extra_stuff[2];
		struct
		{
			mobj_dropitem_t *start;
			mobj_dropitem_t *end;
		} dropitem;
		struct
		{
			plrp_start_item_t *start;
			plrp_start_item_t *end;
		} start_item;
	};
	// new states
	uint16_t state_heal;
	uint16_t state_crush;
	// shared states
	union
	{
		uint16_t extra_states[4];
		struct
		{
			uint16_t state_ready;
			uint16_t state_lower;
			uint16_t state_raise;
			uint16_t state_deadlow;
			uint16_t state_fire;
			uint16_t state_fire_alt;
			uint16_t state_hold;
			uint16_t state_hold_alt;
			uint16_t state_flash;
			uint16_t state_flash_alt;
		} st_weapon;
	};
	// type based stuff
	uint32_t extra_type;
	union
	{
		ei_player_t player;
		ei_inventory_t inventory;
		ei_weapon_t weapon;
		ei_ammo_t ammo;
		ei_armor_t armor;
	};
} mobjinfo_t;

typedef struct state_s
{
	uint16_t sprite;
	uint16_t frame;
	void *arg;
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
	uint32_t validcount;
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
	uint16_t sidenum[2];
	fixed_t bbox[4];
	uint32_t slopetype;
	sector_t *frontsector;
	sector_t *backsector;
	uint32_t validcount;
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
// sound

typedef struct sfxinfo_s
{ // this structure has been changed
	uint64_t alias;
	uint32_t priority;
	uint8_t reserved; // can be used for $limit
	uint8_t rng_count;
	uint16_t rng_id[5];
	void *data;
	int32_t usefulness;
	int32_t lumpnum;
} sfxinfo_t;

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

typedef struct vissprite_s
{
	struct vissprite_s *next;
	struct vissprite_s *prev;
	int32_t x, y;
	fixed_t gx, gy;
	fixed_t gz, gzt;
	fixed_t startfrac;
	fixed_t scale;
	fixed_t xiscale;
	fixed_t texturemid;
	int32_t patch;
	void *colormap;
	uint32_t mobjflags;
} vissprite_t;

typedef struct spriteframe_s
{
	uint32_t rotate;
	uint16_t lump[8];
	uint8_t flip[8];
} spriteframe_t;

typedef struct spritedef_s
{
	uint32_t numframes;
	spriteframe_t *spriteframes;
} spritedef_t;

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
{ // this structure has been changed
	thinker_t thinker;
	fixed_t x;
	fixed_t y;
	fixed_t z;
	struct mobj_s *snext;
	struct mobj_s *sprev;
	angle_t angle;
	uint16_t sprite;
	uint16_t frame;
	uint32_t flags1;	// new flags
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
	uint32_t validcount;
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
	uint16_t animation;	// animation system
	struct mobj_s *tracer;
	uint32_t netid;	// unique identification
	struct inventory_s *inventory;
} __attribute__((packed)) mobj_t;

// ASM hooks
void hook_mobj_damage();

// some variables
extern uint32_t *gamemode;
extern fixed_t *finesine;
extern fixed_t *finecosine;
extern uint32_t *viewheight;
extern uint32_t *viewwidth;

// math
fixed_t FixedDiv(fixed_t, fixed_t) __attribute((regparm(2)));
#define FixedMul(a,b)	(((int64_t)(a) * (int64_t)(b)) >> FRACBITS)

// main.c
extern uint8_t *ldr_alloc_message;
void gfx_progress(int32_t step);
void *ldr_malloc(uint32_t size);
void *ldr_realloc(void *ptr, uint32_t size);

// i_video
void I_InitGraphics() __attribute((regparm(2)));
void I_FinishUpdate() __attribute((regparm(2)));

// m_argv
uint32_t M_CheckParm(uint8_t*) __attribute((regparm(2)));

// m_random
int32_t P_Random() __attribute((regparm(2)));

// st_stuff
void ST_Init() __attribute((regparm(2)));
void ST_Start() __attribute((regparm(2)));

// hu_stuff
void HU_Start() __attribute((regparm(2)));

// p_enemy
void doom_A_Look(mobj_t*) __attribute((regparm(2)));
void doom_A_Chase(mobj_t*) __attribute((regparm(2)));

// p_map
uint32_t P_TryMove(mobj_t*,fixed_t,fixed_t) __attribute((regparm(2)));

// p_maputl
void P_SetThingPosition(mobj_t*) __attribute((regparm(2)));
void P_UnsetThingPosition(mobj_t*) __attribute((regparm(2)));

// p_mobj
void P_RemoveMobj(mobj_t*) __attribute((regparm(2)));
mobj_t *P_SpawnMobj(fixed_t,fixed_t,fixed_t,uint32_t) __attribute((regparm(2)));
void P_SpawnPlayer(mapthing_t*) __attribute((regparm(2)));

// p_inter
void P_DamageMobj(mobj_t*,mobj_t*,mobj_t*,int32_t) __attribute((regparm(2)));
void P_TouchSpecialThing(mobj_t*,mobj_t*) __attribute((regparm(2)));
void P_KillMobj(mobj_t*,mobj_t*) __attribute((regparm(2)));

// p_pspr
void P_SetupPsprites(player_t*) __attribute((regparm(2)));

// p_setup
void P_SetupLevel() __attribute((regparm(2)));

// p_tick
void P_AddThinker(thinker_t*) __attribute((regparm(2)));

// r_data
void R_GenerateLookup(uint32_t) __attribute((regparm(2)));
void *R_GetColumn(uint32_t,uint32_t) __attribute((regparm(2)));

// r_main
void R_RenderPlayerView(player_t*) __attribute((regparm(2)));
angle_t R_PointToAngle2(fixed_t,fixed_t,fixed_t,fixed_t) __attribute((regparm(2)));

// r_things
void R_DrawMaskedColumn(struct column_s*) __attribute((regparm(2)));
void R_InstallSpriteLump(uint32_t,uint32_t,uint32_t,uint32_t) __attribute((regparm(2)));

// s_sound.c
void S_StartSound(mobj_t*,uint32_t) __attribute((regparm(2)));

// v_video
void V_DrawPatchDirect(int32_t, int32_t, uint32_t, patch_t*) __attribute((regparm(2)));

// w_wad
int32_t W_CheckNumForName(uint8_t *name) __attribute((regparm(2)));
void *W_CacheLumpName(uint8_t *name, uint32_t tag) __attribute((regparm(2)));
void *W_CacheLumpNum(int32_t lump, uint32_t tag) __attribute((regparm(2)));
uint32_t W_LumpLength(int32_t lump) __attribute((regparm(2)));
void W_ReadLump(int32_t lump, void *dst) __attribute((regparm(2)));

// z_zone
void *Z_Malloc(uint32_t size, uint32_t tag, void *owner) __attribute((regparm(2)));
void Z_Free(void *ptr) __attribute((regparm(2)));

// extra inline

static inline fixed_t P_AproxDistance(fixed_t dx, fixed_t dy)
{
	dx = abs(dx);
	dy = abs(dy);
	if(dx < dy)
		return dx + dy - (dx >> 1);
	return dx + dy - (dy >> 1);
}

