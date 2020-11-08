// kgsws' Doom ACE
// New map format.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "map.h"
#include "generic.h"
#include "render.h"
#include "decorate.h"

void bad_map_warning(); // asm.S

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
	int16_t type;
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

static void activate_special(line_t *ln, mobj_t *mo, int side) __attribute((regparm(2)));
static void map_LoadLineDefs(int) __attribute((regparm(2),no_caller_saved_registers));
static void map_LoadThings(int) __attribute((regparm(2),no_caller_saved_registers));
static void map_SpawnSpecials() __attribute((no_caller_saved_registers));
static void map_PlayerInSpecialSector(player_t*) __attribute((regparm(2),no_caller_saved_registers));
static int map_UseSpecialLine(mobj_t*, line_t*) __attribute((regparm(2),no_caller_saved_registers));
static void map_CrossSpecialLine(int,int) __attribute((regparm(2),no_caller_saved_registers));
static void map_ShootSpecialLine(mobj_t*,line_t*) __attribute((regparm(2),no_caller_saved_registers));

uint32_t *numlines;
uint32_t *numsectors;
line_t **lines;
vertex_t **vertexes;
side_t **sides;
sector_t **sectors;

static uint8_t skill_flag[] = {MTF_EASY, MTF_EASY, MTF_MEDIUM, MTF_HARD, MTF_HARD};
static line_t fakeline; // for thing specials

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
	// restore exit switch sound line special
	{0x00030368, CODE_HOOK | HOOK_UINT8, 11},
	// terminator
	{0}
};

static hook_t map_enable_reject[] = // TODO: broken sight check
{
	// enable rejectmatrix
	{0x0002ed69, CODE_HOOK | HOOK_UINT16, 0x508B}, // original value
	// terminator
	{0}
};

static hook_t map_disable_reject[] = // TODO: fix sight check
{
	// disable rejectmatrix
	{0x0002ed69, CODE_HOOK | HOOK_UINT16, 0x73EB}, // 'jmp'
	// terminator
	{0}
};

void map_init()
{
}

//
// special functions

static __attribute((no_caller_saved_registers))
void map_SpawnSpecials()
{
	// must call original initialization too
	P_SpawnSpecials();

	// count secrets
	for(int i = 0; i < *numsectors; i++)
	{
		if((*sectors)[i].special & 1024)
			(*totalsecret)++;
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
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
//	P_PlayerInSpecialSector(pl);
}

static __attribute((regparm(2),no_caller_saved_registers))
int map_UseSpecialLine(mobj_t *mo, line_t *ln)
{
	register int side asm("ebx");

	if(side)
		// wrong side
		return 1;

	if((ln->flags & 0x1C00) != 0x0400 && (ln->flags & 0x1C00) != 0x1800)
		// not a 'use' special
		return 1;

	if(!mo->player && !(ln->flags & 0x2000))
		// monsters can't use this
		return 0;

	activate_special(ln, mo, side);

	if(mo->player)
		// passtrough for player
		return (ln->flags & 0x1C00) == 0x1800;

	// continue checking for monster
	return 1;
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

__attribute((regparm(2),no_caller_saved_registers))
void map_ItemPickup(mobj_t *item, mobj_t *mo)
{
	if(item->type < NUMMOBJTYPES)
	{
		// original Doom code
		item->dehacked_sprite = item->sprite - 1;
		P_TouchSpecialThing(item, mo);
		// addon for specials
		if(item->thinker.function != (void*)-1)
			// item was not removed (not picked up)
			return;
	}

	// TODO: decorate items

	if(item->special)
	{
		fakeline.special = item->special;
		fakeline.tag = item->arg0;
		fakeline.specialdata = (void*)item->args;
		activate_special(&fakeline, mo, 0);
	}
}

//
// map loading

__attribute((regparm(2),no_caller_saved_registers))
int32_t map_get_map_lump(char *name)
{
	int lumpnum;
	uint32_t tmp;

	// TODO: custom names

	lumpnum = W_GetNumForName(name);

	// check for hexen format
	if(lumpnum + ML_BEHAVIOR < numlumps && lumpinfo[lumpnum + ML_BEHAVIOR].wame == 0x524f495641484542)
		// new map format
		utils_install_hooks(map_load_hooks_new);
	else
		// old map format
		utils_install_hooks(map_load_hooks_old);

	// check some limits here so game does not have to crash
	tmp = W_LumpLength(lumpnum + ML_SEGS);
	if(!tmp)
	{
		M_StartMessage("This map does not use DOOM nodes.\n", NULL, 0);
		goto bail_out;
	}
	if((tmp / 12) > 32767)
	{
		M_StartMessage("This map has too many segs.\n", NULL, 0);
		goto bail_out;
	}
	tmp = W_LumpLength(lumpnum + ML_SIDEDEFS);
	if((tmp / 30) > 32767)
	{
		M_StartMessage("This map has too many sidedefs.\n", NULL, 0);
		goto bail_out;
	}

	// check 'reject'
	tmp = W_LumpLength(lumpnum + ML_REJECT);
	if(tmp)
		utils_install_hooks(map_enable_reject);
	else
		utils_install_hooks(map_disable_reject);

	return lumpnum;

bail_out:
	D_StartTitle();
	bad_map_warning();
	return -1;
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t map_get_spawn_type(uint32_t ednum)
{
	// scan backwards
	uint32_t idx = decorate_num_mobjinfo - 1;
	do
	{
		if(mobjinfo[idx].doomednum == ednum)
			return idx;
	} while(--idx);
	return 35; // this is 'unknown item' (29=spawnfire; 35=bfgball)
}

static __attribute((regparm(2),no_caller_saved_registers))
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

static __attribute((regparm(2),no_caller_saved_registers))
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

		if(mt->type < 1)
			continue;

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
		for(idx = decorate_num_mobjinfo - 1; idx; idx--)
			if(mobjinfo[idx].doomednum == mt->type)
				break;

		if(!idx)
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
		if(mt->flags & MTF_INACTIVE)
		{
			// TODO
			mo->new_flags |= MFN_INACTIVE;
			if(mo->flags & MF_SHOOTABLE)
			{
				mo->flags |= MF_NOBLOOD; // this is a hack to disable blood
				mo->health = -mo->health; // this is a hack to disable damage, crusher breaks this
			}
			mo->tics = -1;
		} else
		{
			// random tick
			if(mo->tics > 0)
				mo->tics = 1 + (P_Random() % mo->tics);
		}

		// color hack for demo
		if(mt->flags & MTF_STANDSTILL)
			mo->new_flags |= MFN_COLORHACK;

		// place special & flags
		mo->special = mt->special;
		mo->arg0 = mt->arg0;
		mo->args = mt->args;
		mo->tid = mt->tid;
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
void spec_DoorGeneric(sector_t *sec, fixed_t speed, int delay, int lighttag, int lightmin, int lightmax)
{
	// custom sounds can be supported
	// even new sound behavior
	// crushing doors can be defined too
	generic_mover_info_t info;

	// direction
	if(speed > 0)
	{
		// open
		info.start = sec->floorheight;
		info.stop = P_FindLowestCeilingSurrounding(sec) - 4 * FRACUNIT;
		info.speed = speed;
	} else
	{
		// close
		info.start = P_FindLowestCeilingSurrounding(sec) - 4 * FRACUNIT;
		info.stop = sec->floorheight;
		info.speed = -speed;
	}

	// the check
	if(info.stop == sec->ceilingheight && delay < 0)
		return;

	// sound by speed
	if(info.speed > 4 * FRACUNIT)
	{
		info.opensound = sfx_bdopn;
		info.closesound = sfx_bdcls;
	} else
	{
		info.opensound = sfx_doropn;
		info.closesound = sfx_dorcls;
	}

	if(info.stop != sec->ceilingheight)
		S_StartSound((mobj_t *)&sec->soundorg, speed < 0 ? info.closesound : info.opensound);

	// the rest
	info.crushspeed = 0;
	info.delay = delay;
	info.stopsound = 0;
	info.movesound = 0;
	info.sleeping = 0;
	info.lighttag = lighttag;
	info.lightmin = lightmin;
	info.lightmax = lightmax;

	generic_door(sec, &info);
}

static __attribute((regparm(2)))
int specDoorLight(int lighttag, uint8_t *lmin, uint8_t *lmax)
{
	uint8_t lightmax = 0;
	uint8_t lightmin = 255;

	if(!lighttag)
		return 0;

	sector_t *sec = *sectors;
	for(int i = 0; i < *numsectors; i++, sec++)
	{
		if(sec->tag == lighttag)
		{
			if(sec->light > lightmax)
				lightmax = sec->light;
			if(sec->light < lightmin)
				lightmin = sec->light;

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
					if(s2->light > lightmax)
						lightmax = s2->light;
					if(s2->light < lightmin)
						lightmin = s2->light;
				}
			}
		}
	}

	if(lightmax <= lightmin)
		return 0;

	*lmin = lightmin;
	*lmax = lightmax;

	return lighttag;
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
		// HACK FOR DEMO
		case 80:
		{
			sector_t *sec = *sectors;
			uint8_t step = ln->arg1 + 1;

			if(step == 7)
				step = 0;
			ln->arg1 = step;

			// change colormap
			for(int i = 0; i < *numsectors; i++, sec++)
			{
				if(sec->tag == ln->tag)
				{
					sec->colormap = step;
					success = 1;
				}
			}
		}
		case 81:
		{
			static uint8_t tidx_torch[] = {0, 5, 6, 7, 8, 9};
			void *translation;
			uint8_t idx;

			if(ln->arg1)
			{
				// monsters
				idx = ln->arg2 + 1;
				if(idx >= 5)
					idx = 0;
				ln->arg2 = idx;
			} else
			{
				// green stuff
				idx = ln->arg2 + 1;
				if(idx >= sizeof(tidx_torch))
					idx = 0;
				ln->arg2 = idx;
				idx = tidx_torch[idx];
			}

			translation = render_get_translation(idx);

			if(!ln->tag)
			{
				mo->translation = translation;
				success = 1;
			} else
			{
				thinker_t *th;
				for(th = thinkercap->next; th != thinkercap; th = th->next)
				{
					if(th->function != ptr_MobjThinker)
						continue;

					mobj_t *tt = (mobj_t*)th;
					if(tt->tid != ln->tag)
						continue;

					tt->translation = translation;
					success = 1;
				}
			}
		}
		// normal functions
		break;
		case 10: // Door_Close
		case 11: // Door_Open
		case 12: // Door_Raise
		{
			int16_t delay;
			fixed_t speed = ln->arg1 << (FRACBITS - 3);
			uint16_t lighttag;
			uint8_t lightmax = 0;
			uint8_t lightmin = 255;

			if(ln->special == 12)
			{
				delay = ln->arg2;
				lighttag = ln->arg3;
			} else
			{
				delay = -1;
				lighttag = ln->arg2;
			}

			if(ln->special == 10)
				speed = -speed;

			// check for lights
			lighttag = specDoorLight(lighttag, &lightmin, &lightmax);

			// activate
			if(ln->tag)
			{
				sector_t *sec = *sectors;

				// activate all
				for(int i = 0; i < *numsectors; i++, sec++)
				{
					if(sec->tag == ln->tag && !sec->specialdata)
					{
						spec_DoorGeneric(sec, speed, delay, lighttag, lightmin, lightmax);
						success = 1;
					}
				}
			} else
			{
				if(side || !ln->backsector)
					return;

				if(ln->backsector->specialdata)
				{
					if(delay < 0)
						// wrong type
						return;
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

				spec_DoorGeneric(ln->backsector, speed, delay, lighttag, lightmin, lightmax);
				success = 1;
			}
		}
		break;
		case 20:
		case 23:
		{
			fixed_t speed = ln->arg1 << (FRACBITS - 3);
			fixed_t value = ln->arg2 << FRACBITS;
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
			fixed_t speed = ln->arg1 << (FRACBITS - 3);
			fixed_t value = ln->arg2 << FRACBITS;
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
		case 119: // Thing_Damage
		{
			if(!ln->tag) // arg0
			{
				P_DamageMobj(mo, NULL, NULL, ln->arg1);
				success = 1;
			} else
			{
				thinker_t *th;
				for(th = thinkercap->next; th != thinkercap; th = th->next)
				{
					if(th->function != ptr_MobjThinker)
						continue;

					mobj_t *tt = (mobj_t*)th;
					if(tt->tid != ln->tag)
						continue;

					P_DamageMobj(tt, NULL, NULL, ln->arg1);
					success = 1;
				}
			}
		}
		break;
		case 128: // ThrustThingZ
		{
			uint32_t momz = ln->arg1 << (FRACBITS-2);

			if(ln->arg2)
				momz = -momz;

			if(!ln->tag)
			{
				spec_ThrustThingZ(mo, momz, ln->arg3);
				success = 1;
			} else
			{
				thinker_t *th;
				for(th = thinkercap->next; th != thinkercap; th = th->next)
				{
					if(th->function != ptr_MobjThinker)
						continue;

					mobj_t *tt = (mobj_t*)th;
					if(tt->tid != ln->tag)
						continue;

					spec_ThrustThingZ(tt, momz, ln->arg3);
					success = 1;
				}
			}
		}
		break;
		case 130: // Thing_Activate
		{
			// TODO: propper handling
			if(ln->tag)
			{
				thinker_t *th;
				for(th = thinkercap->next; th != thinkercap; th = th->next)
				{
					if(th->function != ptr_MobjThinker)
						continue;

					mobj_t *tt = (mobj_t*)th;
					if(tt->tid != ln->tag)
						continue;

					if(!(tt->new_flags & MFN_INACTIVE))
						continue;

					if(tt->flags & MF_SHOOTABLE)
					{
						tt->health = -tt->health;
						if(!(tt->info->flags & MF_NOBLOOD))
							tt->flags &= ~MF_NOBLOOD;
						tt->tics = 2;
					}

					success = 1;
				}
			}
		}
		break;
		case 243: // Exit_Normal
			G_ExitLevel();
//			I_Error("Memory:\nCODE: 0x%08X\nDATA: 0x%08X\nZone: 0x%08X", doom_code_segment, doom_data_segment, *((uint32_t*)(relocate_addr_data(0x00074FE0))));
		break;
		case 208: // TranslucentLine
			// do nothing
			return;
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

