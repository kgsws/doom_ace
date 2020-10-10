// kgsws' Doom ACE
// New map format.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "map.h"
#include "generic.h"

#define LNSPEC_ARG(x,y)	(((uint32_t)(x) >> ((y-1) * 8)) & 0xFF)

typedef struct
{
	uint16_t v1, v2;
	uint16_t flags;
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
	int16_t sidenum[2];
} __attribute__((packed)) new_maplinedef_t;

typedef struct
{
	uint16_t tid;
	uint16_t x, y, z;
	uint16_t angle;
	uint16_t type;
	uint16_t flags;
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
} __attribute__((packed)) new_mapthing_t;

typedef struct
{
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
	uint8_t newflags;
} __attribute__((packed)) mobj_extra_t;

static void activate_special(line_t *ln, mobj_t *mo, int side) __attribute((regparm(2)));

static int get_map_lump(char*) __attribute((regparm(1),no_caller_saved_registers));
static void map_LoadLineDefs(int) __attribute((regparm(1),no_caller_saved_registers));
static void map_LoadThings(int) __attribute((regparm(1),no_caller_saved_registers));
static void map_SpawnSpecials() __attribute((no_caller_saved_registers));
static void map_PlayerInSpecialSector(player_t*) __attribute((regparm(1),no_caller_saved_registers));
static int map_UseSpecialLine(mobj_t*, line_t*) __attribute((regparm(2),no_caller_saved_registers));
static void map_CrossSpecialLine(int,int) __attribute((regparm(2),no_caller_saved_registers));
static void map_ShootSpecialLine(mobj_t*,line_t*) __attribute((regparm(2),no_caller_saved_registers));
static void map_ItemPickup(mobj_t*,mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

uint32_t *numlines;
uint32_t *numsectors;
line_t **lines;
vertex_t **vertexes;
side_t **sides;
sector_t **sectors;

uint32_t *totalitems;
uint32_t *totalkills;

thinker_t *thinkercap;
void *ptr_MobjThinker;

static uint8_t skill_flag[] = {MTF_EASY, MTF_EASY, MTF_MEDIUM, MTF_HARD, MTF_HARD};
static line_t fakeline; // for thing specials

static hook_t hook_list[] =
{
	// replace call to W_GetNumForName in P_SetupLevel
	{0x0002e8c1, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)get_map_lump},
	// required variables
	{0x0002C134, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numlines},
	{0x0002C14C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsectors},
	{0x0002C120, DATA_HOOK | HOOK_IMPORT, (uint32_t)&lines},
	{0x0002C138, DATA_HOOK | HOOK_IMPORT, (uint32_t)&vertexes},
	{0x0002C118, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sides},
	{0x0002C148, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sectors},
	{0x0002CF74, DATA_HOOK | HOOK_IMPORT, (uint32_t)&thinkercap},
	{0x00031490, CODE_HOOK | HOOK_IMPORT, (uint32_t)&ptr_MobjThinker},
	// less required variables
	{0x0002B3D0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalitems},
	{0x0002B3D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalkills},
	// change mobj_t size - add extra space for new stuff
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
	// replace call to item pickup
	{0x0002b032, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)map_ItemPickup},
	// change exit switch sound line special
	{0x00030368, CODE_HOOK | HOOK_UINT8, 243},
	// terminator
	{0}
};

static hook_t map_load_hooks_old[] =
{
	// restore call to map format specific lump loading
	{0x0002e8f3, CODE_HOOK | HOOK_RELADDR_DOOM, 0x0002E220},
	{0x0002e93b, CODE_HOOK | HOOK_RELADDR_DOOM, 0x0002E180},
	// restore call to specials initialization
	{0x0002e982, CODE_HOOK | HOOK_RELADDR_DOOM, 0x0002FFF0},
	{0x00030191, CODE_HOOK | HOOK_UINT16, 0xff66}, // original values
	// restore call to player sector function
	{0x000333e1, CODE_HOOK | HOOK_RELADDR_DOOM, 0x0002FB20},
	// restore call to 'use line' function
	{0x0002bcff, CODE_HOOK | HOOK_RELADDR_DOOM, 0x00030710}, // by player
	{0x00027287, CODE_HOOK | HOOK_RELADDR_DOOM, 0x00030710}, // by monster
	{0x0002bd03, CODE_HOOK | HOOK_UINT16, 0xC031}, // 'xor %eax,%eax'
	// restore call to 'cross line' function
	{0x0002b341, CODE_HOOK | HOOK_RELADDR_DOOM, 0x0002F500},
	// restore call to 'shoot line' function
	{0x0002b907, CODE_HOOK | HOOK_RELADDR_DOOM, 0x0002FA70},
	// restore call to item pickup
	{0x0002b032, CODE_HOOK | HOOK_RELADDR_DOOM, 0x00029AE0},
	// restore exit switch sound line special
	{0x00030368, CODE_HOOK | HOOK_UINT8, 11},
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
	if(pl->mo->subsector->sector->special & 1024)
	{
		// new secret message
		pl->mo->subsector->sector->special &= ~1024;
		pl->message = "SECRET!";
		pl->secretcount++;
		S_StartSound(NULL, sfx_radio);
		if(!pl->mo->subsector->sector->special)
			return;
	}
	// run the original
	P_PlayerInSpecialSector(pl);
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

static __attribute((regparm(2),no_caller_saved_registers))
void map_ItemPickup(mobj_t *item, mobj_t *mo)
{
	// original Doom code
	P_TouchSpecialThing(item, mo);

	// addon for specials
	if(item->thinker.function != (void*)-1)
		// item was not removed (not picked up)
		return;

	mobj_extra_t *me = (mobj_extra_t*)&item->spawnpoint;
	if(me->special)
	{
		fakeline.special = me->special;
		fakeline.tag = me->arg0;
		fakeline.specialdata = (void*)me->args;
		activate_special(&fakeline, mo, 0);
	}
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
		ln->special = ml->special;
		ln->tag = ml->arg0; // arg[0] is often used as a tag
		ln->specialdata = (void*)(ml->args); // abuse this space for more args
		ln->sidenum[0] = ml->sidenum[0];
		ln->sidenum[1] = ml->sidenum[1];
		ln->validcount = 0;

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

		// cool new stuff
		mobj_extra_t *me = (mobj_extra_t*)&mo->spawnpoint;

		if(mt->flags & MTF_INACTIVE)
		{
			// TODO
			me->newflags |= MFN_INACTIVE;
			if(mo->flags & MF_SHOOTABLE)
			{
				mo->flags |= MF_NOBLOOD; // this is a hack to disable blood
				mo->health = -mo->health; // this is a hack to disable damage
			}
			mo->tics = -1;
		} else
		{
			// random tick
			if(mo->tics > 0)
				mo->tics = 1 + (P_Random() % mo->tics);
		}

		// place special & flags
		me->special = mt->special;
		me->arg0 = mt->arg0;
		me->args = mt->args;
		me->tag = mt->tid;
	}

	Z_Free(buff);
}

//
// line special functions

static __attribute((regparm(2)))
void spec_CeilingMove(sector_t *sec, fixed_t speed, fixed_t dest)
{
	// custom sounds can be supported
	// even new sound behavior
	generic_mover_info_t info;

	info.start = sec->ceilingheight;
	info.stop = dest;
	info.speed = speed;
	info.crushspeed = 0;
	info.stopsound = 0;
	info.movesound = sfx_stnmov;
	info.texture = sec->ceilingpic; // no change
	info.special = sec->special; // no change
	info.light = sec->lightlevel; // no change

	generic_ceiling(sec, &info);
}

static __attribute((regparm(2)))
void spec_FloorMove(sector_t *sec, fixed_t speed, fixed_t dest)
{
	// custom sounds can be supported
	// even new sound behavior
	generic_mover_info_t info;

	info.start = sec->floorheight;
	info.stop = dest;
	info.speed = speed;
	info.crushspeed = 0;
	info.stopsound = 0;
	info.movesound = sfx_stnmov;
	info.texture = sec->floorpic; // no change
	info.special = sec->special; // no change
	info.light = sec->lightlevel; // no change

	generic_floor(sec, &info);
}

static __attribute((regparm(2)))
void spec_DoorOpen(sector_t *sec, fixed_t speed, int delay, int lighttag, int lightmin, int lightmax)
{
	// custom sounds can be supported
	// even new sound behavior
	// crushing doors can be defined too
	generic_mover_info_t info;

	info.start = sec->floorheight;
	info.stop = P_FindLowestCeilingSurrounding(sec) - 4 * FRACUNIT;
	info.speed = speed;
	info.crushspeed = 0;
	info.delay = delay;
	info.stopsound = 0;
	info.movesound = 0;
	info.sleeping = 0;
	info.lighttag = lighttag;
	info.lightmin = lightmin;
	info.lightmax = lightmax;

	if(speed > 4 * FRACUNIT)
	{
		info.opensound = sfx_bdopn;
		info.closesound = sfx_bdcls;
	} else
	{
		info.opensound = sfx_doropn;
		info.closesound = sfx_dorcls;
	}

	S_StartSound((mobj_t *)&sec->soundorg, info.opensound);
	generic_door(sec, &info);
}

static __attribute((regparm(2)))
void spec_ThrustThingZ(mobj_t *mo, fixed_t momz, int add)
{
	if(add)
		mo->momz += momz;
	else
		mo->momz = momz;
}

static __attribute((regparm(2)))
void activate_special(line_t *ln, mobj_t *mo, int side)
{
	// TODO: implement all
	int success = 0;

	switch(ln->special)
	{
		case 12: // Door_Raise // TODO: lighttag
		{
			fixed_t speed = LNSPEC_ARG(ln->specialdata, 1) << (FRACBITS - 3);
			uint16_t delay = LNSPEC_ARG(ln->specialdata, 2);
			uint16_t lighttag = LNSPEC_ARG(ln->specialdata, 3);
			uint8_t lightmax = 0;
			uint8_t lightmin = 255;

			// check for lights
			if(lighttag)
			{
				sector_t *sec = *sectors;
				for(int i = 0; i < *numsectors; i++, sec++)
				{
					if(sec->tag == lighttag)
					{
						for(int j = 0; j < sec->linecount; j++)
						{
							line_t *ln = sec->lines[j];
							if(ln->flags & ML_TWOSIDED)
							{
								sector_t *s2;
								if(ln->frontsector == sec)
									s2 = ln->backsector;
								else
									s2 = ln->frontsector;
								if(s2->lightlevel > lightmax)
									lightmax = s2->lightlevel;
								if(s2->lightlevel < lightmin)
									lightmin = s2->lightlevel;
							}
						}
					}
				}
				if(lightmax <= lightmin)
					lighttag = 0;
			}

			if(ln->tag)
			{
				sector_t *sec = *sectors;

				// activate all
				for(int i = 0; i < *numsectors; i++, sec++)
				{
					if(sec->tag == ln->tag && !sec->specialdata)
					{
						spec_DoorOpen(sec, speed, delay, lighttag, lightmin, lightmax);
						success = 1;
					}
				}
			} else
			{
				if(side || !ln->backsector)
					return;

				if(ln->backsector->specialdata)
				{
					if(!mo->player)
						// don't even try
						return;
					if(((thinker_t*)ln->backsector->specialdata)->function == think_door_mover)
					{
						// only change direction
						generic_door_toggle(ln->backsector->specialdata);
						success = 1;
					}
					// busy
					return;
				}

				spec_DoorOpen(ln->backsector, speed, delay, lighttag, lightmin, lightmax);
				success = 1;
			}
		}
		break;
		case 20:
		case 23:
		{
			fixed_t speed = LNSPEC_ARG(ln->specialdata, 1) << (FRACBITS - 3);
			fixed_t value = LNSPEC_ARG(ln->specialdata, 2) << FRACBITS;
			sector_t *sec = *sectors;

			if(ln->special == 20)
				value = -value;

			// activate all
			for(int i = 0; i < *numsectors; i++, sec++)
			{
				if(sec->tag == ln->tag && !sec->specialdata)
				{
					spec_FloorMove(sec, speed, sec->floorheight + value);
					success = 1;
				}
			}
		}
		case 40: // Ceiling_LowerByValue
		case 41: // Ceiling_RaiseByValue
		{
			fixed_t speed = LNSPEC_ARG(ln->specialdata, 1) << (FRACBITS - 3);
			fixed_t value = LNSPEC_ARG(ln->specialdata, 2) << FRACBITS;
			sector_t *sec = *sectors;

			if(ln->special == 40)
				value = -value;

			// activate all
			for(int i = 0; i < *numsectors; i++, sec++)
			{
				if(sec->tag == ln->tag && !sec->specialdata)
				{
					spec_CeilingMove(sec, speed, sec->ceilingheight + value);
					success = 1;
				}
			}
		}
		break;
		case 73: // DamageThing
		{
			register uint32_t dmg = ln->tag; // arg0
			if(!dmg)
				dmg = 10000;
			P_DamageMobj(mo, NULL, NULL, dmg);
			success = 1;
		}
		break;
		case 128: // ThrustThingZ
		{
			uint32_t momz = LNSPEC_ARG(ln->specialdata, 1) << (FRACBITS-2);

			if(LNSPEC_ARG(ln->specialdata, 2))
				momz = -momz;

			if(!ln->tag)
			{
				spec_ThrustThingZ(mo, momz, LNSPEC_ARG(ln->specialdata, 3));
				success = 1;
			} else
			{
				thinker_t *th;
				for(th = thinkercap->next; th != thinkercap; th = th->next)
				{
					if(th->function != ptr_MobjThinker)
						continue;

					mobj_extra_t *me = (mobj_extra_t*)((void*)th + offsetof(mobj_t, spawnpoint));
					if(me->tag != ln->tag)
						continue;

					spec_ThrustThingZ((mobj_t*)th, momz, LNSPEC_ARG(ln->specialdata, 3));
					success = 1;
				}
			}
		}
		break;
		case 243: // Exit_Normal
			G_ExitLevel();
		break;
		default:
			I_Error("TODO: special %d by %p side %d\n", ln->special, mo, side);
		break;
	}

	if(ln == &fakeline)
		return;

	if(!success)
		return;

	P_ChangeSwitchTexture(ln, ln->flags & 0x0200);
}

