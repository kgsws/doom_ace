// kgsws' ACE Engine
////

#define LOCKDEFS_DEF_SIZE	1024

#define MAX_EPISODES	5

#define MAP_FORMAT_DOOM	1
#define MAP_FORMAT_HEXEN	2

#define MTF_EASY	0x0001
#define MTF_MEDIUM	0x0002
#define MTF_HARD	0x0004
#define MTF_AMBUSH	0x0008
#define MTF_INACTIVE	0x0010
#define MTF_CLASS0	0x0020
#define MTF_CLASS1	0x0040
#define MTF_CLASS2	0x0080
#define MTF_SINGLE	0x0100
#define MTF_COOPERATIVE	0x0200
#define MTF_DEATHMATCH	0x0400
#define MTF_SHADOW	0x0800
#define MTF_ALTSHADOW	0x1000
#define MTF_FRIENDLY	0x2000
#define MTF_STANDSTILL	0x4000

#define MAP_END_UNDEFINED	0xFFFA // must be first
#define MAP_END_TO_TITLE	0xFFFB // must be second
#define MAP_END_BUNNY_SCROLL	0xFFFC
#define MAP_END_DOOM_CAST	0xFFFD
#define MAP_END_CUSTOM_PIC_N	0xFFFE
#define MAP_END_CUSTOM_PIC_S	0xFFFF

#define EPI_FLAG_NO_SKILL_MENU	1

#define MAP_FLAG_NO_INTERMISSION	0x0001
#define MAP_FLAG_FALLING_DAMAGE	0x0002
#define MAP_FLAG_MONSTER_FALL_DMG_KILL	0x0004
#define MAP_FLAG_MONSTER_FALL_DMG	0x0008
#define MAP_FLAG_NO_FREELOOK	0x0010
#define MAP_FLAG_FILTER_STARTS	0x0020
#define MAP_FLAG_USE_PLAYER_START_Z	0x0040
#define MAP_FLAG_ALLOW_RESPAWN	0x0080
#define MAP_FLAG_NO_INFIGHTING	0x0100
#define MAP_FLAG_TOTAL_INFIGHTING	0x0200
#define MAP_FLAG_CHECK_SWITCH_RANGE	0x0400
#define MAP_FLAG_RESET_INVENTORY	0x0800
#define MAP_FLAG_FORGET_STATE	0x1000
#define MAP_FLAG_SPAWN_WITH_WEAPON_RAISED	0x2000
#define MAP_FLAG_NO_MONSTER_ACTIVATION	0x4000

#define CLST_FLAG_HUB	0x0001

enum
{
	CLUSTER_D1_EPISODE1,
	CLUSTER_D1_EPISODE2,
	CLUSTER_D1_EPISODE3,
	//
	D1_CLUSTER_COUNT
};

enum
{
	CLUSTER_D2_1TO6,
	CLUSTER_D2_7TO11,
	CLUSTER_D2_12TO20,
	CLUSTER_D2_21TO30,
	CLUSTER_D2_LVL31,
	CLUSTER_D2_LVL32,
	//
	D2_CLUSTER_COUNT
};

enum
{
	MAPD1_E1M1,
	MAPD1_E1M2,
	MAPD1_E1M3,
	MAPD1_E1M4,
	MAPD1_E1M5,
	MAPD1_E1M6,
	MAPD1_E1M7,
	MAPD1_E1M8,
	MAPD1_E1M9,
	MAPD1_E2M1,
	MAPD1_E2M2,
	MAPD1_E2M3,
	MAPD1_E2M4,
	MAPD1_E2M5,
	MAPD1_E2M6,
	MAPD1_E2M7,
	MAPD1_E2M8,
	MAPD1_E2M9,
	MAPD1_E3M1,
	MAPD1_E3M2,
	MAPD1_E3M3,
	MAPD1_E3M4,
	MAPD1_E3M5,
	MAPD1_E3M6,
	MAPD1_E3M7,
	MAPD1_E3M8,
	MAPD1_E3M9,
	//
	NUM_D1_MAPS
};

enum
{
	MAPD2_MAP01,
	MAPD2_MAP02,
	MAPD2_MAP03,
	MAPD2_MAP04,
	MAPD2_MAP05,
	MAPD2_MAP06,
	MAPD2_MAP07,
	MAPD2_MAP08,
	MAPD2_MAP09,
	MAPD2_MAP10,
	MAPD2_MAP11,
	MAPD2_MAP12,
	MAPD2_MAP13,
	MAPD2_MAP14,
	MAPD2_MAP15,
	MAPD2_MAP16,
	MAPD2_MAP17,
	MAPD2_MAP18,
	MAPD2_MAP19,
	MAPD2_MAP20,
	MAPD2_MAP21,
	MAPD2_MAP22,
	MAPD2_MAP23,
	MAPD2_MAP24,
	MAPD2_MAP25,
	MAPD2_MAP26,
	MAPD2_MAP27,
	MAPD2_MAP28,
	MAPD2_MAP29,
	MAPD2_MAP30,
	MAPD2_MAP31,
	MAPD2_MAP32,
	//
	NUM_D2_MAPS
};

enum
{
	KEYLOCK_TERMINATOR = 0,
	KEYLOCK_MESSAGE = 0x1000,
	KEYLOCK_REMTMSG = 0x2000,
	KEYLOCK_KEYLIST = 0x3000,
};

typedef struct
{
	int16_t floorheight;
	int16_t ceilingheight;
	union
	{
		uint8_t floorpic[8];
		uint64_t wfloor;
	};
	union
	{
		uint8_t ceilingpic[8];
		uint64_t wceiling;
	};
	uint16_t lightlevel;
	uint16_t special;
	uint16_t tag;
} __attribute__((packed)) map_sector_t;

typedef struct
{
	int16_t textureoffset;
	int16_t rowoffset;
	union
	{
		uint8_t toptexture[8];
		uint64_t wtop;
	};
	union
	{
		uint8_t bottomtexture[8];
		uint64_t wbot;
	};
	union
	{
		uint8_t midtexture[8];
		uint64_t wmid;
	};
	uint16_t sector;
} __attribute__((packed)) map_sidedef_t;

typedef struct
{
	uint16_t v1;
	uint16_t v2;
	uint16_t flags;
	uint16_t special;
	uint16_t tag;
	uint16_t sidenum[2];
} __attribute__((packed)) map_linedef_t;

typedef struct
{
	uint16_t tid;
	int16_t x, y, z;
	int16_t angle;
	uint16_t type;
	uint16_t flags;
	uint8_t special;
	uint8_t arg[5];
} __attribute__((packed)) map_thinghex_t;

typedef struct
{
	uint16_t v1;
	uint16_t v2;
	uint16_t flags;
	uint8_t special;
	uint8_t arg0;
	uint32_t args;
	uint16_t sidenum[2];
} __attribute__((packed)) map_linehex_t;

//

typedef union
{
	uint8_t name[12];
	uint64_t wame;
} map_lump_name_t;

//

typedef struct
{
	thinker_t thinker;
	line_t *line;
	int16_t x, y;
} line_scroll_t;

//

typedef struct map_episode_s
{
	int32_t map_lump;
	int32_t title_lump;
	uint32_t flags;
} map_episode_t;

typedef struct
{
	uint8_t *text_enter;
	uint8_t *text_leave;
	int32_t lump_music;
//	int32_t lump_patch; // TODO: make it work
	int32_t lump_flat;
	uint16_t flags;
	uint8_t idx;
} map_cluster_t;

typedef struct
{
	int32_t lump;
	uint8_t *name;
	uint16_t next_normal;
	uint16_t next_secret;
	int32_t title_lump;
	int32_t win_lump[2];
	int32_t music_level;
	int32_t music_inter;
	uint16_t texture_sky[2];
	uint16_t par_time;
	uint16_t flags;
	uint8_t cluster;
	uint8_t levelnum;
	uint8_t levelhack; // for original maps
} map_level_t;

//

typedef struct
{
	uint16_t size;
	uint8_t id;
	uint8_t color; // not implemented
	uint16_t sound;
	uint16_t data[];
} lockdef_t;

//

typedef uint32_t (*line_func_t)(line_t*) __attribute((regparm(2),no_caller_saved_registers));

//

extern subsector_extra_t *e_subsectors;

extern map_lump_name_t map_lump;
extern int32_t map_lump_idx;
extern uint_fast8_t map_format;
extern map_level_t *map_level_info;
extern uint32_t num_maps;
extern map_level_t *map_info;
extern uint8_t map_start_id;
extern uint8_t map_start_facing;
extern uint16_t map_next_levelnum;

extern uint_fast8_t map_skip_stuff;
extern uint_fast8_t is_title_map;
extern uint_fast8_t is_net_desync;

extern uint_fast8_t survival;
extern uint_fast8_t net_inventory;
extern uint_fast8_t no_friendly_fire;
extern uint_fast8_t keep_keys;
extern uint_fast8_t weapons_stay;

extern uint32_t num_clusters;
extern map_cluster_t *map_cluster;

extern uint32_t map_episode_count;
extern map_episode_t map_episode_def[MAX_EPISODES];

extern void *lockdefs;
extern uint32_t lockdefs_size;

//

void init_map();

void map_start_title();

uint32_t map_load_setup(uint32_t);
void map_setup_old(uint32_t skill, uint32_t episode, uint32_t level);

map_cluster_t *map_find_cluster(uint32_t num);

// thinker
void think_line_scroll(line_scroll_t *ls) __attribute((regparm(2),no_caller_saved_registers));

// blockmap
uint32_t P_BlockLinesIterator(int32_t x, int32_t y, line_func_t func) __attribute((regparm(3),no_caller_saved_registers)); // three!

