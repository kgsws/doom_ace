// kgsws' ACE Engine
////

enum
{
	CLUSTER_NONE = -1,
	CLUSTER_D1_EPISODE1,
	CLUSTER_D1_EPISODE2,
	CLUSTER_D1_EPISODE3,
	CLUSTER_D1_EPISODE4,
	CLUSTER_D2_1TO6,
	CLUSTER_D2_7TO11,
	CLUSTER_D2_12TO20,
	CLUSTER_D2_21TO30,
	CLUSTER_D2_LVL31,
	CLUSTER_D2_LVL32,
	DEF_CLUSTER_COUNT
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
} map_cluster_t;

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

extern uint8_t map_lump_name[9];
extern int32_t map_lump_idx;

extern uint_fast8_t map_skip_stuff;
extern uint_fast8_t is_title_map;

extern uint32_t num_clusters;
extern map_cluster_t *map_cluster;

//

void init_map();

void map_start_title();

void map_load_setup() __attribute((regparm(2),no_caller_saved_registers));

// thinker
void think_line_scroll(line_scroll_t *ls) __attribute((regparm(2),no_caller_saved_registers));

