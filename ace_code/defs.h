// kgsws' Doom ACE
//

typedef struct
{
	fixed_t viewheight;
} player_class_t;

extern player_t *players;
extern weaponinfo_t *e_weaponinfo;
extern mobjinfo_t *e_mobjinfo;
extern state_t *e_states;

extern char **spr_names;

extern uint32_t numlumps;
extern lumpinfo_t *lumpinfo;
extern void **lumpcache;
extern uint32_t *screenblocks;
extern uint8_t **destscreen;
extern uint8_t *screens0;

extern uint32_t *skytexture;
extern uint32_t *skyflatnum;

extern uint32_t *consoleplayer;
extern uint32_t *displayplayer;

extern uint32_t *gamemap;
extern uint32_t *gameepisode;
extern uint32_t *gameskill;
extern uint32_t *netgame;
extern uint32_t *deathmatch;
extern uint32_t *gametic;
extern uint32_t *leveltime;
extern uint32_t *totalitems;
extern uint32_t *totalkills;
extern uint32_t *totalsecret;

extern fixed_t *finesine;
extern fixed_t *finecosine;
extern uint32_t *viewheight;
extern uint32_t *viewwidth;

extern thinker_t *thinkercap;
extern void *ptr_MobjThinker;

// storage (always temporary)
#define STORAGE_VISPLANES	(128*sizeof(visplane_t))
#define STORAGE_VISSPRITES	(128*sizeof(vissprite_t))
#define STORAGE_DRAWSEGS	(256*sizeof(drawseg_t))
#define STORAGE_ZLIGHT	(LIGHTLEVELS*MAXLIGHTZ*4)
extern void *storage_visplanes; // used for DECORATE lump
extern void *storage_vissprites; // used for DECORATE custom state names
extern void *storage_drawsegs; // used for DECORATE actor name list
extern void *storage_zlight; // used for sprite name list

void Z_Enlarge(void *ptr, uint32_t size) __attribute((regparm(2)));

