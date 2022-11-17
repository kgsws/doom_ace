// kgsws' ACE Engine
////
// ZDoom line specials.
// ZDoom sector specials.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "player.h"
#include "mobj.h"
#include "map.h"
#include "render.h"
#include "action.h"
#include "sound.h"
#include "decorate.h"
#include "animate.h"
#include "generic.h"
#include "special.h"

enum
{
	MVT_GENERIC, // must be zero
	MVT_DOOR,
	MVT_CRUSH_CEILING,
};

//

line_t spec_magic_line;
uint32_t spec_success;

static mobj_t *activator;

static uint_fast8_t door_monster_hack;
static fixed_t value_mult;
static fixed_t value_offs;
static int32_t spawn_type;

fixed_t nearest_up;
fixed_t nearest_dn;

//
// missing height search

void find_nearest_floor(sector_t *sec)
{
	nearest_up = ONCEILINGZ;
	nearest_dn = ONFLOORZ;

	for(uint32_t i = 0; i < sec->linecount; i++)
	{
		line_t *li = sec->lines[i];
		sector_t *bs;

		if(li->frontsector == sec)
			bs = li->backsector;
		else
			bs = li->frontsector;
		if(!bs)
			continue;

		if(bs->floorheight < sec->floorheight && bs->floorheight > nearest_dn)
			nearest_dn = bs->floorheight;

		if(bs->floorheight > sec->floorheight && bs->floorheight < nearest_up)
			nearest_up = bs->floorheight;
	}

	if(nearest_up == ONCEILINGZ)
		nearest_up = sec->floorheight;
	if(nearest_dn == ONFLOORZ)
		nearest_dn = sec->floorheight;
}

void find_nearest_ceiling(sector_t *sec)
{
	nearest_up = ONCEILINGZ;
	nearest_dn = ONFLOORZ;

	for(uint32_t i = 0; i < sec->linecount; i++)
	{
		line_t *li = sec->lines[i];
		sector_t *bs;

		if(li->frontsector == sec)
			bs = li->backsector;
		else
			bs = li->frontsector;
		if(!bs)
			continue;

		if(bs->ceilingheight < sec->ceilingheight && bs->ceilingheight > nearest_dn)
			nearest_dn = bs->ceilingheight;

		if(bs->ceilingheight > sec->ceilingheight && bs->ceilingheight < nearest_up)
			nearest_up = bs->ceilingheight;
	}

	if(nearest_up == ONCEILINGZ)
		nearest_up = sec->ceilingheight;
	if(nearest_dn == ONFLOORZ)
		nearest_dn = sec->ceilingheight;
}

//
// Doors

static uint32_t act_Door_Close(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;

	if(sec->ceilingheight == sec->floorheight)
		return 1;

	if(sec->ceilingheight < sec->floorheight)
	{
		sec->ceilingheight = sec->floorheight;
		P_ChangeSector(sec, 0);
		return 1;
	}

	gm = generic_ceiling(sec, DIR_DOWN, SNDSEQ_DOOR, ln->arg1 >= 64);
	if(!gm)
		return 0;

	gm->top_height = sec->ceilingheight;
	gm->bot_height = sec->floorheight;
	gm->speed_now = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->flags = MVF_BLOCK_STAY;
	gm->lighttag = ln->arg2;

	return 1;
}

static uint32_t act_Door_Raise(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;
	fixed_t height;

	if(sec->specialactive & ACT_CEILING)
	{
		if(ln->arg0)
			return 0;

		// find existing door, reverse direction
		gm = generic_ceiling_by_sector(sec);

		if(!gm)
			return 0;

		if(gm->type != MVT_DOOR)
			return 0;

		if(door_monster_hack && gm->direction != DIR_DOWN)
			return 0;

		if(gm->wait)
			gm->wait = 0;
		else
			gm->direction = !gm->direction;

		if(gm->direction == DIR_UP)
		{
			if(gm->up_seq)
			{
				if(gm->up_seq->start)
					S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->start);
				gm->sndwait = gm->up_seq->delay;
			}
		} else
		{
			if(gm->dn_seq)
			{
				if(gm->dn_seq->start)
					S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->start);
				gm->sndwait = gm->dn_seq->delay;
			}
		}

		return 1;
	}

	height = P_FindLowestCeilingSurrounding(sec) - 4 * FRACUNIT;

	if(sec->ceilingheight == height)
		return 1;

	gm = generic_ceiling(sec, DIR_UP, SNDSEQ_DOOR, ln->arg1 >= 64);
	gm->top_height = height;
	gm->bot_height = sec->floorheight;
	gm->speed_now = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->speed_dn = gm->speed_now;
	gm->type = MVT_DOOR;
	gm->flags = MVF_BLOCK_GO_UP;

	if(value_mult)
	{
		gm->lighttag = ln->arg4;
		gm->delay = ln->arg2;
		if(!ln->arg2)
		{
			gm->speed_dn = 0;
			gm->flags = 0;
			gm->type = MVT_GENERIC;
		}
	} else
	{
		gm->lighttag = ln->arg3;
		if(!ln->arg2)
		{
			gm->delay = 1;
			gm->flags |= MVF_WAIT_STOP;
		} else
			gm->delay = ln->arg2;
	}

	return 1;
}

//
// floors & ceilings

static uint32_t act_Floor_ByValue(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;
	fixed_t dest;

	if(sec->specialactive & ACT_FLOOR)
		return 0;

	dest = sec->floorheight + (fixed_t)ln->arg2 * value_mult * FRACUNIT;
	if(dest > sec->ceilingheight)
		dest = sec->ceilingheight;

	if(dest == sec->floorheight)
		return 1;

	gm = generic_floor(sec, value_mult < 0 ? DIR_DOWN : DIR_UP, SNDSEQ_FLOOR, 0);
	gm->top_height = dest;
	gm->bot_height = dest;
	gm->speed_now = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->flags = MVF_BLOCK_STAY;

	if(ln->special == 239 && ln->frontsector)
	{
		sec->floorpic = ln->frontsector->floorpic;
		sec->special = ln->frontsector->special;
	}

	return 1;
}

static uint32_t act_Generic_Floor(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;
	fixed_t dest;
	uint16_t texture;
	uint16_t special;
	uint16_t flags = 0;

	if(sec->specialactive & ACT_FLOOR)
		return 0;

	switch(ln->arg3)
	{
		case 0:
			dest = sec->floorheight;
			if(ln->arg4 & 8)
				dest += (fixed_t)ln->arg2 * FRACUNIT;
			else
				dest -= (fixed_t)ln->arg2 * FRACUNIT;
		break;
		case 1:
			dest = P_FindHighestFloorSurrounding(sec);
		break;
		case 2:
			dest = P_FindLowestFloorSurrounding(sec);
		break;
		case 3:
			find_nearest_floor(sec);
			if(ln->arg4 & 8)
				dest = nearest_up;
			else
				dest = nearest_dn;
		break;
		case 4:
			dest = P_FindLowestCeilingSurrounding(sec);
		break;
		case 5:
			dest = sec->ceilingheight;
		break;
		default:
			return 0;
	}

	if(dest > sec->ceilingheight)
		dest = sec->ceilingheight;

	if(ln->arg4 & 3)
	{
		if(ln->arg4 & 4)
		{
			for(uint32_t i = 0; i < sec->linecount; i++)
			{
				line_t *li = sec->lines[i];
				sector_t *bs;

				if(li->frontsector == sec)
					bs = li->backsector;
				else
					bs = li->frontsector;
				if(!bs)
					continue;

				if(bs->floorheight == dest)
				{
					texture = bs->floorpic;
					special = bs->special;
					break;
				}
			}
		} else
		if(ln->frontsector)
		{
			texture = ln->frontsector->floorpic;
			special = ln->frontsector->special;
		} else
		{
			texture = sec->floorpic;
			special = sec->special;
		}
	}

	switch(ln->arg4 & 3)
	{
		case 1:
			special = 0;
		case 3:
			flags |= MVF_SET_TEXTURE | MVF_SET_SPECIAL;
		break;
		case 2:
			flags |= MVF_SET_TEXTURE;
		break;
		default:
			flags = 0;
		break;
	}

	if(dest == sec->floorheight)
	{
		if(flags & MVF_SET_TEXTURE)
			sec->floorpic = texture;
		if(flags & MVF_SET_SPECIAL)
			sec->special = special;
		return 1;
	}

	gm = generic_floor(sec, ln->arg4 & 8 ? DIR_UP : DIR_DOWN, SNDSEQ_FLOOR, 0);
	gm->top_height = dest;
	gm->bot_height = dest;
	gm->speed_now = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->flags = flags | MVF_BLOCK_STAY;
	gm->crush = ln->arg4 & 16 ? 0x8014 : 0;
	gm->texture = texture;
	gm->special = special;

	return 1;
}

static uint32_t act_Floor_Crush(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;
	fixed_t dest;

	if(sec->specialactive & ACT_FLOOR)
		return 0;

	if(value_mult)
		dest = P_FindLowestCeilingSurrounding(sec);
	else
		dest = sec->ceilingheight;

	dest -= 8 * FRACUNIT;

	gm = generic_floor(sec, DIR_UP, SNDSEQ_FLOOR, 0);
	gm->top_height = dest;
	gm->bot_height = dest;
	gm->speed_now = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->crush = 0x8000 | ln->arg2;

	switch(ln->arg3)
	{
		case 2:
			gm->flags = MVF_BLOCK_STAY;
		break;
		case 0:
			if(gm->speed_now != FRACUNIT)
				break;
		case 3:
			gm->flags = MVF_BLOCK_SLOW;
		break;
	}

	gm->flags |= MVF_WAIT_STOP;

	return 1;
}

static uint32_t act_Ceiling_ByValue(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;
	fixed_t dest;

	if(sec->specialactive & ACT_CEILING)
		return 0;

	dest = sec->ceilingheight + (fixed_t)ln->arg2 * value_mult * FRACUNIT;
	if(dest < sec->floorheight)
		dest = sec->floorheight;

	if(dest == sec->ceilingheight)
		return 1;

	gm = generic_ceiling(sec, value_mult < 0 ? DIR_DOWN : DIR_UP, SNDSEQ_CEILING, 0);
	gm->top_height = dest;
	gm->bot_height = dest;
	gm->speed_now = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->flags = MVF_BLOCK_STAY;

	return 1;
}

static uint32_t act_Generic_Ceiling(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;
	fixed_t dest;
	uint16_t texture;
	uint16_t special;
	uint16_t flags = 0;

	if(sec->specialactive & ACT_CEILING)
		return 0;

	switch(ln->arg3)
	{
		case 0:
			dest = sec->ceilingheight;
			if(ln->arg4 & 8)
				dest += (fixed_t)ln->arg2 * FRACUNIT;
			else
				dest -= (fixed_t)ln->arg2 * FRACUNIT;
		break;
		case 1:
			dest = P_FindHighestCeilingSurrounding(sec);
		break;
		case 2:
			dest = P_FindLowestCeilingSurrounding(sec);
		break;
		case 3:
			find_nearest_ceiling(sec);
			if(ln->arg4 & 8)
				dest = nearest_up;
			else
				dest = nearest_dn;
		break;
		case 4:
			dest = P_FindHighestFloorSurrounding(sec);
		break;
		case 5:
			dest = sec->floorheight;
		break;
		default:
			return 0;
	}

	if(dest < sec->floorheight)
		dest = sec->floorheight;

	if(ln->arg4 & 3)
	{
		if(ln->arg4 & 4)
		{
			for(uint32_t i = 0; i < sec->linecount; i++)
			{
				line_t *li = sec->lines[i];
				sector_t *bs;

				if(li->frontsector == sec)
					bs = li->backsector;
				else
					bs = li->frontsector;
				if(!bs)
					continue;

				if(bs->ceilingheight == dest)
				{
					texture = bs->ceilingpic;
					special = bs->special;
					break;
				}
			}
		} else
		if(ln->frontsector)
		{
			texture = ln->frontsector->ceilingpic;
			special = ln->frontsector->special;
		} else
		{
			texture = sec->ceilingpic;
			special = sec->special;
		}
	}

	switch(ln->arg4 & 3)
	{
		case 1:
			special = 0;
		case 3:
			flags |= MVF_SET_TEXTURE | MVF_SET_SPECIAL;
		break;
		case 2:
			flags |= MVF_SET_TEXTURE;
		break;
	}

	if(dest == sec->ceilingheight)
	{
		if(flags & MVF_SET_TEXTURE)
			sec->ceilingpic = texture;
		if(flags & MVF_SET_SPECIAL)
			sec->special = special;
		return 1;
	}

	gm = generic_ceiling(sec, ln->arg4 & 8 ? DIR_UP : DIR_DOWN, SNDSEQ_CEILING, 0);
	gm->top_height = dest;
	gm->bot_height = dest;
	gm->speed_now = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->flags = ln->arg4 & 16 ? 0 : MVF_BLOCK_STAY;
	gm->crush = ln->arg4 & 16 ? 0x8014 : 0;
	gm->flags |= flags;
	gm->texture = texture;
	gm->special = special;

	return 1;
}

static uint32_t act_Ceiling_Crush(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;
	fixed_t dest;

	if(sec->specialactive & ACT_CEILING)
	{
		if(!value_mult)
			return 0;

		gm = generic_ceiling_by_sector(sec);

		if(!gm)
			return 0;

		if(gm->type != MVT_CRUSH_CEILING)
			return 0;

		gm->wait = 0;

		return 0;
	}

	gm = generic_ceiling(sec, DIR_DOWN, SNDSEQ_CEILING, 0);
	gm->top_height = sec->ceilingheight;
	gm->bot_height = sec->floorheight;
	gm->speed_up = (fixed_t)ln->arg2 * (FRACUNIT/8);
	gm->speed_now = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->type = MVT_CRUSH_CEILING;
	gm->crush = 0x8000 | ln->arg3;
	if(value_mult)
		gm->speed_dn = gm->speed_now;

	switch(ln->arg4)
	{
		case 2:
			gm->flags = MVF_BLOCK_STAY;
		break;
		case 0:
			if(gm->speed_now != FRACUNIT)
				break;
		case 3:
			gm->flags = MVF_BLOCK_SLOW;
		break;
	}

	gm->flags |= MVF_WAIT_STOP;

	return 1;
}

static uint32_t act_Ceiling_LowerAndCrushDist(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;

	if(sec->specialactive & ACT_CEILING)
		return 0;

	gm = generic_ceiling(sec, DIR_DOWN, SNDSEQ_CEILING, 0);
	gm->bot_height = sec->floorheight + (fixed_t)ln->arg3 * FRACUNIT;
	gm->top_height = gm->bot_height;
	gm->speed_now = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->type = MVT_CRUSH_CEILING;
	gm->crush = 0x8000 | ln->arg2;

	switch(ln->arg4)
	{
		case 2:
			gm->flags = MVF_BLOCK_STAY;
		break;
		case 0:
			if(gm->speed_now != FRACUNIT)
				break;
		case 3:
			gm->flags = MVF_BLOCK_SLOW;
		break;
	}

	gm->flags |= MVF_WAIT_STOP;

	return 1;
}

static uint32_t act_Ceiling_CrushStop(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;

	if(!(sec->specialactive & ACT_CEILING))
		return 1;

	gm = generic_ceiling_by_sector(sec);
	if(!gm)
		return 1;

	if(gm->type != MVT_CRUSH_CEILING)
		return 1;

	gm->wait = 1;

	return 1;
}

//
// platforms

static uint32_t act_Plat_Bidir(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;

	if(sec->specialactive & ACT_FLOOR)
		return 0;

	gm = generic_floor(sec, value_mult < 0 ? DIR_DOWN : DIR_UP, SNDSEQ_PLAT, 0);
	gm->speed_now = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->delay = ln->arg2;
	gm->flags = MVF_BLOCK_GO_DN | MVF_BLOCK_GO_UP;

	if(value_mult < 0)
	{
		gm->bot_height = P_FindLowestFloorSurrounding(sec) + value_offs;
		gm->top_height = sec->floorheight;
		gm->speed_up = gm->speed_now;
	} else
	{
		if(value_mult)
		{
			find_nearest_floor(sec);
			gm->top_height = nearest_up;
		} else
			gm->top_height = P_FindHighestFloorSurrounding(sec);
		gm->bot_height = sec->floorheight;
		gm->speed_dn = gm->speed_now;
	}

	return 1;
}

//
// colors

static uint32_t act_SetColor(sector_t *sec, line_t *ln)
{
	sec->extra->color = (ln->arg1 >> 4) | (ln->arg2 & 0xF0) | ((uint16_t)(ln->arg3 & 0xF0) << 4) | ((uint16_t)(ln->arg4 & 0xF0) << 8);

	for(uint32_t i = 0; i < sector_light_count; i++)
	{
		sector_light_t *cl = sector_light + i;

		if(cl->color == sec->extra->color && cl->fade == sec->extra->fade)
		{
			sec->lightlevel = (sec->lightlevel & 0x1FF) | (i << 9);
			break;
		}
	}
}

static uint32_t act_SetFade(sector_t *sec, line_t *ln)
{
	sec->extra->fade = (ln->arg1 >> 4) | (ln->arg2 & 0xF0) | ((uint16_t)(ln->arg3 & 0xF0) << 4);

	for(uint32_t i = 0; i < sector_light_count; i++)
	{
		sector_light_t *cl = sector_light + i;

		if(cl->color == sec->extra->color && cl->fade == sec->extra->fade)
		{
			sec->lightlevel = (sec->lightlevel & 0x1FF) | (i << 9);
			break;
		}
	}

	return 1;
}

//
// thing stuff

static void act_Thing_Stop(mobj_t *th, line_t *ln)
{
	th->momx = 0;
	th->momy = 0;
	th->momz = 0;
}

static void act_ThrustThing(mobj_t *th, line_t *ln)
{
	angle_t angle = ln->arg0 << (24 - ANGLETOFINESHIFT);
	fixed_t force = (fixed_t)ln->arg1 * FRACUNIT;

	if(!ln->arg2 && force > 30 * FRACUNIT)
		force = 30 * FRACUNIT;

	th->momx += FixedMul(force, finecosine[angle]);
	th->momy += FixedMul(force, finesine[angle]);
}

static void act_ThrustThingZ(mobj_t *th, line_t *ln)
{
	fixed_t speed = (fixed_t)ln->arg1 * (FRACUNIT/4);

	if(ln->arg2)
		speed = -speed;

	if(ln->arg3)
		speed += th->momz;

	th->momz = speed;
}

static void act_Thing_SetSpecial(mobj_t *th, line_t *ln)
{
	th->special.special = ln->arg1;
	th->special.arg[0] = ln->arg2;
	th->special.arg[1] = ln->arg3;
	th->special.arg[2] = ln->arg4;
}

static void act_Thing_ChangeTID(mobj_t *th, line_t *ln)
{
	th->special.tid = ln->arg1;
}

static void act_Thing_Damage(mobj_t *th, line_t *ln)
{
	mobj_damage(th, NULL, NULL, ln->arg1, NULL);
}

static void act_Thing_Spawn(mobj_t *th, line_t *ln)
{
	mobj_t *mo;

	mo = P_SpawnMobj(th->x, th->y, th->z, spawn_type);

	if(value_mult > 1)
		mo->target = th;

	if(!P_CheckPosition(mo, mo->x, mo->y))
	{
		mobj_remove(mo);
		return;
	}

	if(mo->flags & MF_COUNTKILL)
		totalkills++;
	if(mo->flags & MF_COUNTITEM)
		totalitems++;

	if(value_mult)
	{
		mo->angle = ln->arg2 << 24;
		if(value_mult > 1)
		{
			fixed_t speed = (fixed_t)ln->arg3 * (FRACUNIT/8);
			angle_t angle = mo->angle >> ANGLETOFINESHIFT;
			mo->momx = FixedMul(speed, finecosine[angle]);
			mo->momy = FixedMul(speed, finesine[angle]);
			mo->momz = (fixed_t)ln->arg4 * (FRACUNIT/8);
			if(value_mult > 2)
			{
				mo->flags &= ~MF_NOGRAVITY;
				if(mo->flags & MF_MISSILE)
					mo->gravity = FRACUNIT / 8;
			} else
				mo->flags |= MF_NOGRAVITY;
			return;
		}
	} else
		mo->angle = th->angle;

	mo->special.tid = ln->arg3;

	if(!value_offs)
		return;

	P_SpawnMobj(th->x, th->y, th->z, 39); // MT_TFOG
}

static void act_Thing_Remove(mobj_t *th, line_t *ln)
{
	if(th->player)
		return;

	if(th->flags & MF_COUNTKILL)
		totalkills--;

	if(th->flags & MF_COUNTITEM)
		totalitems--;

	mobj_remove(th);
}

static void act_Thing_Destroy(mobj_t *th, line_t *ln)
{
	if(ln->arg2 && th->subsector->sector->tag != ln->arg2)
		return;

	if(!ln->arg0 && !(th->flags1 & MF1_ISMONSTER))
		return;

	mobj_damage(th, NULL, NULL, ln->arg1 ? 1000000 : 1000001, NULL);
}

//
// teleports

static uint32_t cb_teleport(mobj_t *mo)
{
	fixed_t x, y, z, f;
	angle_t angle;

	if(mo->special.tid != value_offs)
		return 0;

	if(mo->type != 41) // MT_TELEPORTMAN
		return 0;

	x = activator->x;
	y = activator->y;
	z = activator->z;
	f = activator->floorz;

	if(!P_TeleportMove(activator, mo->x, mo->y)) // TODO: handle Z
		return 0;

	if(mo->spawnpoint.type == 9044)
	{
		activator->z = mo->z;
		if(P_CheckPosition(activator, activator->x, activator->y))
		{
			activator->floorz = tmfloorz;
			activator->ceilingz = tmceilingz;
		}
	} else
	{
		activator->z = mo->floorz;
		if(activator->flags & MF_MISSILE)
		{
			activator->z += (z - f);
			if(P_CheckPosition(activator, activator->x, activator->y))
			{
				activator->floorz = tmfloorz;
				activator->ceilingz = tmceilingz;
			}
		}
	}

	if(value_mult < 0)
		// TODO: other angle modes
		activator->angle = mo->angle;

	angle = mo->angle >> ANGLETOFINESHIFT;

	if(value_mult < 0)
	{
		P_SpawnMobj(x, y, z, 39); // MT_TFOG
		P_SpawnMobj(activator->x + 20 * finecosine[angle], activator->y + 20 * finesine[angle], activator->z, 39); // MT_TFOG
	}

	if(activator->flags & MF_MISSILE)
	{
		fixed_t speed, pitch;

		if(activator->info->fast_speed && (fastparm || gameskill == sk_nightmare))
			speed = activator->info->fast_speed;
		else
			speed = activator->info->speed;

		if(activator->momz)
		{
			pitch = slope_to_angle(activator->momz);
			pitch >>= ANGLETOFINESHIFT;
			speed = FixedMul(speed, finecosine[pitch]);
		}

		activator->momx = FixedMul(speed, finecosine[angle]);
		activator->momy = FixedMul(speed, finesine[angle]);
	} else
	{
		if(value_mult < 0)
		{
			activator->momx = 0;
			activator->momy = 0;
			activator->momz = 0;
			if(activator->player)
				activator->reactiontime = 18;
		}
	}

	// HACK - move other sound slots
	activator->sound_body.x = activator->x;
	activator->sound_body.y = activator->y;
	activator->sound_weapon.x = activator->x;
	activator->sound_weapon.y = activator->y;

	return 1;
}

//
// tag handler

static uint32_t handle_tag(line_t *ln, uint32_t tag, uint32_t (*cb)(sector_t*,line_t*))
{
	uint32_t success = 0;

	if(!tag)
	{
		if(ln->backsector)
			return cb(ln->backsector, ln);
		return 0;
	}

	for(uint32_t i = 0; i < numsectors; i++)
	{
		sector_t *sec = sectors + i;
		if(sec->tag == tag)
			success |= cb(sec, ln);
	}

	return success;
}

//
// tid handler

static void handle_tid(line_t *ln, int32_t tid, void (*cb)(mobj_t*,line_t*))
{
	spec_success = 1;

	if(!tid)
	{
		if(activator)
			cb(activator, ln);
		return;
	}

	for(thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		uint32_t ret;
		mobj_t *mo;

		if(th->function != (void*)0x00031490 + doom_code_segment)
			continue;

		mo = (mobj_t*)th;

		if(tid > 0 && mo->special.tid != tid)
			continue;

		cb(mo, ln);
	}
}

//
// API

void spec_activate(line_t *ln, mobj_t *mo, uint32_t type)
{
	uint32_t back_side = type & SPEC_ACT_BACK_SIDE;

	spec_success = 0;
	type &= SPEC_ACT_TYPE_MASK;

	if(!ln)
	{
		ln = &spec_magic_line;
		if(ln->tag == 0)
			return;
	} else
	switch(ln->flags & ML_ACT_MASK)
	{
		case MLA_PLR_CROSS:
			if(type != SPEC_ACT_CROSS)
				return;
			if(mo->player)
				break;
			if(	mo->flags & MF_MISSILE &&
				!(mo->flags & MF1_NOTELEPORT) &&
				(
					ln->special == 70 ||	// Teleport
					ln->special == 71	// Teleport_NoFog
				)
			)
				break;
			if(	!(ln->flags & ML_MONSTER_ACT) &&
				(	map_level_info->flags & MAP_FLAG_NO_MONSTER_ACTIVATION ||
					(
						ln->special != 206 &&	// Plat_DownWaitUpStayLip
						ln->special != 70 &&	// Teleport
						ln->special != 71	// Teleport_NoFog
					)
				)
			)
				return;
			if(mo->flags & MF_MISSILE)
				return;
		break;
		case MLA_PLR_USE:
			if(type != SPEC_ACT_USE)
				return;
			if(mo->player)
				break;
			if(	!(ln->flags & ML_MONSTER_ACT) &&
				(	map_level_info->flags & MAP_FLAG_NO_MONSTER_ACTIVATION ||
					(
						ln->special != 12 &&	// Door_Raise
						ln->special != 70	// Teleport
					)
				)
			)
				return;
		break;
		case MLA_MON_CROSS:
			if(!(mo->flags1 & MF1_ISMONSTER))
				return;
			if(type != SPEC_ACT_CROSS)
				return;
		break;
		case MLA_ATK_HIT:
			if(	type == SPEC_ACT_CROSS ||
				type == SPEC_ACT_BUMP
			)
			{
				if(!(mo->flags & MF_MISSILE))
					return;
				if(type == SPEC_ACT_BUMP)
					mo = mo->target;
				break;
			}
			if(type != SPEC_ACT_SHOOT)
				return;
		break;
		case MLA_PLR_BUMP:
			if(type != SPEC_ACT_BUMP)
				return;
			if(mo->flags & MF_MISSILE)
				return;
			if(mo->player)
				break;
			if(!(ln->flags & ML_MONSTER_ACT))
				return;
			if(!(mo->flags1 & MF1_ISMONSTER))
				return;
		break;
		case MLA_PROJ_CROSS:
			if(!(mo->flags & MF_MISSILE))
				return;
			if(type != SPEC_ACT_CROSS)
				return;
		break;
		default:
			// unsupported
			ln->special = 0;
			return;
	}

	activator = mo;

	switch(ln->special)
	{
		case 10: // Door_Close
			spec_success = handle_tag(ln, ln->arg0, act_Door_Close);
		break;
		case 12: // Door_Raise
			door_monster_hack = !mo->player;
			value_mult = 0;
			spec_success = handle_tag(ln, ln->arg0, act_Door_Raise);
		break;
		case 13: // Door_LockedRaise
			door_monster_hack = 0;
			value_mult = 1;
			if(ln->arg3)
			{
				uint8_t *message;
				message = mobj_check_keylock(activator, ln->arg3, ln->arg0);
				if(message)
				{
					if(activator->player && message[0])
						activator->player->message = message;
					value_mult = 0;
				}
			}
			if(value_mult)
				spec_success = handle_tag(ln, ln->arg0, act_Door_Raise);
		break;
		case 19: // Thing_Stop
			handle_tid(ln, ln->arg0, act_Thing_Stop);
		break;
		case 20: // Floor_LowerByValue
			value_mult = -1;
			spec_success = handle_tag(ln, ln->arg0, act_Floor_ByValue);
		break;
		case 23: // Floor_RaiseByValue
			value_mult = 1;
			spec_success = handle_tag(ln, ln->arg0, act_Floor_ByValue);
		break;
		case 28: // Floor_RaiseAndCrush
			value_mult = 0;
			spec_success = handle_tag(ln, ln->arg0, act_Floor_Crush);
		break;
		case 35: // Floor_RaiseByValueTimes8
			value_mult = 8;
			spec_success = handle_tag(ln, ln->arg0, act_Floor_ByValue);
		break;
		case 36: // Floor_LowerByValueTimes8
			value_mult = -8;
			spec_success = handle_tag(ln, ln->arg0, act_Floor_ByValue);
		break;
		case 40: // Ceiling_LowerByValue
			value_mult = -1;
			spec_success = handle_tag(ln, ln->arg0, act_Ceiling_ByValue);
		break;
		case 41: // Ceiling_RaiseByValue
			value_mult = 1;
			spec_success = handle_tag(ln, ln->arg0, act_Ceiling_ByValue);
		break;
		case 44: // Ceiling_CrushStop
			spec_success = handle_tag(ln, ln->arg0, act_Ceiling_CrushStop);
		break;
		case 62: // Plat_DownWaitUpStay
			value_mult = -1;
			value_offs = 8 * FRACUNIT;
			spec_success = handle_tag(ln, ln->arg0, act_Plat_Bidir);
		break;
		case 64: // Plat_UpWaitDownStay
			value_mult = 0;
			spec_success = handle_tag(ln, ln->arg0, act_Plat_Bidir);
		break;
		case 70: // Teleport
			if(!back_side)
			{
				value_mult = -1;
				value_offs = ln->arg0;
				spec_success = mobj_for_each(cb_teleport);
			}
		break;
		case 71: // Teleport_NoFog
			if(!back_side)
			{
				value_mult = 0;
				value_offs = ln->arg0;
				spec_success = mobj_for_each(cb_teleport);
			}
		break;
		case 72: // ThrustThing
			handle_tid(ln, ln->arg3, act_ThrustThing);
		break;
		case 73: // DamageThing
			if(activator)
			{
				uint32_t damage = ln->arg0;
				if(!damage)
					damage = 1000000;
				mobj_damage(activator, NULL, NULL, damage, NULL);
				spec_success = 1;
			}
		break;
		case 74: // Teleport_NewMap
			secretexit = 0;
			gameaction = ga_completed;
			map_next_levelnum = ln->arg0;
			map_start_id = ln->arg1;
			map_start_facing = !!ln->arg2;
			spec_success = 1;
		break;
		case 97: // Ceiling_LowerAndCrushDist
			spec_success = handle_tag(ln, ln->arg0, act_Ceiling_LowerAndCrushDist);
		break;
		case 99: // Floor_RaiseAndCrushDoom
			value_mult = 1;
			spec_success = handle_tag(ln, ln->arg0, act_Floor_Crush);
		break;
		case 119: // Thing_Damage
			handle_tid(ln, ln->arg0, act_Thing_Damage);
		break;
		case 127: // Thing_SetSpecial
			handle_tid(ln, ln->arg0, act_Thing_SetSpecial);
		break;
		case 128: // ThrustThingZ
			handle_tid(ln, ln->arg0, act_ThrustThingZ);
		break;
		case 132: // Thing_Remove
			handle_tid(ln, ln->arg0, act_Thing_Remove);
		break;
		case 133: // Thing_Destroy
			handle_tid(ln, ln->arg0 > 0 ? ln->arg0 : -1, act_Thing_Destroy);
		break;
		case 134: // Thing_Projectile
			value_mult = 2;
			value_offs = 1;
			goto thing_spawn;
		break;
		case 135: // Thing_Spawn
			value_mult = 1;
			value_offs = 1;
			goto thing_spawn;
		break;
		case 136: // Thing_ProjectileGravity
			value_mult = 3;
			value_offs = 1;
			goto thing_spawn;
		break;
		case 137: // Thing_SpawnNoFog
			value_mult = 1;
			value_offs = 0;
			goto thing_spawn;
		break;
		case 139: // Thing_SpawnFacing
			value_mult = 0;
			value_offs = !ln->arg2;
thing_spawn:
			spawn_type = mobj_by_spawnid(ln->arg1);
			if(spawn_type >= 0 && ln->arg0 != ln->arg3)
				handle_tid(ln, ln->arg0, act_Thing_Spawn);
		break;
		case 172: // Plat_UpNearestWaitDownStay
			value_mult = 1;
			spec_success = handle_tag(ln, ln->arg0, act_Plat_Bidir);
		break;
		case 176: // Thing_ChangeTID
			if(!ln->arg1 || ln->arg0 != ln->arg1)
				handle_tid(ln, ln->arg0, act_Thing_ChangeTID);
		break;
		case 195: // Ceiling_CrushRaiseAndStayA
			value_mult = 0;
			spec_success = handle_tag(ln, ln->arg0, act_Ceiling_Crush);
		break;
		case 196: // Ceiling_CrushAndRaiseA
			value_mult = 1;
			spec_success = handle_tag(ln, ln->arg0, act_Ceiling_Crush);
		break;
		case 198: // Ceiling_RaiseByValueTimes8
			value_mult = 8;
			spec_success = handle_tag(ln, ln->arg0, act_Ceiling_ByValue);
		break;
		case 199: // Ceiling_LowerByValueTimes8
			value_mult = -8;
			spec_success = handle_tag(ln, ln->arg0, act_Ceiling_ByValue);
		break;
		case 200: // Generic_Floor
			spec_success = handle_tag(ln, ln->arg0, act_Generic_Floor);
		break;
		case 201: // Generic_Ceiling
			spec_success = handle_tag(ln, ln->arg0, act_Generic_Ceiling);
		break;
		case 206: // Plat_DownWaitUpStayLip
			value_mult = -1;
			value_offs = (fixed_t)ln->arg3 * FRACUNIT;
			spec_success = handle_tag(ln, ln->arg0, act_Plat_Bidir);
		break;
		case 212: // Sector_SetColor
			spec_success = handle_tag(ln, ln->arg0, act_SetColor);
		break;
		case 213: // Sector_SetFade
			spec_success = handle_tag(ln, ln->arg0, act_SetFade);
		break;
		case 239: // Floor_RaiseByValueTxTy
			value_mult = 1;
			spec_success = handle_tag(ln, ln->arg0, act_Floor_ByValue);
		break;
		case 243: // Exit_Normal
			secretexit = 0;
			gameaction = ga_completed;
			map_start_id = ln->arg0;
			spec_success = 1;
		break;
		case 244: // Exit_Secret
			secretexit = 1;
			gameaction = ga_completed;
			map_start_id = ln->arg0;
			spec_success = 1;
		break;
		default:
			doom_printf("special %u; side %u; mo 0x%08X; pl 0x%08X\n", ln->special, !!back_side, mo, mo->player);
		break;
	}

	if(spec_success && ln != &spec_magic_line)
		do_line_switch(ln, ln->flags & ML_REPEATABLE);
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
void spec_line_cross(uint32_t lidx, uint32_t side)
{
	register mobj_t *mo asm("ebx"); // hack to extract 3rd argument
	spec_activate(lines + lidx, mo, (side ? SPEC_ACT_BACK_SIDE : 0) | SPEC_ACT_CROSS);
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t spec_line_use(mobj_t *mo, line_t *ln)
{
	register uint32_t side asm("ebx"); // hack to extract 3rd argument

	if(side)
		return 0;

	spec_activate(ln, mo, SPEC_ACT_USE);

	return spec_success;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// 'PTR_UseTraverse' checks for ln->special (8bit)
	{0x0002BCA9, CODE_HOOK | HOOK_UINT32, 0x00127B80},
	{0x0002BCAD, CODE_HOOK | HOOK_UINT8, 0x90},
};

