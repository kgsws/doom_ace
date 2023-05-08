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

// original values
#define NUMMOBJTYPES	137
#define NUMSTATES	967
#define NUMSPRITES	138
#define NUMSFX	109
#define MAXEVENTS	64
#define MAXINTERCEPTS	128
#define MAXSPECIALCROSS	8
#define MAXSPECIALBUMP	8	// new
#define LIGHTLEVELS	16
#define LIGHTSEGSHIFT	4
#define MAXLIGHTSCALE	48
#define LIGHTSCALESHIFT	12
#define MAXLIGHTZ	128
#define LIGHTZSHIFT	20
#define FUZZTABLE	50
#define BASEYCENTER	100
#define MINZ	(4*FRACUNIT)
#define MAXBOB	(16 * FRACUNIT)
#define MAXRADIUS	(64 * FRACUNIT)
#define ONFLOORZ	-2147483648
#define ONCEILINGZ	2147483647

// HU values
#define HU_MAXLINELENGTH	80
#define HU_FONTSTART	'!'
#define HU_FONTEND	'_'
#define HU_FONTSIZE	(HU_FONTEND - HU_FONTSTART + 1)

// netgame
#define MAXNETNODES	8
#define BACKUPTICS	12

// tick data transfer
#define TIC_CMD_CHEAT_BUFFER	47
#define TIC_CMD_DATA_BUFFER	63 // maximum limit
#define TIC_CMD_CHEAT	0x8C
#define TIC_CMD_DATA	0xC0	// top two bits ON, used as mask
#define TIC_DATA_CHECK0	0xCC	// consistancy check
#define TIC_DATA_CHECK1	0xCD	// consistancy check
#define TIC_DATA_CHECK2	0xCE	// consistancy check
#define TIC_DATA_CHECK3	0xCF	// consistancy check
#define TIC_DATA_PLAYER_INFO	0xE1	// player info

//
// tables

#define ANGLETOFINESHIFT	19
#define FINEANGLES	8192
#define FINEMASK	(FINEANGLES - 1)
#define ANGLETOSKYSHIFT	22

#define ANG45	0x20000000UL
#define ANG90	0x40000000UL
#define ANG180	0x80000000UL
#define ANG270	0xC0000000UL

#define SLOPERANGE	2048
#define SLOPEBITS	11
#define DBITS	(FRACBITS - SLOPEBITS)

#define MAPBLOCKUNITS	128
#define MAPBLOCKSIZE	(MAPBLOCKUNITS * FRACUNIT)
#define MAPBLOCKSHIFT	(FRACBITS + 7)
#define MAPBMASK	(MAPBLOCKSIZE - 1)
#define MAPBTOFRAC	(MAPBLOCKSHIFT - FRACBITS)

#define PT_ADDLINES	1
#define PT_ADDTHINGS	2

#define ST_HORIZONTAL	0
#define ST_VERTICAL	1
#define ST_POSITIVE	2
#define ST_NEGATIVE	3

#define INTH_SIDE_LEFT	0
#define INTH_SIDE_RIGHT	1
#define INTH_SIDE_TOP	2
#define INTH_SIDE_BOT	3

//
// stuff

#define BOXTOP	0
#define BOXBOTTOM	1
#define BOXLEFT	2
#define BOXRIGHT	3

//
// skill

#define sk_baby	0
#define sk_easy	1
#define sk_medium	2
#define sk_hard	3
#define sk_nightmare	4

//
// game stuff

#define ga_nothing	0
#define ga_loadlevel	1
#define ga_newgame	2
#define ga_loadgame	3
#define ga_savegame	4
#define ga_playdemo	5
#define ga_completed	6
#define ga_victory	7
#define ga_worlddone	8
#define ga_screenshot	9

#define GS_LEVEL	0
#define GS_INTERMISSION	1
#define GS_FINALE	2
#define GS_DEMOSCREEN	3

#define ev_keydown	0
#define ev_keyup	1
#define ev_mouse	2
#define ev_joystick	3

//
// map

#define ML_LABEL	0
#define ML_THINGS	1
#define ML_LINEDEFS	2
#define ML_SIDEDEFS	3
#define ML_VERTEXES	4
#define ML_SEGS	5
#define ML_SSECTORS	6
#define ML_NODES	7
#define ML_SECTORS	8
#define ML_REJECT	9
#define ML_BLOCKMAP	10
#define ML_BEHAVIOR	11

#define ML_BLOCKING	0x0001
#define ML_BLOCKMONSTERS	0x0002
#define ML_TWOSIDED	0x0004
#define ML_DONTPEGTOP	0x0008
#define ML_DONTPEGBOTTOM	0x0010
#define ML_SECRET	0x0020
#define ML_SOUNDBLOCK	0x0040
#define ML_DONTDRAW	0x0080
#define ML_MAPPED	0x0100
#define ML_REPEATABLE	0x0200
#define ML_ACT_MASK	0x1C00
#define ML_MONSTER_ACT	0x2000
#define ML_BLOCK_PLAYER	0x4000
#define ML_BLOCK_ALL	0x8000

#define MLA_PLR_CROSS	0x0000
#define MLA_PLR_USE	0x0400
#define MLA_MON_CROSS	0x0800
#define MLA_ATK_HIT	0x0C00
#define MLA_PLR_BUMP	0x1000
#define MLA_PROJ_CROSS	0x1400
#define MLA_PASS	0x1800

#define MLI_3D_MIDTEX	0x01
#define MLI_IS_POLY	0x02
#define MLI_EXTRA_FRONT	0x04
#define MLI_EXTRA_BACK	0x08

#define MAXCEILINGS	30
#define MAXPLATS	30

//
// cheats

#define CF_NOCLIP	1
#define CF_GODMODE	2
#define CF_NOMOMENTUM	4 // REMOVED
#define CF_BUDDHA	8
#define CF_REVENGE	16
#define CF_CHANGE_CLASS	0x40000000
#define CF_IS_CHEATER	0x80000000

//
// zone

#define ZONEID	0x1D4A11
#define PU_STATIC	1
#define PU_LEVEL	50
#define PU_LEVELSPEC	51
#define PU_LEVEL_E3D	52	// extra floors
#define PU_LEVEL_INV	53	// mobj inventory
#define	PU_PURGELEVEL	100
#define PU_CACHE	101

//
// menu

typedef struct menuitem_s
{
	int16_t status;
	union
	{
		uint8_t name[10];
		uint8_t *text;
	} __attribute__((packed));
	void (*func)(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
	uint8_t key;
} __attribute__((packed)) menuitem_t;

typedef struct menu_s
{
	uint16_t numitems;
	struct menu_s *prev;
	menuitem_t *menuitems;
	void (*draw)() __attribute((regparm(2),no_caller_saved_registers));
	int16_t x, y;
	uint16_t last;
} __attribute__((packed)) menu_t;

typedef struct
{
	uint32_t type;
	uint32_t data1;
	uint32_t data2;
	uint32_t data3;
} event_t;

//
// center text

typedef struct
{
	uint32_t tic;
	uint16_t font;
	uint16_t lines;
	uint8_t *text;
} center_text_t;

//
// new inventory

struct invitem_s;
struct inventory_s;

//
// tics

#define BT_ATTACK	1
#define BT_USE	2
#define BT_CHANGE	4
#define BT_ALTACK	64
#define BT_SPECIAL	128
#define BT_ACTIONMASK	0b00111100
#define BT_ACTIONSHIFT	2

#define BT_WEAPONMASK	0b00111000
#define BT_WEAPONSHIFT	3

#define BT_SPECIALMASK	0b01100011

#define BT_ACT_INV_PREV	15
#define BT_ACT_INV_NEXT	14
#define BT_ACT_INV_USE	13
#define BT_ACT_INV_QUICK	12
#define BT_ACT_JUMP	11

typedef struct
{ // this structure has been changed
	int8_t forwardmove;
	int8_t sidemove;
	int16_t angleturn;
	int16_t pitchturn;
	uint8_t chatchar;
	uint8_t buttons;
} ticcmd_t;

typedef struct thinker_s
{
	struct thinker_s *prev;
	struct thinker_s *next;
	union
	{
		void *function;
		void (*func)(struct thinker_s*) __attribute((regparm(2),no_caller_saved_registers));
	};
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
	int32_t tics;
	uint32_t extra;
	int16_t sx, sy;
} pspdef_t;

//
// extra

typedef struct
{
	uint32_t alias;
	uint16_t state;
} __attribute__((packed)) custom_state_t;

//
// player

#define	NUMPSPRITES	2
#define MAXPLAYERS	4

#define NUMWEAPONS	9
#define NUMAMMO		4
#define NUMCARDS		6

#define WEAPONBOTTOM	128
#define WEAPONTOP	32
#define WEAPONRAISE	6

#define USERANGE	(64 << FRACBITS)
#define MELEERANGE	(64 << FRACBITS)
#define MISSILERANGE	(2048 << FRACBITS)
#define AIMRANGE	(1024 << FRACBITS)

enum
{
	PST_LIVE,
	PST_DEAD,
	PST_REBORN,
	PST_SPECTATE,
};

enum
{
	PROP_FROZEN,
	PROP_NOTARGET,
	PROP_INSTANTWEAPONSWITCH, // not used
	PROP_FLY,
	PROP_TOTALLYFROZEN,
	// custom
	PROP_CAMERA_MOVE,
};

enum
{
	pw_invulnerability,
	pw_strength,
	pw_invisibility,
	pw_ironfeet,
	pw_allmap,
	pw_infrared,
	pw_buddha,  // new
	pw_attack_speed, // new
	pw_flight, // new
	pw_reserved0,
	pw_reserved1,
	pw_reserved2,
	NUMPOWERS
};

typedef struct player_s
{ // this structure has been changed
	struct mobj_s *mo;
	uint16_t state;
	uint16_t prop;
	ticcmd_t cmd;
	fixed_t viewz;
	fixed_t viewheight;
	fixed_t deltaviewheight;
	fixed_t bob;
	int32_t health;
	int32_t armorpoints;
	int32_t armortype;
	int32_t powers[NUMPOWERS];
	uint32_t backpack;
	int32_t frags[MAXPLAYERS];
	struct mobjinfo_s *readyweapon;
	struct mobjinfo_s *pendingweapon;
	struct inventory_s *inventory; // for level transition
	int16_t inv_sel; // current selection
	uint16_t inv_tick; // inventory selection visible
	uint32_t stbar_update;
	uint32_t __unused__0;
	uint16_t power_mobj[NUMPOWERS];
	uint16_t flags;
	int16_t airsupply;
	struct mobj_s *camera;
	center_text_t text;
	angle_t angle;
	angle_t pitch;
	uint16_t attackdown;
	uint16_t weapon_ready;
	int32_t usedown;
	uint32_t cheats;
	uint32_t refire;
	uint32_t killcount;
	uint32_t itemcount;
	uint32_t secretcount;
	char *message;
	int32_t damagecount;
	int32_t bonuscount;
	struct mobj_s *attacker;
	int32_t extralight;
	int32_t fixedcolormap;
	int32_t fixedpalette;
	pspdef_t psprites[NUMPSPRITES];
	uint32_t didsecret;
} player_t;

typedef struct
{ // this is not cleared as 'player_t' is
	uint16_t color;
	uint16_t quick_inv;
	uint8_t playerclass;
	uint8_t flags;
} player_info_t;

//
// video

typedef struct patch_s
{
	uint16_t width, height;
	int16_t x, y;
	uint32_t offs[];
} patch_t;

typedef struct column_s
{
	uint8_t topdelta;
	uint8_t length;
} column_t;

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
// watcom

typedef struct heap_block_s
{
	uint32_t size;
	struct heap_block_s *prev;
	struct heap_block_s *next;
} heap_block_t;

typedef struct
{
	uint32_t unk0;
	uint32_t unk1;
	heap_block_t *block;
	uint32_t unk3;
	uint32_t unk4;
	uint32_t unk5;
	uint32_t unk6;
	heap_block_t rover;
} heap_base_t;

//
// zone

typedef struct memblock_s
{
	int32_t size;
	void **user;
	int32_t tag;
	int32_t id;
	struct memblock_s *next;
	struct memblock_s *prev;
} memblock_t;

typedef struct
{
	int32_t size;
	memblock_t blocklist;
	memblock_t *rover;
} memzone_t;

//
// info

enum
{
	ETYPE_NONE, // must be first
	ETYPE_POWERUP_BASE,
	ETYPE_PLAYERPAWN,
	ETYPE_PLAYERCHUNK,
	ETYPE_SWITCHABLE,
	ETYPE_RANDOMSPAWN,
	ETYPE_HEALTH,
	ETYPE_INV_SPECIAL,
	ETYPE_INVENTORY,
	ETYPE_INVENTORY_CUSTOM,
	ETYPE_WEAPON,
	ETYPE_AMMO,
	ETYPE_AMMO_LINK,
	ETYPE_KEY,
	ETYPE_ARMOR,
	ETYPE_ARMOR_BONUS,
	ETYPE_HEALTH_PICKUP,
	ETYPE_POWERUP,
	//
	NUM_EXTRA_TYPES,
};

enum
{
	PLR_SND_DEATH,
	PLR_SND_XDEATH,
	PLR_SND_GIBBED,
	PLR_SND_PAIN25,
	PLR_SND_PAIN50,
	PLR_SND_PAIN75,
	PLR_SND_PAIN100,
	PLR_SND_LAND,
	PLR_SND_USEFAIL,
	PLR_SND_JUMP,
	//
	NUM_PLAYER_SOUNDS
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
	fixed_t jump_z;
	fixed_t view_bob;
	uint8_t *name;
	uint16_t run_health;
	uint8_t color_first;
	uint8_t color_last;
	union
	{
		struct
		{
			uint16_t death;
			uint16_t xdeath;
			uint16_t gibbed;
			uint16_t pain[4];
			uint16_t land;
			uint16_t usefail;
			uint16_t jump;
		} sound;
		uint16_t sound_slot[NUM_PLAYER_SOUNDS];
	};
} ei_player_t;

typedef struct
{
	uint16_t count;
	uint16_t max_count;
	uint16_t hub_count;
	uint16_t flash_type;
	uint16_t respawn_tics;
	uint16_t sound_use;
	uint16_t sound_pickup;
	uint16_t special; // backpack, ammo parent ...
	int32_t icon;
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
	uint16_t sound_up;
	uint16_t sound_ready;
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

typedef struct
{
	ei_inventory_t inventory;
	int32_t duration;
	uint16_t type;
	int16_t strength;
	uint16_t colorstuff;
	int8_t mode;
} ei_powerup_t;

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
	uint8_t stealth_alpha;
	uint8_t __free_27;
	uint32_t state_pain;
	uint16_t damage_type;
	uint16_t blood_type;
	uint16_t painsound;
	uint16_t bouncesound;
	uint32_t state_melee;
	uint32_t state_missile;
	uint32_t state_death;
	uint32_t state_xdeath;
	uint16_t deathsound;
	uint8_t render_style;
	uint8_t render_alpha;
	int32_t speed;
	int32_t radius;
	int32_t height;
	int32_t mass;
	int32_t damage;
	uint16_t activesound;
	uint16_t bounce_count;
	uint32_t flags;
	uint32_t state_raise;
	// new stuff
	uint64_t alias;
	uint8_t args[6];
	uint16_t replacement;
	uint32_t flags1;
	uint32_t flags2;
	uint32_t eflags;
	fixed_t fast_speed;
	fixed_t vspeed;
	fixed_t step_height;
	fixed_t camera_height;
	fixed_t dropoff;
	fixed_t gravity;
	fixed_t range_melee;
	fixed_t scale;
	fixed_t bounce_factor;
	uint64_t species;
	uint16_t telefog[2];
	uint8_t *translation;
	uint8_t *blood_trns;
	uint8_t *damage_func;
	// damage type stuff
	uint16_t painchance[NUM_DAMAGE_TYPES];
	uint16_t damage_factor[NUM_DAMAGE_TYPES];
	// custom states
	custom_state_t *custom_states;
	custom_state_t *custom_st_end;
	// new states
	union
	{
		struct
		{
			// new states
			uint16_t state_heal;
			uint16_t state_crush;
			uint16_t state_crash;
			uint16_t state_xcrash;
		};
		uint16_t new_states[4];
	};
	// type-based states
	union
	{
		uint16_t extra_states[4];
		struct
		{
			uint16_t ready;
			uint16_t lower;
			uint16_t raise;
			uint16_t deadlow;
			uint16_t fire;
			uint16_t fire_alt;
			uint16_t hold;
			uint16_t hold_alt;
			uint16_t flash;
			uint16_t flash_alt;
		} st_weapon;
		struct
		{
			uint16_t pickup;
			uint16_t use;
		} st_custinv;
		struct
		{
			uint16_t active;
			uint16_t inactive;
		} st_switchable;
	};
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
	// type based stuff
	uint32_t extra_type;
	union
	{
		ei_player_t player;
		ei_inventory_t inventory;
		ei_weapon_t weapon;
		ei_ammo_t ammo;
		ei_armor_t armor;
		ei_powerup_t powerup;
		uint32_t random_weight;
	};
} mobjinfo_t;

typedef uint32_t(*stfunc_t)(struct mobj_s*,uint32_t,uint16_t) __attribute((regparm(3),no_caller_saved_registers)); // three!

typedef struct state_s
{
	uint16_t sprite;
	uint16_t frame;
	void *arg;
	uint16_t tics;
	uint16_t next_extra;
	union
	{
		void *action;
		void (*acp)(struct mobj_s*,struct state_s*,stfunc_t) __attribute((regparm(2),no_caller_saved_registers));
	};
	uint32_t nextstate;
	int32_t misc1;
	int32_t misc2;
} state_t;

//
// old info

typedef struct
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
} deh_mobjinfo_t;

typedef struct deh_state_s
{
	int32_t sprite;
	int32_t frame;
	int32_t tics;
	void *action;
	int32_t nextstate;
	int32_t misc1;
	int32_t misc2;
} deh_state_t;

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
	int16_t x, y;
} mapvertex_t;

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
	struct sector_s *target;
	uint8_t use_ceiling;
	uint8_t link_floor;
	uint8_t link_ceiling;
} plane_link_t;

typedef struct
{ // this should be part of 'sector_t'
	fixed_t bbox[4];
	plane_link_t *plink;
	uint16_t color;
	uint16_t fade;
	struct
	{
		int16_t amount;
		uint16_t tics;
		uint8_t type;
		uint8_t leak;
	} damage;
	struct
	{
		struct mobj_s *enter;
		struct mobj_s *leave;
	} action;
} sector_extra_t;

typedef struct sector_s
{ // this structure has been changed
	fixed_t floorheight;
	fixed_t ceilingheight;
	uint16_t floorpic;
	uint16_t ceilingpic;
	int16_t lightlevel;
	uint16_t special;
	uint16_t tag;
	int32_t sndtraversed;
	struct mobj_s *soundtarget;
	int32_t blockbox[4];
	union
	{
		struct
		{ // this overlaps unused space in sound source
			sector_extra_t *extra;
			struct extraplane_s *exfloor;
			struct extraplane_s *exceiling;
			fixed_t x, y; // SOUND
			uint8_t sndseq;
			uint8_t ed3_multiple;
			uint8_t e3d_origin;
			uint8_t __free;
		};
		degenmobj_t soundorg;
	};
	uint32_t validcount;
	struct mobj_s *thinglist;
	union
	{
		void *specialdata; // used in MAP_FORMAT_DOOM
		uint32_t specialactive; // used in MAP_FORMAT_HEXEN
	};
	uint32_t linecount;
	struct line_s **lines;
} __attribute__((packed)) sector_t;

typedef struct line_s
{ // this structure has been changed
	vertex_t *v1, *v2;
	fixed_t dx, dy;
	uint16_t flags;
	uint8_t special;
	uint8_t iflags;
	union
	{
		uint16_t tag;
		struct
		{
			uint8_t id;
			uint8_t arg0;
		};
	};
	uint16_t sidenum[2];
	fixed_t bbox[4];
	uint8_t slopetype;
	uint8_t __free_B;
	uint16_t e3d_tag;
	sector_t *frontsector;
	sector_t *backsector;
	uint32_t validcount;
	union
	{
		uint32_t args;
		struct
		{
			uint8_t arg1, arg2, arg3, arg4;
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

typedef struct
{ // this should be part of 'subsector_t'
	struct
	{
		struct seg_s **segs;
		uint8_t segcount;
	} poly;
} subsector_extra_t;

typedef struct subsector_s
{
	sector_t *sector;
	uint16_t numlines;
	uint16_t firstline;
} subsector_t;

typedef struct
{
	fixed_t frac;
	uint32_t isaline;
	union
	{
		struct mobj_s *thing;
		struct line_s *line;
	} d;
} intercept_t;

typedef struct
{
	fixed_t x, y;
	fixed_t dx, dy;
} divline_t;

//
// sound

typedef struct old_sfxinfo_s
{
	uint8_t *name;
	uint32_t single;
	uint32_t priority;
	struct old_sfxinfo_s *link;
	int32_t pitch;
	int32_t volume;
	void *data;
	int32_t usefulness;
	int32_t lumpnum;
} old_sfxinfo_t;

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

typedef struct
{
	char *name;
	int32_t lumpnum;
	void *data;
	int32_t handle;
} musicinfo_t;

typedef struct
{
	uint16_t start;
	uint16_t stop;
	uint16_t move;
	uint16_t delay;
	uint16_t repeat;
} seq_sounds_t;

typedef struct
{
	uint64_t alias;
	uint16_t number;
	seq_sounds_t norm_open;
	seq_sounds_t norm_close;
	seq_sounds_t fast_open;
	seq_sounds_t fast_close;
} sound_seq_t;

//
// data

typedef struct texpatch_s
{
	int32_t originx;
	int32_t originy;
	int32_t patch;
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

typedef struct
{
	uint16_t special;
	int16_t arg[5];
	uint16_t tid;
} mobj_special_t;

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
	fixed_t gravity;
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
	int32_t tics;
	state_t *state;
	uint32_t flags;
	union
	{
		struct
		{
			int32_t health;
			int32_t movedir;
			int32_t movecount;
		};
		struct
		{
			int32_t angle;
			int32_t pitch;
		} mover;
	};
	struct mobj_s *target;
	int32_t reactiontime;
	int32_t threshold;
	struct player_s *player;
	int32_t lastlook;
	mapthing_t spawnpoint;
	struct mobj_s *tracer;
	// new pointers
	struct mobj_s *master;
	struct mobj_s *inside; // for A_SpawnItemEx and similar
	// sector action
	sector_t *old_sector;
	// more flags
	uint32_t flags1;
	uint32_t flags2;
	uint32_t iflags;
	// render
	uint8_t render_style;
	uint8_t render_alpha;
	int8_t alpha_dir;
	uint8_t *translation;
	fixed_t scale;
	fixed_t e3d_floorz;
	// animation system
	uint8_t animation;
	// path traverse
	uint8_t intercept_side;
	// damage type - also set on death
	uint8_t damage_type;
	// water level
	uint8_t waterlevel;
	// unique identification
	uint32_t netid;
	// new orientation
	angle_t pitch;
	// inventory
	struct inventory_s *inventory;
	// activating item, nesting is not supported
	mobjinfo_t *custom_inventory;
	// state jumps
	uint32_t next_state;
	uint32_t custom_state;
	uint16_t next_extra;
	uint16_t custom_extra;
	// to avoid mutiple rip damage per tick
	struct mobj_s *rip_thing;
	uint32_t rip_tick;
	// bounce
	uint16_t bounce_count;
	// frozen corpse
	uint32_t freeze_tick;
	// special
	mobj_special_t special;
	// new sound sources
	vertex_t sound_body;
	vertex_t sound_weapon;
} __attribute__((packed)) mobj_t;

//
// render

typedef struct
{
	fixed_t	x, y;
	fixed_t	dx, dy;
	fixed_t	bbox[2][4];
	uint16_t children[2];
} node_t;

typedef struct seg_s
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
{ // this structure has been changed
	fixed_t height;
	int32_t picnum;
	union
	{
		int32_t lightlevel;
		struct
		{
			uint16_t light;
			uint16_t alpha;
		};
	};
	int32_t minx, maxx;
	uint8_t pad0;
	uint8_t top[SCREENWIDTH];
	uint8_t pad1[2];
	uint8_t bottom[SCREENWIDTH];
	uint8_t pad2;
} __attribute__((packed)) visplane_t;

typedef struct vissprite_s
{ // this structure has been changed
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
	fixed_t scale_scaled;
	union
	{
		mobj_t *mo;
		pspdef_t *psp;
		void *ptr;
	};
} vissprite_t;

typedef	struct
{
	int32_t first;
	int32_t last;
} cliprange_t;

//
// network

typedef struct
{
	uint8_t check;
	uint8_t game_info; // 0-2 skill; 3-4 mode; 5-6 inventory
	uint8_t flags;
	uint8_t respawn;
	uint8_t map_idx;
	uint8_t menu_idx;
	uint16_t prng_idx;
	player_info_t pi[MAXPLAYERS];
} dc_net_key_t;

typedef struct
{
	uint32_t version;
	uint32_t mod_csum;
	uint8_t key_check;
	uint8_t node_check;
	player_info_t pi;
} dc_net_node_t;

typedef struct
{
	uint16_t offset;
	uint8_t data[];
} dc_net_px_t;

typedef struct
{
	uint32_t checksum;
	uint8_t retransmitfrom;
	uint8_t starttic;
	uint8_t player;
	uint8_t numtics;
	union
	{
		ticcmd_t cmds[BACKUPTICS];
		dc_net_key_t net_key;
		dc_net_node_t net_node;
		dc_net_px_t net_px;
	};
} doomdata_t;

typedef struct
{
	uint32_t id; // 0x12345678
	uint16_t intnum;
	uint16_t command;
	uint16_t remotenode;
	uint16_t datalength;
	uint16_t numnodes;
	uint16_t ticdup;
	uint16_t extratics;
	uint16_t deathmatch;
	uint16_t savegame;
	uint16_t episode;
	uint16_t map;
	uint16_t skill;
	uint16_t consoleplayer;
	uint16_t numplayers;
	uint16_t angleoffset;
	uint16_t drone;
	doomdata_t data;
} doomcom_t;

//
// status bar

typedef struct
{ // this structure has been changed
	int32_t x;
	int32_t y;
	int32_t width;
	uint32_t oldnum;
	uint16_t *num;
	uint32_t *on;
	patch_t **p;
	int32_t data;
} st_number_t;

typedef struct
{
	int32_t x;
	int32_t y;
	uint32_t oldinum;
	uint32_t *inum;
	int32_t on;
	patch_t **p;
	int32_t data;
} st_multicon_t;

//
// headsup

typedef struct
{
	int32_t x;
	int32_t y;
	patch_t **f;
	int32_t sc;
	uint8_t l[HU_MAXLINELENGTH+1];
	int32_t len;
	int32_t needsupdate;
} __attribute__((packed)) hu_textline_t;

typedef struct
{
	uint8_t text[TIC_CMD_CHEAT_BUFFER];
	int8_t tpos;
	uint8_t data[TIC_CMD_DATA_BUFFER];
	int8_t dlen;
	int8_t dpos;
} cheat_buf_t;

//
// WI

typedef struct
{
	uint32_t in;
	int32_t skills;
	int32_t sitems;
	int32_t ssecret;
	int32_t stime;
	int32_t frags[MAXPLAYERS];
	int32_t score;
} wbplayerstruct_t;

typedef struct
{
	int32_t epsd;
	uint32_t didsecret;
	int32_t last;
	int32_t next;
	int32_t maxkills;
	int32_t maxitems;
	int32_t maxsecret;
	int32_t maxfrags;
	int32_t partime;
	int32_t pnum;
	wbplayerstruct_t plyr[MAXPLAYERS];
} wbstartstruct_t;

//
// old special effects

typedef struct
{
	thinker_t thinker;
	uint32_t type;
	sector_t *sector;
	fixed_t bottomheight;
	fixed_t topheight;
	fixed_t speed;
	uint32_t crush;
	int32_t direction;
	uint32_t tag;
	int32_t olddirection;
} ceiling_t;

typedef struct
{
	thinker_t thinker;
	uint32_t type;
	sector_t *sector;
	fixed_t	topheight;
	fixed_t	speed;
	int32_t direction;
	int32_t topwait;
	int32_t topcountdown;
} vldoor_t;

typedef struct
{
	thinker_t thinker;
	uint32_t type;
	uint32_t crush;
	sector_t *sector;
	int32_t direction;
	uint32_t newspecial;
	uint16_t texture;
	fixed_t floordestheight;
	fixed_t speed;
} __attribute__((packed)) floormove_t;

typedef struct
{
	thinker_t thinker;
	sector_t *sector;
	fixed_t speed;
	fixed_t low;
	fixed_t high;
	int32_t wait;
	int32_t count;
	uint32_t status;
	uint32_t oldstatus;
	uint32_t crush;
	uint32_t tag;
	uint32_t type;
} plat_t;

typedef struct
{
	thinker_t thinker;
	sector_t *sector;
	int32_t count;
	int32_t maxlight;
	int32_t minlight;
	uint32_t maxtime;
	uint32_t mintime;
} lightflash_t;

typedef struct
{
	thinker_t thinker;
	sector_t *sector;
	int32_t count;
	int32_t minlight;
	int32_t maxlight;
	uint32_t darktime;
	uint32_t brighttime;
} strobe_t;

typedef struct
{
	thinker_t thinker;
	sector_t *sector;
	int32_t minlight;
	int32_t maxlight;
	int32_t direction;
} glow_t;

typedef struct
{
	thinker_t thinker;
	sector_t *sector;
	int32_t count;
	int32_t maxlight;
	int32_t minlight;
} fireflicker_t;

// ASM hooks
void hook_mobj_damage();
void hook_obj_key();
void hook_path_traverse();
void hook_sound_adjust();
void hook_masked_range_draw();
void hook_bluescreen();
void engine_error(const uint8_t*,const uint8_t*, ...) __attribute((noreturn));
uint32_t dpmi_get_ram(); // this is modified I_ZoneBase
void skip_message_cancel() __attribute((noreturn));
void skip_menu_draw() __attribute((noreturn));
void rng_asm_code();
void _hack_update(); // this is address of 'ret' opcode in 'D_Display'; no-DOS-Doom2 hooks this location with screen update

// extra
void I_FinishUpdate();

// doom variables
#include "doom_vars.h"

// math
fixed_t FixedDiv(fixed_t, fixed_t) __attribute((regparm(2),no_caller_saved_registers));
#define FixedMul(a,b)	((int32_t)(((int64_t)(a) * (int64_t)(b)) >> FRACBITS))

// main.c
extern uint8_t *ldr_alloc_message;
extern uint_fast8_t dev_mode;
void gfx_progress(int32_t step);
void *ldr_malloc(uint32_t size);
void *ldr_realloc(void *ptr, uint32_t size);
void ldr_dump_buffer(const uint8_t *path, void *buff, uint32_t size);
void ldr_get_patch_header(int32_t lump, patch_t *p);
void error_message(uint8_t*);
void zone_info();

// stuff
void I_StartTic() __attribute((regparm(2),no_caller_saved_registers));

// am_map
void AM_Stop() __attribute((regparm(2),no_caller_saved_registers));

// d_net
void D_CheckNetGame() __attribute((regparm(2),no_caller_saved_registers));
void HSendPacket(uint32_t node, uint32_t flags) __attribute((regparm(2),no_caller_saved_registers));
uint32_t HGetPacket() __attribute((regparm(2),no_caller_saved_registers));

// f_finale
void F_StartCast() __attribute((regparm(2),no_caller_saved_registers));

// g_game
void G_DeferedInitNew(uint32_t,uint32_t,uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void G_BuildTiccmd(ticcmd_t*) __attribute((regparm(2),no_caller_saved_registers));
void G_CheckDemoStatus() __attribute((regparm(2),no_caller_saved_registers));
void G_DeathMatchSpawnPlayer(uint32_t) __attribute((regparm(2),no_caller_saved_registers));

// i_video
void I_InitGraphics() __attribute((regparm(2),no_caller_saved_registers));
void I_UpdateNoBlit() __attribute((regparm(2),no_caller_saved_registers));
void I_SetPalette(uint8_t*) __attribute((regparm(2),no_caller_saved_registers));
uint32_t I_GetTime() __attribute((regparm(2),no_caller_saved_registers));
void I_WaitVBL(uint32_t) __attribute((regparm(2),no_caller_saved_registers));

// i_sound
void I_StartupSound() __attribute((regparm(2),no_caller_saved_registers));

// m_argv
uint32_t M_CheckParm(uint8_t*) __attribute((regparm(2),no_caller_saved_registers));

// m_bbox
void M_ClearBox(fixed_t*) __attribute((regparm(2),no_caller_saved_registers));
void M_AddToBox(fixed_t*,fixed_t,fixed_t) __attribute((regparm(2),no_caller_saved_registers));

// m_random
int32_t P_Random() __attribute((regparm(2),no_caller_saved_registers));

// st_stuff
void ST_Init() __attribute((regparm(2),no_caller_saved_registers));
void ST_Start() __attribute((regparm(2),no_caller_saved_registers));
void ST_Drawer(uint32_t,uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void ST_doPaletteStuff() __attribute((regparm(2),no_caller_saved_registers));

// hu_stuff
void HU_Start() __attribute((regparm(2),no_caller_saved_registers));
uint8_t HU_dequeueChatChar() __attribute((regparm(2),no_caller_saved_registers));
void HU_queueChatChar(uint8_t) __attribute((regparm(2),no_caller_saved_registers));

// hu_lib
void HUlib_drawTextLine(hu_textline_t *l, uint32_t cursor);

// m_menu
void M_Drawer() __attribute((regparm(2),no_caller_saved_registers));
void M_ClearMenus() __attribute((regparm(2),no_caller_saved_registers));
void M_StartMessage(uint8_t*,void*,uint32_t) __attribute((regparm(2),no_caller_saved_registers));

// m_misc
void M_LoadDefaults() __attribute((regparm(2),no_caller_saved_registers));

// p_ceiling
void P_AddActiveCeiling(ceiling_t*) __attribute((regparm(2),no_caller_saved_registers));

// p_door
uint32_t EV_DoDoor(line_t*,uint32_t) __attribute((regparm(2),no_caller_saved_registers));

// p_enemy
void doom_A_Chase(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void doom_A_VileChase(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void doom_A_BrainAwake(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void doom_A_BrainSpit(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void doom_A_SpawnFly(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void P_NoiseAlert(mobj_t*,mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
uint32_t P_LookForPlayers(mobj_t*,uint32_t) __attribute((regparm(2),no_caller_saved_registers));

// p_map
uint32_t P_TryMove(mobj_t*,fixed_t,fixed_t) __attribute((regparm(2),no_caller_saved_registers));
void P_UseLines(player_t*) __attribute((regparm(2),no_caller_saved_registers));
fixed_t doom_P_AimLineAttack(mobj_t*,angle_t,fixed_t) __attribute((regparm(2),no_caller_saved_registers));
void P_LineAttack(mobj_t*,angle_t,fixed_t,fixed_t,uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void P_SlideMove(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void P_HitSlideLine(line_t*) __attribute((regparm(2),no_caller_saved_registers));
uint32_t P_CheckPosition(mobj_t*,fixed_t,fixed_t) __attribute((regparm(2),no_caller_saved_registers));
uint32_t P_TeleportMove(mobj_t*,fixed_t,fixed_t) __attribute((regparm(2),no_caller_saved_registers));

// p_maputl
void P_SetThingPosition(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void P_UnsetThingPosition(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void P_LineOpening(line_t*) __attribute((regparm(2),no_caller_saved_registers));
uint32_t P_TraverseIntercepts(void*,fixed_t) __attribute((regparm(2),no_caller_saved_registers));
uint32_t P_BlockThingsIterator(int32_t,int32_t,void*) __attribute((regparm(2),no_caller_saved_registers));
fixed_t P_InterceptVector(divline_t*,divline_t*) __attribute((regparm(2),no_caller_saved_registers));
uint32_t P_PointOnDivlineSide(fixed_t,fixed_t,divline_t*) __attribute((regparm(2),no_caller_saved_registers));
uint32_t P_PointOnLineSide(fixed_t,fixed_t,line_t*) __attribute((regparm(2),no_caller_saved_registers));
uint32_t P_BoxOnLineSide(fixed_t*,line_t*) __attribute((regparm(2),no_caller_saved_registers));

// p_mobj
mobj_t *P_SpawnMobj(fixed_t,fixed_t,fixed_t,uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void P_SpawnPlayerMissile(mobj_t*,uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void P_NightmareRespawn(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void P_MobjThinker(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

// p_plat
void P_AddActivePlat(plat_t*) __attribute((regparm(2),no_caller_saved_registers));

// p_pspr
void P_BulletSlope(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

// p_inter
void P_KillMobj(mobj_t*,mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

// p_setup
void P_GroupLines() __attribute((regparm(2),no_caller_saved_registers));
void P_LoadBlockMap(int32_t) __attribute((regparm(2),no_caller_saved_registers));
void P_LoadSectors(int32_t) __attribute((regparm(2),no_caller_saved_registers));
void P_LoadSideDefs(int32_t) __attribute((regparm(2),no_caller_saved_registers));
void P_LoadSubsectors(int32_t) __attribute((regparm(2),no_caller_saved_registers));
void P_LoadNodes(int32_t) __attribute((regparm(2),no_caller_saved_registers));
void P_LoadSegs(int32_t) __attribute((regparm(2),no_caller_saved_registers));
void doom_LoadLineDefs(int32_t) __attribute((regparm(2),no_caller_saved_registers));

// p_spec
void P_PlayerInSpecialSector(player_t*) __attribute((regparm(2),no_caller_saved_registers));
void P_SpawnSpecials() __attribute((regparm(2),no_caller_saved_registers));
void P_ShootSpecialLine(mobj_t*,line_t*) __attribute((regparm(2),no_caller_saved_registers));
fixed_t P_FindLowestCeilingSurrounding(sector_t*) __attribute((regparm(2),no_caller_saved_registers));
fixed_t P_FindLowestFloorSurrounding(sector_t*) __attribute((regparm(2),no_caller_saved_registers));

// p_tick
void P_InitThinkers() __attribute((regparm(2),no_caller_saved_registers));
void P_RunThinkers() __attribute((regparm(2),no_caller_saved_registers));
void P_AddThinker(thinker_t*) __attribute((regparm(2),no_caller_saved_registers));
void P_RemoveThinker(thinker_t*) __attribute((regparm(2),no_caller_saved_registers));

// p_user
void P_DeathThink(player_t*) __attribute((regparm(2),no_caller_saved_registers));

// r_bsp
void R_AddLine(seg_t*) __attribute((regparm(2),no_caller_saved_registers));
void R_ClipPassWallSegment(int32_t,int32_t) __attribute((regparm(2),no_caller_saved_registers));
void R_ClipSolidWallSegment(int32_t,int32_t) __attribute((regparm(2),no_caller_saved_registers));
subsector_t *R_PointInSubsector(int32_t,int32_t) __attribute((regparm(2),no_caller_saved_registers));

// r_data
void R_GenerateComposite(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void R_GenerateLookup(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void R_PrecacheLevel() __attribute((regparm(2),no_caller_saved_registers));

// r_main
void R_ExecuteSetViewSize() __attribute((regparm(2),no_caller_saved_registers));
void R_RenderPlayerView(player_t*) __attribute((regparm(2),no_caller_saved_registers));
angle_t R_PointToAngle(fixed_t,fixed_t) __attribute((regparm(2),no_caller_saved_registers));
angle_t R_PointToAngle2(fixed_t,fixed_t,fixed_t,fixed_t) __attribute((regparm(2),no_caller_saved_registers));
void R_SetupFrame(player_t*) __attribute((regparm(2),no_caller_saved_registers));
fixed_t R_ScaleFromGlobalAngle(angle_t) __attribute((regparm(2),no_caller_saved_registers));
fixed_t R_PointToDist(fixed_t,fixed_t) __attribute((regparm(2),no_caller_saved_registers));

// r_plane
visplane_t *R_FindPlane(fixed_t,uint32_t,uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void R_MakeSpans(int32_t,int32_t,int32_t,int32_t,int32_t) __attribute((regparm(2),no_caller_saved_registers));

// r_things
void R_InstallSpriteLump(uint32_t,uint32_t,uint32_t,uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void R_DrawSprite(vissprite_t*) __attribute((regparm(2),no_caller_saved_registers));
void R_SortVisSprites() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawPSprite(pspdef_t*,fixed_t,fixed_t) __attribute((regparm(1),no_caller_saved_registers)); // modified for two arguments on stack!

// s_sound.c
void S_Start() __attribute((regparm(2),no_caller_saved_registers));
void S_StartSound(mobj_t*,uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void doom_S_StopSound(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void S_ResumeSound() __attribute((regparm(2),no_caller_saved_registers));
void S_StopMusic() __attribute((regparm(2),no_caller_saved_registers));
void S_ChangeMusic(uint32_t,uint32_t) __attribute((regparm(2),no_caller_saved_registers));

// v_video
void V_DrawPatch(int32_t, int32_t, uint32_t, patch_t*) __attribute((regparm(2),no_caller_saved_registers));
void V_MarkRect(int32_t,int32_t,int32_t,int32_t) __attribute((regparm(2),no_caller_saved_registers));
void V_CopyRect(int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t); // this one is nasty

// w_wad
int32_t W_CheckNumForName(uint8_t *name) __attribute((regparm(2),no_caller_saved_registers));
int32_t W_GetNumForName(uint8_t *name) __attribute((regparm(2),no_caller_saved_registers));
void *W_CacheLumpName(uint8_t *name, uint32_t tag) __attribute((regparm(2),no_caller_saved_registers));
void *W_CacheLumpNum(int32_t lump, uint32_t tag) __attribute((regparm(2),no_caller_saved_registers));
uint32_t W_LumpLength(int32_t lump) __attribute((regparm(2),no_caller_saved_registers));
void W_ReadLump(int32_t lump, void *dst) __attribute((regparm(2),no_caller_saved_registers));

// wi_stuff
void WI_Start(wbstartstruct_t*) __attribute((regparm(2),no_caller_saved_registers));

// z_zone
void *Z_Malloc(uint32_t size, uint32_t tag, void *owner) __attribute((regparm(2),no_caller_saved_registers));
void Z_Free(void *ptr) __attribute((regparm(2),no_caller_saved_registers));
void Z_FreeTags(uint32_t,uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void Z_ChangeTag2(void*,uint32_t) __attribute((regparm(2),no_caller_saved_registers));

// extra inline

static inline fixed_t P_AproxDistance(fixed_t dx, fixed_t dy)
{
	dx = abs(dx);
	dy = abs(dy);
	if(dx < dy)
		return dx + dy - (dx >> 1);
	return dx + dy - (dy >> 1);
}

