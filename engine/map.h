// kgsws' ACE Engine
////

#define MAP_END_TITLE	0xFFFB
#define MAP_END_DOOM1_EP1	0xFFFC
#define MAP_END_DOOM1_EP2	0xFFFD
#define MAP_END_DOOM1_EP3	0xFFFE
#define MAP_END_DOOM2	0xFFFF

#define MAP_FLAG_NOINTERMISSION	1

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
	int8_t x, y;
} line_scroll_t;

//

typedef struct
{
	uint8_t *text_enter;
	uint8_t *text_leave;
	int32_t lump_music;
	int32_t lump_patch;
	int32_t flat_num;
	uint8_t idx;
} map_cluster_t;

typedef struct
{
	int32_t lump;
	uint8_t *name;
	uint16_t next_normal;
	uint16_t next_secret;
	int32_t title_patch;
	int32_t music_lump;
	uint32_t par_time;
	uint16_t texture_sky[2];
	uint8_t cluster;
	uint8_t flags;
	uint8_t levelnum;
	uint8_t levelhack; // for original maps
} map_level_t;

//

extern mapthing_t *playerstarts;
extern mapthing_t *deathmatchstarts;
extern mapthing_t **deathmatch_p;

extern uint32_t *nomonsters;
extern uint32_t *fastparm;
extern uint32_t *respawnparm;

extern uint32_t *netgame;
extern uint32_t *deathmatch;
extern uint32_t *gamemap;
extern uint32_t *gameepisode;
extern uint32_t *gameskill;
extern uint32_t *leveltime;

extern uint32_t *totalkills;
extern uint32_t *totalitems;
extern uint32_t *totalsecret;

extern uint32_t *skytexture;
extern uint32_t *skyflatnum;

extern uint32_t *numsides;
extern uint32_t *numlines;
extern uint32_t *numsectors;
extern line_t **lines;
extern vertex_t **vertexes;
extern side_t **sides;
extern sector_t **sectors;

extern uint32_t *prndindex;

extern plat_t **activeplats;
extern ceiling_t **activeceilings;

extern uint32_t *netgame;
extern uint32_t *usergame;

extern uint32_t *nofit;
extern uint32_t *crushchange;

extern fixed_t *tmdropoffz;
extern fixed_t *openrange;
extern fixed_t *opentop;
extern fixed_t *openbottom;

extern line_t **ceilingline;

extern mobj_t **linetarget;

extern fixed_t *bmaporgx;
extern fixed_t *bmaporgy;

extern map_lump_name_t map_lump;
extern int32_t map_lump_idx;

extern uint_fast8_t map_skip_stuff;
extern uint_fast8_t is_title_map;

extern uint32_t num_clusters;
extern map_cluster_t *map_cluster;

//

void init_map();

void map_start_title();

void map_load_setup() __attribute((regparm(2),no_caller_saved_registers));
void map_setup_old(uint32_t skill, uint32_t episode, uint32_t level);

// thinker
void think_line_scroll(line_scroll_t *ls) __attribute((regparm(2),no_caller_saved_registers));

