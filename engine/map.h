// kgsws' ACE Engine
////

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

extern fixed_t *tmdropoffz;
extern fixed_t *openrange;

extern line_t **ceilingline;

extern mobj_t **linetarget;

extern uint8_t map_lump_name[9];
extern int32_t map_lump_idx;

extern uint_fast8_t map_skip_stuff;
extern uint_fast8_t is_title_map;

//

void map_start_title();

void map_load_setup() __attribute((regparm(2),no_caller_saved_registers));

// thinker
void think_line_scroll(line_scroll_t *ls) __attribute((regparm(2),no_caller_saved_registers));

