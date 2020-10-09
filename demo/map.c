// kgsws' Doom ACE
// New map format.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "map.h"

typedef struct
{
	uint16_t v1, v2;
	uint16_t flags;
	uint8_t special;
	uint8_t arg[5];
	int16_t sidenum[2];
} new_maplinedef_t;

typedef struct
{
	uint16_t tid;
	uint16_t x, y, z;
	uint16_t angle;
	uint16_t type;
	uint16_t flags;
	uint8_t special;
	uint8_t arg[5];
} new_mapthing_t;

typedef struct
{
	// partialy overlaps mapthing_t
	int16_t x, y;
	uint16_t angle;
	uint16_t type;
	uint8_t special;
	uint8_t arg[3]; // TODO: more args
} mobj_extra_t;

static void activate_special(line_t *ln, mobj_t *mo, int side) __attribute((regparm(2)));

static int get_map_lump(char*) __attribute((regparm(1),no_caller_saved_registers));
static void map_LoadLineDefs(int) __attribute((regparm(1),no_caller_saved_registers));
static void map_LoadThings(int) __attribute((regparm(1),no_caller_saved_registers));
static void map_SpawnSpecials() __attribute((no_caller_saved_registers));
static void map_PlayerInSpecialSector(player_t*) __attribute((regparm(1),no_caller_saved_registers));
static int map_UseSpecialLine(mobj_t*, line_t*) __attribute((regparm(2),no_caller_saved_registers));
static void map_CrossSpecialLine(int,int) __attribute((regparm(2),no_caller_saved_registers));
static void map_ShootSpecialLine(mobj_t*,line_t*) __attribute((regparm(2),no_caller_saved_registers));

uint32_t *numlines;
line_t **lines;
vertex_t **vertexes;
side_t **sides;

uint32_t *totalitems;
uint32_t *totalkills;

static uint8_t skill_flag[] = {MTF_EASY, MTF_EASY, MTF_MEDIUM, MTF_HARD, MTF_HARD};

static hook_t hook_list[] =
{
	// replace call to W_GetNumForName in P_SetupLevel
	{0x0002e8c1, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)get_map_lump},
	// required variables
	{0x0002C134, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numlines},
	{0x0002C120, DATA_HOOK | HOOK_IMPORT, (uint32_t)&lines},
	{0x0002C138, DATA_HOOK | HOOK_IMPORT, (uint32_t)&vertexes},
	{0x0002C118, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sides},
	// lest required variables
	{0x0002B3D0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalitems},
	{0x0002B3D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalkills},
	// change mobj_t size - Z_Malloc forces 4 byte align anyway
	{0x00031552, CODE_HOOK | HOOK_UINT32, sizeof(mobj_t)}, // for Z_Malloc
	{0x00031563, CODE_HOOK | HOOK_UINT32, sizeof(mobj_t)}, // for memset
	// terminator
	{0}
};

static hook_t map_load_hooks_new[] =
{
	// replace call to map format specific lump loading
	{0x0002e8f3, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)map_LoadLineDefs},
	{0x0002e93b, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)map_LoadThings},
	// replace call to specials initialization
	{0x0002e982, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)map_SpawnSpecials},
	{0x00030191, CODE_HOOK | HOOK_UINT16, 0x05EB}, // disable Doom texture scrolling special
	// replace call to player sector function
	{0x000333e1, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)map_PlayerInSpecialSector},
	// replace call to 'use line' function
	{0x0002bcff, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)map_UseSpecialLine}, // by player
	{0x00027287, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)map_UseSpecialLine}, // by monster
	{0x0002bd03, CODE_HOOK | HOOK_UINT16, 0x9090}, // allow for 'pass trough' for player use
	// replace call to 'cross line' function
	{0x0002b341, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)map_CrossSpecialLine},
	// replace call to 'shoot line' function
	{0x0002b907, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)map_ShootSpecialLine},
	// terminator
	{0}
};

static hook_t map_load_hooks_old[] =
{
	// replace call to map format specific lump loading
	{0x0002e8f3, CODE_HOOK | HOOK_RELADDR_DOOM, 0x0002E220},
	{0x0002e93b, CODE_HOOK | HOOK_RELADDR_DOOM, 0x0002E180},
	// replace call to specials initialization
	{0x0002e982, CODE_HOOK | HOOK_RELADDR_DOOM, 0x0002FFF0},
	{0x00030191, CODE_HOOK | HOOK_UINT16, 0xff66}, // original values
	// replace call to player sector function
	{0x000333e1, CODE_HOOK | HOOK_RELADDR_DOOM, 0x0002FB20},
	// replace call to 'use line' function
	{0x0002bcff, CODE_HOOK | HOOK_RELADDR_DOOM, 0x00030710}, // by player
	{0x00027287, CODE_HOOK | HOOK_RELADDR_DOOM, 0x00030710}, // by monster
	{0x0002bd03, CODE_HOOK | HOOK_UINT16, 0xC031}, // 'xor %eax,%eax'
	// replace call to 'cross line' function
	{0x0002b341, CODE_HOOK | HOOK_RELADDR_ACE, 0x0002F500},
	// replace call to 'shoot line' function
	{0x0002b907, CODE_HOOK | HOOK_RELADDR_ACE, 0x0002FA70},
	// terminator
	{0}
};

void map_init()
{
	utils_install_hooks(hook_list);

	// fix all mobjtypes, set MF_ISMONSTER where needed
	mobjinfo[15].flags |= MF_ISMONSTER; // lost soul
	for(int i = 0; i < 137; i++)
		if(mobjinfo[i].flags & MF_COUNTKILL)
			mobjinfo[i].flags |= MF_ISMONSTER;
}

//
// special functions

static __attribute((no_caller_saved_registers))
void map_SpawnSpecials()
{
	// must call original initialization too
	P_SpawnSpecials();
}

static __attribute((regparm(1),no_caller_saved_registers))
void map_PlayerInSpecialSector(player_t *pl)
{
	I_Error("TODO: special sector (%d)\n", pl->mo->subsector->sector->special);
}

static __attribute((regparm(2),no_caller_saved_registers))
int map_UseSpecialLine(mobj_t *mo, line_t *ln)
{
	register int side asm("ebx");

	if((ln->flags & 0x1C00) != 0x0400 && (ln->flags & 0x1C00) != 0x1800)
		// not a 'use' special
		return 1;

	if(!mo->player && !(ln->flags & 0x2000))
		// monsters can't use this
		return 1;

	activate_special(ln, mo, side);

	if(mo->player)
		// passtrough for player
		return (ln->flags & 0x1C00) == 0x1800;

	// continue checking for monster
	return 0;
}

static __attribute((regparm(2),no_caller_saved_registers))
void map_CrossSpecialLine(int linenum, int side)
{
	register mobj_t *mo asm("ebx");
	line_t *ln = &(*lines)[linenum];

	if(mo->player)
	{
		if(ln->flags & 0x1C00)
			// not a 'player cross' special
			return;
	} else
	if(mo->flags & MF_ISMONSTER)
	{
		if((ln->flags & 0x1C00) != 0x0800)
			// not a 'monster cross' special
			return;
	} else
	if(mo->flags & MF_MISSILE)
	{
		if((ln->flags & 0x1C00) != 0x1400)
			// not a 'projectile cross' special
			return;
	} else
		// this thing can't activate stuff
		return;

	activate_special(ln, mo, side);
}

static __attribute((regparm(2),no_caller_saved_registers))
void map_ShootSpecialLine(mobj_t *mo, line_t *ln)
{
	if((ln->flags & 0x1C00) != 0x0C00)
		// not a 'shoot' special
		return;

	activate_special(ln, mo, 0);
}

//
// map loading

static __attribute((regparm(1),no_caller_saved_registers))
int get_map_lump(char *name)
{
	int lumpnum;

	// TODO: custom names

	lumpnum = W_GetNumForName(name);

	// check for hexen format
	if(lumpnum + ML_BEHAVIOR < numlumps && lumpinfo[lumpnum + ML_BEHAVIOR].wame == 0x524f495641484542)
		// new map format
		utils_install_hooks(map_load_hooks_new);
	else
		// old map format
		utils_install_hooks(map_load_hooks_old);

	return lumpnum;
}

static __attribute((regparm(1),no_caller_saved_registers))
void map_LoadLineDefs(int lump)
{
	int nl;
	line_t *ln;
	new_maplinedef_t *ml;
	void *buff;

	nl = W_LumpLength(lump) / sizeof(new_maplinedef_t);
	ln = Z_Malloc(nl * sizeof(line_t), PU_LEVEL, NULL);
	buff = W_CacheLumpNum(lump, PU_STATIC);
	ml = buff;

	*numlines = nl;
	*lines = ln;

	for(int i = 0; i < nl; i++, ln++, ml++)
	{
		vertex_t *v1 = &(*vertexes)[ml->v1];
		vertex_t *v2 = &(*vertexes)[ml->v2];

		ln->v1 = v1;
		ln->v2 = v2;
		ln->dx = v2->x - v1->x;
		ln->dy = v2->y - v1->y;
		ln->flags = ml->flags;
		ln->special = ml->special | (ml->arg[0] << 8);
		ln->tag = ml->arg[1] | (ml->arg[2] << 8); // TODO: more args
		ln->sidenum[0] = ml->sidenum[0];
		ln->sidenum[1] = ml->sidenum[1];
		ln->validcount = 0;
		ln->specialdata = NULL;

		if(v1->x < v2->x)
		{
			ln->bbox[BOXLEFT] = v1->x;
			ln->bbox[BOXRIGHT] = v2->x;
		} else
		{
			ln->bbox[BOXLEFT] = v2->x;
			ln->bbox[BOXRIGHT] = v1->x;
		}
		if(v1->y < v2->y)
		{
			ln->bbox[BOXBOTTOM] = v1->y;
			ln->bbox[BOXTOP] = v2->y;
		} else
		{
			ln->bbox[BOXBOTTOM] = v2->y;
			ln->bbox[BOXTOP] = v1->y;
		}

		if(!ln->dx)
			ln->slopetype = ST_VERTICAL;
		else
		if(!ln->dy)
			ln->slopetype = ST_HORIZONTAL;
		else
		{
			if(FixedDiv(ln->dy, ln->dx) > 0)
				ln->slopetype = ST_POSITIVE;
			else
				ln->slopetype = ST_NEGATIVE;
		}

		if(ln->sidenum[0] != -1)
			ln->frontsector = (*sides)[ln->sidenum[0]].sector;
		else
			ln->frontsector = NULL;

		if(ln->sidenum[1] != -1)
			ln->backsector = (*sides)[ln->sidenum[1]].sector;
		else
			ln->backsector = NULL;
	}

	Z_Free(buff);
}

static __attribute((regparm(1),no_caller_saved_registers))
void map_LoadThings(int lump)
{
	mapthing_t ot;
	new_mapthing_t *mt;
	void *buff;
	int count, idx;

	buff = W_CacheLumpNum(lump, PU_STATIC);
	count = W_LumpLength(lump) / sizeof(new_mapthing_t);
	mt = buff;

	for(int i = 0; i < count; i++, mt++)
	{
		ot.x = mt->x;
		ot.y = mt->y;
		ot.angle = mt->angle;
		ot.type = mt->type;

		if(mt->type == 11)
			// TODO: deathmatch start
			continue;

		if(mt->type <= 4)
		{
			// TODO: playerstarts
			if(!*deathmatch)
				P_SpawnPlayer(&ot);
		}

		// skill checks
		if(*gameskill < sizeof(skill_flag) && !(mt->flags & skill_flag[*gameskill]))
			continue;

		// flag checks
		if(*netgame)
		{
			if(*deathmatch)
			{
				if(!(mt->flags & MTF_DEATHMATCH))
					continue;
			} else
			{
				if(!(mt->flags & MTF_COOPERATIVE))
					continue;
			}
		} else
		{
			if(!(mt->flags & MTF_SINGLE))
				continue;
		}

		// find this thing
		for(idx = 0; idx < NUMMOBJTYPES; idx++)
			if(mobjinfo[idx].doomednum == mt->type)
				break;

		if(idx >= NUMMOBJTYPES)
			// maybe spawn something instead
			continue;

		// check spawn flags
		if(*deathmatch && mobjinfo[idx].flags & MF_NOTDMATCH)
			continue;

		// TODO: nomonsters ?

		// special position
		if(mobjinfo[idx].flags & MF_SPAWNCEILING)
			mt->z = 0x7FFF;

		// spawn the thing
		mobj_t *mo = P_SpawnMobj(mt->x << FRACBITS, mt->y << FRACBITS, mt->z << FRACBITS, idx);
		mo->spawnpoint = ot;

		// change Z
		if(!(mobjinfo[idx].flags & MF_SPAWNCEILING) && mo->subsector)
			mo->z += mo->subsector->sector->floorheight;

		// set angle
		mo->angle = ANG45 * (mt->angle / 45);

		// counters
		if(mo->flags & MF_COUNTKILL)
			*totalkills = *totalkills + 1;
		if(mo->flags & MF_COUNTITEM)
			*totalitems = *totalitems + 1;

		// more flags
		if(mt->flags & MTF_AMBUSH)
			mo->flags |= MF_AMBUSH;
		if(mt->flags & MTF_SHADOW)
			mo->flags |= MF_SHADOW;

		if(mt->flags & MTF_INACTIVE)
		{
			// TODO
			mo->flags |= MF_INACTIVE;
			mo->flags |= MF_NOBLOOD; // this is a hack to disable blood
			mo->health = 0; // this is a hack to disable damage
			mo->tics = -1;
		} else
		{
			// random tick
			if(mo->tics > 0)
				mo->tics = 1 + (P_Random() % mo->tics);
		}

		// place special & flags
		mobj_extra_t *me = (mobj_extra_t*)&mo->spawnpoint;
		me->special = mt->special;
		me->arg[0] = mt->arg[0];
		me->arg[1] = mt->arg[1];
		me->arg[2] = mt->arg[2];
	}

	Z_Free(buff);
}

//
// line special functions

static __attribute((regparm(2)))
void activate_special(line_t *ln, mobj_t *mo, int side)
{
	// TODO: implement all

	switch(ln->special & 0xFF) // 
	{
		case 0:
			// original Doom code is checking 16 bits, so this can happen
		break;
		case 73: // DamageThing
		{
			register uint32_t dmg = ln->special >> 8;
			if(!dmg)
				dmg = 10000;
			P_DamageMobj(mo, NULL, NULL, dmg);
		}
		break;
		default:
			I_Error("TODO: special %d by %p side %d\n", ln->special, mo, side);
		break;
	}
}

