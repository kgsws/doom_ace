// kgsws' ACE Engine
////
// ZDoom line specials.
// ZDoom sector specials.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "player.h"
#include "draw.h"
#include "mobj.h"
#include "map.h"
#include "render.h"
#include "action.h"
#include "sound.h"
#include "decorate.h"
#include "animate.h"
#include "generic.h"
#include "polyobj.h"
#include "saveload.h"
#include "special.h"

enum
{
	MVT_GENERIC, // must be zero
	MVT_DOOR,
	MVT_CRUSH_CEILING,
};

//

uint32_t spec_special;
int32_t spec_arg[5];
uint32_t spec_success;

static line_t *spec_line;
static mobj_t *activator;

static uint_fast8_t door_monster_hack;
static fixed_t value_mult;
static fixed_t value_offs;
static int32_t spawn_type;

static fixed_t ts_offs_x;
static fixed_t ts_offs_y;
static fixed_t ts_offs_z;

static sector_extra_t *sec_extra;

fixed_t find_up;
fixed_t find_dn;


//
// missing or fixed height search

void find_nearest_floor(sector_t *sec)
{
	find_up = ONCEILINGZ;
	find_dn = ONFLOORZ;

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

		if(bs->floorheight < sec->floorheight && bs->floorheight > find_dn)
			find_dn = bs->floorheight;

		if(bs->floorheight > sec->floorheight && bs->floorheight < find_up)
			find_up = bs->floorheight;
	}

	if(find_up == ONCEILINGZ)
		find_up = sec->floorheight;
	if(find_dn == ONFLOORZ)
		find_dn = sec->floorheight;
}

void find_nearest_ceiling(sector_t *sec)
{
	find_up = ONCEILINGZ;
	find_dn = ONFLOORZ;

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

		if(bs->ceilingheight < sec->ceilingheight && bs->ceilingheight > find_dn)
			find_dn = bs->ceilingheight;

		if(bs->ceilingheight > sec->ceilingheight && bs->ceilingheight < find_up)
			find_up = bs->ceilingheight;
	}

	if(find_up == ONCEILINGZ)
		find_up = sec->ceilingheight;
	if(find_dn == ONFLOORZ)
		find_dn = sec->ceilingheight;
}

void find_highest(sector_t *sec)
{
	find_up = ONFLOORZ;
	find_dn = ONFLOORZ;

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

		if(bs->ceilingheight > find_up)
			find_up = bs->ceilingheight;

		if(bs->floorheight > find_dn)
			find_dn = bs->floorheight;
	}

	if(find_up == ONFLOORZ)
		find_up = sec->ceilingheight;
	if(find_dn == ONFLOORZ)
		find_dn = sec->floorheight;
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
		mobj_change_sector(sec, 0);
		return 1;
	}

	gm = generic_ceiling(sec, DIR_DOWN, SNDSEQ_DOOR, spec_arg[1] >= 64);
	if(!gm)
		return 0;

	gm->top_height = sec->ceilingheight;
	gm->bot_height = sec->floorheight;
	gm->speed_now = spec_arg[1] * (FRACUNIT/8);
	gm->flags = MVF_BLOCK_STAY;
	gm->lighttag = spec_arg[2];

	return 1;
}

static uint32_t act_Door_Raise(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;
	fixed_t height;

	if(sec->specialactive & ACT_CEILING)
	{
		if(spec_arg[0])
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

	gm = generic_ceiling(sec, DIR_UP, SNDSEQ_DOOR, spec_arg[1] >= 64);
	gm->top_height = height;
	gm->bot_height = sec->floorheight;
	gm->speed_now = spec_arg[1] * (FRACUNIT/8);
	gm->speed_dn = gm->speed_now;
	gm->type = MVT_DOOR;
	gm->flags = MVF_BLOCK_GO_UP;

	if(value_mult)
	{
		gm->lighttag = spec_arg[4];
		gm->delay = spec_arg[2];
		if(!spec_arg[2])
		{
			gm->speed_dn = 0;
			gm->flags = 0;
			gm->type = MVT_GENERIC;
		}
	} else
	{
		gm->lighttag = spec_arg[3];
		if(!spec_arg[2])
		{
			gm->delay = 1;
			gm->flags |= MVF_WAIT_STOP;
		} else
			gm->delay = spec_arg[2];
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

	if(!value_mult)
	{
		dest = (fixed_t)spec_arg[2] * FRACUNIT;
		if(spec_arg[3])
			dest = -dest;
		value_mult = dest < sec->floorheight ? -1 : 1;
	} else
		dest = sec->floorheight + spec_arg[2] * value_mult * FRACUNIT;

	if(dest > sec->ceilingheight)
		dest = sec->ceilingheight;

	if(dest == sec->floorheight)
		return 1;

	gm = generic_floor(sec, value_mult < 0 ? DIR_DOWN : DIR_UP, SNDSEQ_FLOOR, 0);
	gm->top_height = dest;
	gm->bot_height = dest;
	gm->speed_now = spec_arg[1] * (FRACUNIT/8);
	gm->flags = MVF_BLOCK_STAY;

	if(ln && ln->special == 239)
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

	switch(spec_arg[3])
	{
		case 0:
			dest = sec->floorheight;
			if(spec_arg[4] & 8)
				dest += spec_arg[2] * FRACUNIT;
			else
				dest -= spec_arg[2] * FRACUNIT;
		break;
		case 1:
			find_highest(sec);
			dest = find_dn;
		break;
		case 2:
			dest = P_FindLowestFloorSurrounding(sec);
		break;
		case 3:
			find_nearest_floor(sec);
			if(spec_arg[4] & 8)
				dest = find_up;
			else
				dest = find_dn;
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

	if(spec_arg[4] & 3)
	{
		if(spec_arg[4] & 4)
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
		if(ln)
		{
			texture = ln->frontsector->floorpic;
			special = ln->frontsector->special;
		} else
		{
			texture = sec->floorpic;
			special = sec->special;
		}
	}

	switch(spec_arg[4] & 3)
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

	gm = generic_floor(sec, spec_arg[4] & 8 ? DIR_UP : DIR_DOWN, SNDSEQ_FLOOR, 0);
	gm->top_height = dest;
	gm->bot_height = dest;
	gm->speed_now = spec_arg[1] * (FRACUNIT/8);
	gm->flags = flags | MVF_BLOCK_STAY;
	gm->crush = spec_arg[4] & 16 ? 0x8014 : 0;
	gm->texture = texture;
	gm->special = special;

	// apply instant movement
	if(spec_arg[4] & 8)
	{
		// UP
		if(	dest <= sec->floorheight ||
			sec->floorheight + gm->speed_now >= dest
		){
			gm->sndwait = 2;
			think_floor(gm);
		}
	} else
	{
		// DOWN
		if(	dest >= sec->floorheight ||
			sec->floorheight - gm->speed_now <= dest
		){
			gm->sndwait = 2;
			think_floor(gm);
		}
	}

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
	gm->speed_now = spec_arg[1] * (FRACUNIT/8);
	gm->crush = 0x8000 | spec_arg[2];

	switch(spec_arg[3])
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

	if(!value_mult)
	{
		dest = (fixed_t)spec_arg[2] * FRACUNIT;
		if(spec_arg[3])
			dest = -dest;
		value_mult = dest < sec->ceilingheight ? -1 : 1;
	} else
		dest = sec->ceilingheight + spec_arg[2] * value_mult * FRACUNIT;

	if(dest < sec->floorheight)
		dest = sec->floorheight;

	if(dest == sec->ceilingheight)
		return 1;

	gm = generic_ceiling(sec, value_mult < 0 ? DIR_DOWN : DIR_UP, SNDSEQ_CEILING, 0);
	gm->top_height = dest;
	gm->bot_height = dest;
	gm->speed_now = spec_arg[1] * (FRACUNIT/8);
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

	switch(spec_arg[3])
	{
		case 0:
			dest = sec->ceilingheight;
			if(spec_arg[4] & 8)
				dest += spec_arg[2] * FRACUNIT;
			else
				dest -= spec_arg[2] * FRACUNIT;
		break;
		case 1:
			find_highest(sec);
			dest = find_up;
		break;
		case 2:
			dest = P_FindLowestCeilingSurrounding(sec);
		break;
		case 3:
			find_nearest_ceiling(sec);
			if(spec_arg[4] & 8)
				dest = find_up;
			else
				dest = find_dn;
		break;
		case 4:
			find_highest(sec);
			dest = find_dn;
		break;
		case 5:
			dest = sec->floorheight;
		break;
		default:
			return 0;
	}

	if(dest < sec->floorheight)
		dest = sec->floorheight;

	if(spec_arg[4] & 3)
	{
		if(spec_arg[4] & 4)
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
		if(ln)
		{
			texture = ln->frontsector->ceilingpic;
			special = ln->frontsector->special;
		} else
		{
			texture = sec->ceilingpic;
			special = sec->special;
		}
	}

	switch(spec_arg[4] & 3)
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

	gm = generic_ceiling(sec, spec_arg[4] & 8 ? DIR_UP : DIR_DOWN, SNDSEQ_CEILING, 0);
	gm->top_height = dest;
	gm->bot_height = dest;
	gm->speed_now = spec_arg[1] * (FRACUNIT/8);
	gm->flags = spec_arg[4] & 16 ? 0 : MVF_BLOCK_STAY;
	gm->crush = spec_arg[4] & 16 ? 0x8014 : 0;
	gm->flags |= flags;
	gm->texture = texture;
	gm->special = special;

	// apply instant movement
	if(spec_arg[4] & 8)
	{
		// UP
		if(	dest <= sec->ceilingheight ||
			sec->ceilingheight + gm->speed_now >= dest
		){
			gm->sndwait = 2;
			think_ceiling(gm);
		}
	} else
	{
		// DOWN
		if(	dest >= sec->ceilingheight ||
			sec->ceilingheight - gm->speed_now <= dest
		){
			gm->sndwait = 2;
			think_ceiling(gm);
		}
	}

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
	gm->speed_up = spec_arg[2] * (FRACUNIT/8);
	gm->speed_now = spec_arg[1] * (FRACUNIT/8);
	gm->type = MVT_CRUSH_CEILING;
	gm->crush = 0x8000 | spec_arg[3];
	if(value_mult)
		gm->speed_dn = gm->speed_now;

	switch(spec_arg[4])
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
	gm->bot_height = sec->floorheight + spec_arg[3] * FRACUNIT;
	gm->top_height = gm->bot_height;
	gm->speed_now = spec_arg[1] * (FRACUNIT/8);
	gm->type = MVT_CRUSH_CEILING;
	gm->crush = 0x8000 | spec_arg[2];

	switch(spec_arg[4])
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
	gm->speed_now = spec_arg[1] * (FRACUNIT/8);
	gm->delay = spec_arg[2];
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
			gm->top_height = find_up;
		} else
		{
			find_highest(sec);
			gm->top_height = find_dn;
		}
		gm->bot_height = sec->floorheight;
		gm->speed_dn = gm->speed_now;
	}

	return 1;
}

//
// stairs

static uint32_t act_Stairs_Build(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;
	uint32_t success = 0;
	uint32_t secspec = sec->special;
	fixed_t speed = spec_arg[1] * (FRACUNIT/8);
	fixed_t height = sec->floorheight;

	if(secspec != 26 && secspec != 27)
		secspec = 0;

	while(1)
	{
		if(sec->specialactive & ACT_FLOOR)
			break;

		gm = generic_floor(sec, value_mult < 0 ? DIR_DOWN : DIR_UP, SNDSEQ_FLOOR, 0);
		gm->flags = MVF_BLOCK_STAY;
		gm->speed_now = speed;

		if(value_mult < 0)
		{
			height -= (fixed_t)spec_arg[2] * FRACUNIT;
			gm->bot_height = height;
		} else
		{
			height += (fixed_t)spec_arg[2] * FRACUNIT;
			gm->top_height = height;
		}

		success = 1;

		if(!secspec)
			break;
		secspec ^= 1;

		if(value_offs)
			speed += spec_arg[1] * (FRACUNIT/8);

		for(uint32_t i = 0; i < sec->linecount; i++)
		{
			line_t *li = sec->lines[i];
			sector_t *sss;

			if(li->frontsector == sec)
				sss = li->backsector;
			else
				sss = li->frontsector;

			if(!sss)
				continue;

			if(sss->specialactive & ACT_FLOOR)
				continue;

			if(sss->special == secspec)
			{
				sec = sss;
				break;
			}
		}
	}

	return success;
}

//
// colors

static uint32_t act_SetColor(sector_t *sec, line_t *ln)
{
	sec->extra->color = (spec_arg[1] >> 4) | (spec_arg[2] & 0xF0) | ((uint16_t)(spec_arg[3] & 0xF0) << 4) | ((uint16_t)(spec_arg[4] & 0xF0) << 8);

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
	sec->extra->fade = (spec_arg[1] >> 4) | (spec_arg[2] & 0xF0) | ((uint16_t)(spec_arg[3] & 0xF0) << 4);

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
// lights

static uint32_t act_Light_ByValue(sector_t *sec, line_t *ln)
{
	int32_t light = sec->lightlevel & 0x1FF;

	light += value_offs;
	if(light >= 255)
		light = 255;
	if(light < 0)
		light = 0;

	sec->lightlevel &= 0xFE00;
	sec->lightlevel |= light;

	return 1;
}

static uint32_t arg_Light_ChangeToValue(sector_t *sec, line_t *ln)
{
	int32_t light = spec_arg[1];

	if(light >= 255)
		light = 255;
	if(light < 0)
		light = 0;

	sec->lightlevel &= 0xFE00;
	sec->lightlevel |= light;

	return 1;
}

static uint32_t arg_Light_Fade(sector_t *sec, line_t *ln)
{
	generic_light_t *gl;
	fixed_t now = (sec->lightlevel & 0x1FF) << FRACBITS;
	fixed_t set = (fixed_t)spec_arg[1] << FRACBITS;

	if(sec->specialactive & ACT_LIGHT)
	{
		gl = generic_light_by_sector(sec);
		if(gl && gl->flags & LIF_IS_FADE)
		{
			if(now == set || !spec_arg[2])
			{
				sec->lightlevel &= 0xFE00;
				sec->lightlevel |= set >> FRACBITS;
				gl->thinker.function = (void*)-1;
				sec->specialactive &= ~ACT_LIGHT;
				return 1;
			} else
				goto do_fade;
		}
		return 0;
	}

	if(now == set)
		return 1;

	if(!spec_arg[2])
	{
		arg_Light_ChangeToValue(sec, ln);
		return 1;
	}

	gl = generic_light(sec);
	if(!gl)
		return 0;

	gl->flags = LIF_IS_FADE;

do_fade:
	if(set > now)
	{
		gl->direction = DIR_UP;
		gl->top = set;
		gl->speed = (set - now) / spec_arg[2];
	} else
	{
		gl->direction = DIR_DOWN;
		gl->bot = set;
		gl->speed = (now - set) / spec_arg[2];
	}

	return 1;
}

static uint32_t arg_Light_Glow(sector_t *sec, line_t *ln)
{
	generic_light_t *gl;
	fixed_t now = (sec->lightlevel & 0x1FF) << FRACBITS;
	fixed_t top = (fixed_t)spec_arg[1] << FRACBITS;
	fixed_t bot = (fixed_t)spec_arg[2] << FRACBITS;

	if(sec->specialactive & ACT_LIGHT)
		return 0;

	if(!spec_arg[3])
		return 0;

	gl = generic_light(sec);
	if(!gl)
		return 0;

	gl->top = top;
	gl->bot = bot;
	gl->speed = (top - bot) / spec_arg[3];
	gl->flags = LIF_TOP_REVERSE | LIF_BOT_REVERSE;

	if(top > now)
		gl->direction = DIR_UP;
	else
		gl->direction = DIR_DOWN;

	return 1;
}

static uint32_t arg_Light_Strobe(sector_t *sec, line_t *ln)
{
	generic_light_t *gl;
	fixed_t now = (sec->lightlevel & 0x1FF) << FRACBITS;
	fixed_t top = (fixed_t)spec_arg[1] << FRACBITS;
	fixed_t bot = (fixed_t)spec_arg[2] << FRACBITS;

	if(sec->specialactive & ACT_LIGHT)
		return 0;

	gl = generic_light(sec);
	if(!gl)
		return 0;

	gl->delay_top = spec_arg[3];
	gl->delay_bot = spec_arg[4];
	gl->top = top;
	gl->bot = bot;
	gl->speed = top - bot;
	gl->flags = LIF_TOP_REVERSE | LIF_BOT_REVERSE;

	if(bot < now)
		gl->direction = DIR_DOWN;
	else
		gl->direction = DIR_UP;

	return 1;
}

static uint32_t arg_Light_Stop(sector_t *sec, line_t *ln)
{
	generic_light_t *gl;

	if(!(sec->specialactive & ACT_LIGHT))
		return 0;

	gl = generic_light_by_sector(sec);
	if(!gl)
		return 0;

	gl->thinker.function = (void*)-1;
	sec->specialactive &= ~ACT_LIGHT;

	return 1;
}

//
// thing stuff

static void act_Thing_Stop(mobj_t *th)
{
	th->momx = 0;
	th->momy = 0;
	th->momz = 0;
}

static void act_ThrustThing(mobj_t *th)
{
	angle_t angle = spec_arg[0] << (24 - ANGLETOFINESHIFT);
	fixed_t force = spec_arg[1] * FRACUNIT;

	if(!spec_arg[2] && force > 30 * FRACUNIT)
		force = 30 * FRACUNIT;

	th->momx += FixedMul(force, finecosine[angle]);
	th->momy += FixedMul(force, finesine[angle]);
}

static void act_ThrustThingZ(mobj_t *th)
{
	fixed_t speed = spec_arg[1] * (FRACUNIT/4);

	if(spec_arg[2])
		speed = -speed;

	if(spec_arg[3])
		speed += th->momz;

	th->momz = speed;
}

static void act_Thing_Activate(mobj_t *th)
{
	if(	th->spawnpoint.type == 9071 ||	// path follower
		th->spawnpoint.type == 9072 ||	// moving camera
		th->spawnpoint.type == 9074	// actor mover
	){
		th->iflags |= MFI_FOLLOW_PATH | MFI_FOLLOW_INIT;
		return;
	}

	if(th->info->extra_type == ETYPE_SWITCHABLE)
	{
		th->flags1 &= ~MF1_DORMANT;
		mobj_set_animation(th, ANIM_S_ACTIVE);
		return;
	}

	if(th->player)
		return;

	if(!(th->flags1 & MF1_ISMONSTER))
		return;

	if(!(th->flags1 & MF1_DORMANT))
		return;

	th->flags1 &= ~MF1_DORMANT;
	th->tics = th->state->tics;
}

static void act_Thing_Deactivate(mobj_t *th)
{
	if(	th->spawnpoint.type == 9071 ||	// path follower
		th->spawnpoint.type == 9072 ||	// moving camera
		th->spawnpoint.type == 9074	// actor mover
	){
		th->iflags &= ~(MFI_FOLLOW_PATH | MFI_FOLLOW_INIT);
		return;
	}

	if(th->info->extra_type == ETYPE_SWITCHABLE)
	{
		th->flags1 |= MF1_DORMANT;
		mobj_set_animation(th, ANIM_S_INACTIVE);
		return;
	}

	if(th->player)
		return;

	if(th->health <= 0)
		return;

	if(!(th->flags1 & MF1_ISMONSTER))
		return;

	if(th->flags1 & MF1_DORMANT)
		return;

	th->flags1 |= MF1_DORMANT;
	th->tics = -1;
}

static void act_Thing_SetSpecial(mobj_t *th)
{
	th->special.special = spec_arg[1];
	th->special.arg[0] = spec_arg[2];
	th->special.arg[1] = spec_arg[3];
	th->special.arg[2] = spec_arg[4];
}

static void act_Thing_ChangeTID(mobj_t *th)
{
	th->special.tid = spec_arg[1];
}

static void act_Thing_Damage(mobj_t *th)
{
	if(!spec_arg[1])
		return;

	if(spec_arg[1] > 0)
		mobj_damage(th, NULL, activator, spec_arg[1], NULL);
	else
		mobj_give_health(activator, -spec_arg[1], 0);
}

static void act_Thing_Spawn(mobj_t *th)
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

	if(value_mult)
	{
		mo->angle = spec_arg[2] << 24;
		if(value_mult > 1)
		{
			fixed_t speed = spec_arg[3] * (FRACUNIT/8);
			angle_t angle = mo->angle >> ANGLETOFINESHIFT;
			mo->momx = FixedMul(speed, finecosine[angle]);
			mo->momy = FixedMul(speed, finesine[angle]);
			mo->momz = spec_arg[4] * (FRACUNIT/8);
			if(value_mult > 2)
			{
				mo->flags &= ~MF_NOGRAVITY;
				mo->gravity = FRACUNIT / 8;
			} else
				mo->flags |= MF_NOGRAVITY;
			return;
		}
	} else
		mo->angle = th->angle;

	mo->special.tid = spec_arg[3];

	if(!value_offs)
		return;

	P_SpawnMobj(th->x, th->y, th->z, 39); // MT_TFOG
}

static void act_Thing_Remove(mobj_t *th)
{
	if(th->player)
		return;
	mobj_remove(th);
}

static void act_Thing_Destroy(mobj_t *th)
{
	if(spec_arg[2] && th->subsector->sector->tag != spec_arg[2])
		return;

	if(!spec_arg[0] && !(th->flags1 & MF1_ISMONSTER))
		return;

	mobj_damage(th, NULL, NULL, spec_arg[1] ? 1000000 : 1000001, NULL);
}

//
// teleports

static uint32_t do_Teleport()
{
	// TODO: other angle modes
	mobj_t *mo;
	uint32_t flags = TELEF_USE_ANGLE;

	if(activator->flags1 & MF1_NOTELEPORT)
		return 0;

	mo = mobj_by_tid_first(spec_arg[0]);
	if(!mo)
		return 0;

	if(mo->spawnpoint.type == 9044)
		flags |= TELEF_USE_Z;

	if(value_mult < 0)
		flags |= TELEF_FOG;

	return mobj_teleport(activator, mo->x, mo->y, mo->z, mo->angle, flags);
}

static uint32_t do_TeleportOther()
{
	mobj_t *mo, *target;
	fixed_t x, y, z;
	uint32_t flags = TELEF_NO_KILL | TELEF_USE_Z;

	mo = mobj_by_tid_first(spec_arg[0]);
	if(!mo)
		return 0;

	target = mobj_by_tid_first(spec_arg[1]);
	if(!target)
		return 0;

	if(spec_arg[2])
		flags |= TELEF_USE_ANGLE | TELEF_FOG;
	else
		flags |= TELEF_NOSTOP;

	return mobj_teleport(mo, target->x, target->y, target->z, target->angle, flags);
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t PIT_TeleportInSector(mobj_t *mo)
{
	fixed_t r;

	if(mo->validcount == validcount)
		return 1;

	mo->validcount = validcount;
	r = mo->radius;

	if(mo->x < sec_extra->bbox[BOXLEFT] - r)
		return 1;

	if(mo->x > sec_extra->bbox[BOXRIGHT] + r)
		return 1;

	if(mo->y < sec_extra->bbox[BOXBOTTOM] - r)
		return 1;

	if(mo->y > sec_extra->bbox[BOXTOP] + r)
		return 1;

	// don't attempt to teleport anything
	// as this can't be done in block thing iterator
	mo->iflags |= MFI_MARKED;

	return 1;
}

static uint32_t do_TeleportInSector()
{
	mobj_t *anchor, *target;

	if(!spec_arg[0])
		return 0;

	anchor = mobj_by_tid_first(spec_arg[1]);
	if(!anchor)
		return 0;

	target = mobj_by_tid_first(spec_arg[2]);
	if(!target)
		return 0;
	if(target->spawnpoint.type != 9044 && target->spawnpoint.type != 14)
		return 0;

	if(anchor->angle != target->angle)
		// sorry, rotation is not supported
		return 0;

	ts_offs_x = target->x - anchor->x;
	ts_offs_y = target->y - anchor->y;

	if(target->spawnpoint.type == 9044)
		ts_offs_z = target->z - anchor->z;
	else
		ts_offs_z = ONFLOORZ;

	validcount++;
	value_offs = 0;

	for(uint32_t i = 0; i < numsectors; i++)
	{
		sector_t *sec = sectors + i;

		if(sec->tag != spec_arg[0])
			continue;

		sec_extra = sec->extra;

		for(mobj_t *mo = sec->thinglist; mo; mo = mo->snext)
			PIT_TeleportInSector(mo);

		for(int32_t x = sec->blockbox[BOXLEFT]; x <= sec->blockbox[BOXRIGHT]; x++)
			for(int32_t y = sec->blockbox[BOXBOTTOM]; y <= sec->blockbox[BOXTOP]; y++)
				P_BlockThingsIterator(x, y, PIT_TeleportInSector);
	}

	for(thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		mobj_t *mo;
		fixed_t x, y, z;
		fixed_t r;
		uint32_t flags;

		if(th->function != (void*)P_MobjThinker)
			continue;

		mo = (mobj_t*)th;

		if(!(mo->iflags & MFI_MARKED))
			continue;

		mo->iflags &= ~MFI_MARKED;

		if(spec_arg[4] && mo->special.tid != spec_arg[4])
			continue;

		flags = 0;
		x = mo->x + ts_offs_x;
		y = mo->y + ts_offs_y;
		z = mo->z + ts_offs_z;

		if(ts_offs_z != ONFLOORZ)
			flags |= TELEF_USE_Z;

		if(spec_arg[3])
			flags |= TELEF_USE_ANGLE | TELEF_FOG;
		else
			flags |= TELEF_NOSTOP;

		value_offs |= mobj_teleport(mo, x, y, z, 0, flags);
	}

	return value_offs;
}

//
// misc

static inline uint32_t do_NoiseAlert()
{
	mobj_t *emitter, *target;

	if(spec_arg[0])
		target = mobj_by_tid_first(spec_arg[0]);
	else
		target = activator;

	if(!target)
		return 0;

	if(spec_arg[1] == spec_arg[0])
		emitter = target;
	else
	if(spec_arg[1])
		emitter = mobj_by_tid_first(spec_arg[1]);
	else
		emitter = activator;

	if(!emitter)
		return 0;

	P_NoiseAlert(target, emitter);

	return 1;
}

static inline uint32_t do_SetPlayerProperty()
{
	uint32_t idx, top;

	if(!spec_arg[0])
	{
		if(!activator)
			return 0;

		if(!activator->player)
			return 0;

		idx = activator->player - players;
		top = idx + 1;
	} else
	{
		idx = 0;
		top = MAXPLAYERS;
	}

	for( ; idx < top; idx++)
	{
		player_t *pl = players + idx;

		switch(spec_arg[2])
		{
			case PROP_NOTARGET:
				if(spec_arg[1])
					pl->mo->flags1 |= MF1_NOTARGET;
				else
					pl->mo->flags1 &= ~MF1_NOTARGET;
			break;
			case PROP_FLY:
				if(spec_arg[1])
					pl->mo->flags |= MF_NOGRAVITY;
				else
					pl->mo->flags &= ~MF_NOGRAVITY;
			break;
			case PROP_FROZEN:
			case PROP_TOTALLYFROZEN:
				if(spec_arg[1])
					pl->prop |= 1 << spec_arg[2];
				else
					pl->prop &= ~(1 << spec_arg[2]);
			break;
		}
	}

	return 1;
}

static inline uint32_t do_ChangeCamera()
{
	uint32_t idx, top;
	mobj_t *cam;

	if(spec_arg[0])
	{
		cam = mobj_by_tid_first(spec_arg[0]);
		if(!cam)
			return 0;
	}

	if(!spec_arg[1])
	{
		if(!activator)
			return 0;

		if(!activator->player)
			return 0;

		idx = activator->player - players;
		top = idx + 1;
	} else
	{
		idx = 0;
		top = MAXPLAYERS;
	}

	for( ; idx < top; idx++)
	{
		player_t *pl = players + idx;

		if(spec_arg[2])
			pl->prop |= (1 << PROP_CAMERA_MOVE);
		else
			pl->prop &= ~(1 << PROP_CAMERA_MOVE);

		if(spec_arg[0])
			pl->camera = cam;
		else
			pl->camera = pl->mo;
	}

	return 1;
}

static inline uint32_t do_GlassBreak(line_t *ln)
{
	fixed_t x, y;
	int32_t type;
	angle_t angle;
	mobj_t *mo;

	if(!ln)
		return 0;

	ln->flags &= ~(ML_BLOCKING | ML_BLOCK_ALL);
	type = !!(ln->flags & ML_REPEATABLE);
	do_line_switch(ln, type);
	if(ln->backsector)
		do_line_switch(ln, type | 2);

	if(spec_arg[0])
		return 0;

	if(!spec_arg[1])
		return 0;

	type = mobj_by_spawnid(spec_arg[1]);
	if(type < 0)
		return 0;

	x = ln->v1->x + (ln->v2->x - ln->v1->x) / 2;
	y = ln->v1->y + (ln->v2->y - ln->v1->y) / 2;
	angle = R_PointToAngle2(ln->v1->x, ln->v1->y, ln->v2->x, ln->v2->y);

	angle >>= ANGLETOFINESHIFT;
	x += finesine[angle] * 2;
	y -= finecosine[angle] * 2;

	for(uint32_t i = 0; i < 7; i++)
	{
		mo = P_SpawnMobj(x, y, ONFLOORZ, type);
		mo->angle = P_Random() << 24;
		mo->z = mo->subsector->sector->floorheight + 24 * FRACUNIT;

		angle = mo->angle >> ANGLETOFINESHIFT;
		mo->momx = (finecosine[angle] * P_Random()) >> 6;
		mo->momy = (finesine[angle] * P_Random()) >> 6;
		mo->momz = P_Random() << 11;

		mo->tics = P_Random() / 2;
	}

	return 0;
}

static uint32_t do_SetTextureOffset(line_t *li, line_t *ln)
{
	side_t *side;
	fixed_t ox, oy;

	if((spec_arg[4] & 7) != 7)
		return 0;

	if(spec_arg[3] == 0)
		side = sides + li->sidenum[0];
	else
	if(spec_arg[3] == 1 && li->sidenum[1] < numsides)
		side = sides + li->sidenum[1];
	else
		return 0;

	if(spec_arg[4] & 8)
	{
		ox = side->textureoffset;
		oy = side->rowoffset;
	} else
	{
		ox = 0;
		oy = 0;
	}

	ox += spec_arg[1];
	oy += spec_arg[2];

	if(spec_arg[1] != 0xFFFF0000)
		side->textureoffset = ox;

	if(spec_arg[2] != 0xFFFF0000)
		side->rowoffset = oy;

	return 1;
}

static uint32_t do_SetBlocking(line_t *li, line_t *ln)
{
	static uint16_t flag_table[] =
	{
		ML_BLOCKING,
		ML_BLOCKMONSTERS,
		ML_BLOCK_PLAYER,
		0, // unsupported - floaters
		0, // unsupported - projectile
		ML_BLOCK_ALL,
		0xFFFF
	};
	uint32_t set = 0;
	uint32_t clr = 0;
	uint32_t i, bit;

	for(i = 0, bit = 1; flag_table[i] != 0xFFFF; i++, bit <<= 1)
	{
		if(spec_arg[1] & bit)
			set |= flag_table[i];
		if(spec_arg[2] & bit)
			clr |= flag_table[i];
	}

	li->flags &= ~clr;
	li->flags |= set;

	return 1;
}

//
// tag handler

static uint32_t handle_tag(line_t *ln, uint32_t tag, uint32_t (*cb)(sector_t*,line_t*))
{
	uint32_t success = 0;

	if(!tag)
	{
		if(ln)
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

static void handle_tid(line_t *ln, int32_t tid, void (*cb)(mobj_t*))
{
	spec_success = 1;

	if(!tid)
	{
		if(activator)
			cb(activator);
		return;
	}

	for(thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		uint32_t ret;
		mobj_t *mo;

		if(th->function != (void*)P_MobjThinker)
			continue;

		mo = (mobj_t*)th;

		if(tid > 0 && mo->special.tid != tid)
			continue;

		cb(mo);
	}
}

//
// lid handler

static uint32_t handle_lid(line_t *ln, uint32_t lid, uint32_t (*cb)(line_t*,line_t*))
{
	uint32_t success = 0;

	for(uint32_t i = 0; i < numlines; i++)
	{
		line_t *li = lines + i;
		if(li->id == lid)
			success |= cb(li, ln);
	}

	return success;
}

//
// API

void spec_activate(line_t *ln, mobj_t *mo, uint32_t type)
{
	uint32_t back_side = type & SPEC_ACT_BACK_SIDE;

	spec_success = 0;

	if(ln)
	{
		spec_special = ln->special;
		spec_arg[0] = ln->arg0;
		spec_arg[1] = ln->arg1;
		spec_arg[2] = ln->arg2;
		spec_arg[3] = ln->arg3;
		spec_arg[4] = ln->arg4;
		type &= SPEC_ACT_TYPE_MASK;

		switch(ln->flags & ML_ACT_MASK)
		{
			case MLA_PLR_CROSS:
				if(type != SPEC_ACT_CROSS)
					return;
				if(mo->player)
					break;
				if(	mo->flags & MF_MISSILE &&
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
				if(mo->flags1 & MF1_ISMONSTER)
					break;
				if(	ln->special != 70 &&	// Teleport
					ln->special != 71	// Teleport_NoFog
				)
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
							ln->special != 70 &&	// Teleport
							ln->special != 71	// Teleport_NoFog
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
					if(!mo->player && !(ln->flags & ML_MONSTER_ACT))
						return;
					break;
				}
				if(type != SPEC_ACT_SHOOT)
					return;
				if(!mo->player && !(ln->flags & ML_MONSTER_ACT))
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
	}

	activator = mo;

	switch(spec_special)
	{
		case 2: // Polyobj_RotateLeft
			spec_success = poly_rotate(NULL, 0);
		break;
		case 3: // Polyobj_RotateRight
			spec_success = poly_rotate(NULL, 1);
		break;
		case 4: // Polyobj_Move
			spec_success = poly_move(NULL, 0);
		break;
		case 7: // Polyobj_DoorSwing
			spec_success = poly_rotate(NULL, 2);
		break;
		case 8: // Polyobj_DoorSlide
			spec_success = poly_move(NULL, 1);
		break;
		case 10: // Door_Close
			spec_success = handle_tag(ln, spec_arg[0], act_Door_Close);
		break;
		case 12: // Door_Raise
			door_monster_hack = !mo->player;
			value_mult = 0;
			spec_success = handle_tag(ln, spec_arg[0], act_Door_Raise);
		break;
		case 13: // Door_LockedRaise
			door_monster_hack = 0;
			value_mult = 1;
			if(spec_arg[3])
			{
				uint8_t *message;
				message = mobj_check_keylock(activator, spec_arg[3], spec_arg[0]);
				if(message)
				{
					if(activator->player && message[0])
						activator->player->message = message;
					value_mult = 0;
				}
			}
			if(value_mult)
				spec_success = handle_tag(ln, spec_arg[0], act_Door_Raise);
		break;
		case 15: // Autosave
		{
			int32_t lump = W_CheckNumForName("WIAUTOSV");
			if(lump >= 0)
			{
				V_DrawPatchDirect(0, 0, W_CacheLumpNum(lump, PU_CACHE));
				I_FinishUpdate();
			}
			if(ln)
				ln->special = 0;
			save_auto(0);
			spec_success = 1;
		}
		break;
		case 19: // Thing_Stop
			handle_tid(ln, spec_arg[0], act_Thing_Stop);
		break;
		case 20: // Floor_LowerByValue
			value_mult = -1;
			spec_success = handle_tag(ln, spec_arg[0], act_Floor_ByValue);
		break;
		case 23: // Floor_RaiseByValue
			value_mult = 1;
			spec_success = handle_tag(ln, spec_arg[0], act_Floor_ByValue);
		break;
		case 26: // Stairs_BuildDown
			value_mult = -1;
			value_offs = 0;
			spec_success = handle_tag(ln, spec_arg[0], act_Stairs_Build);
		break;
		case 27: // Stairs_BuildUp
			value_mult = 1;
			value_offs = 0;
			spec_success = handle_tag(ln, spec_arg[0], act_Stairs_Build);
		break;
		case 28: // Floor_RaiseAndCrush
			value_mult = 0;
			spec_success = handle_tag(ln, spec_arg[0], act_Floor_Crush);
		break;
		case 31: // Stairs_BuildDownSync
			value_mult = -1;
			value_offs = 1;
			spec_success = handle_tag(ln, spec_arg[0], act_Stairs_Build);
		break;
		case 32: // Stairs_BuildUpSync
			value_mult = 1;
			value_offs = 1;
			spec_success = handle_tag(ln, spec_arg[0], act_Stairs_Build);
		break;
		case 35: // Floor_RaiseByValueTimes8
			value_mult = 8;
			spec_success = handle_tag(ln, spec_arg[0], act_Floor_ByValue);
		break;
		case 36: // Floor_LowerByValueTimes8
			value_mult = -8;
			spec_success = handle_tag(ln, spec_arg[0], act_Floor_ByValue);
		break;
		case 37: // Floor_MoveToValue
			value_mult = 0;
			spec_success = handle_tag(ln, spec_arg[0], act_Floor_ByValue);
		break;
		case 40: // Ceiling_LowerByValue
			value_mult = -1;
			spec_success = handle_tag(ln, spec_arg[0], act_Ceiling_ByValue);
		break;
		case 41: // Ceiling_RaiseByValue
			value_mult = 1;
			spec_success = handle_tag(ln, spec_arg[0], act_Ceiling_ByValue);
		break;
		case 44: // Ceiling_CrushStop
			spec_success = handle_tag(ln, spec_arg[0], act_Ceiling_CrushStop);
		break;
		case 47: // Ceiling_MoveToValue
			value_mult = 0;
			spec_success = handle_tag(ln, spec_arg[0], act_Ceiling_ByValue);
		break;
		case 49: // GlassBreak
			spec_success = do_GlassBreak(ln);
		break;
		case 53: // Line_SetTextureOffset
			spec_success = handle_lid(ln, spec_arg[0], do_SetTextureOffset);
		case 55: // Line_SetBlocking
			spec_success = handle_lid(ln, spec_arg[0], do_SetBlocking);
		break;
		case 62: // Plat_DownWaitUpStay
			value_mult = -1;
			value_offs = 8 * FRACUNIT;
			spec_success = handle_tag(ln, spec_arg[0], act_Plat_Bidir);
		break;
		case 64: // Plat_UpWaitDownStay
			value_mult = 0;
			spec_success = handle_tag(ln, spec_arg[0], act_Plat_Bidir);
		break;
		case 70: // Teleport
			if(!back_side)
			{
				value_mult = -1;
				spec_success = do_Teleport();
			}
		break;
		case 71: // Teleport_NoFog
			if(!back_side)
			{
				value_mult = 0;
				spec_success = do_Teleport();
			}
		break;
		case 72: // ThrustThing
			handle_tid(ln, spec_arg[3], act_ThrustThing);
		break;
		case 73: // DamageThing
			if(activator)
			{
				if(spec_arg[0] >= 0)
				{
					int32_t damage = spec_arg[0];
					if(!damage)
						damage = 1000000;
					mobj_damage(activator, NULL, NULL, damage, NULL);
				} else
					mobj_give_health(activator, -spec_arg[0], 0);
				spec_success = 1;
			}
		break;
		case 74: // Teleport_NewMap
			secretexit = 0;
			gameaction = ga_completed;
			map_next_levelnum = spec_arg[0];
			map_start_id = spec_arg[1];
			map_start_facing = !!spec_arg[2];
			spec_success = 1;
		break;
		case 76: // TeleportOther
			spec_success = do_TeleportOther();
		break;
		case 78: // TeleportInSector
			spec_success = do_TeleportInSector();
		break;
		case 97: // Ceiling_LowerAndCrushDist
			spec_success = handle_tag(ln, spec_arg[0], act_Ceiling_LowerAndCrushDist);
		break;
		case 99: // Floor_RaiseAndCrushDoom
			value_mult = 1;
			spec_success = handle_tag(ln, spec_arg[0], act_Floor_Crush);
		break;
		case 110: // Light_RaiseByValue
			value_offs = spec_arg[1];
			spec_success = handle_tag(ln, spec_arg[0], act_Light_ByValue);
		break;
		case 111: // Light_LowerByValue
			value_offs = -spec_arg[1];
			spec_success = handle_tag(ln, spec_arg[0], act_Light_ByValue);
		break;
		case 112: // Light_ChangeToValue
			spec_success = handle_tag(ln, spec_arg[0], arg_Light_ChangeToValue);
		break;
		case 113: // Light_Fade
			spec_success = handle_tag(ln, spec_arg[0], arg_Light_Fade);
		break;
		case 114: // Light_Glow
			spec_success = handle_tag(ln, spec_arg[0], arg_Light_Glow);
		break;
		case 116: // Light_Strobe
			spec_success = handle_tag(ln, spec_arg[0], arg_Light_Strobe);
		break;
		case 117: // Light_Stop
			spec_success = handle_tag(ln, spec_arg[0], arg_Light_Stop);
		break;
		case 119: // Thing_Damage
			handle_tid(ln, spec_arg[0], act_Thing_Damage);
		break;
		case 127: // Thing_SetSpecial
			handle_tid(ln, spec_arg[0], act_Thing_SetSpecial);
		break;
		case 128: // ThrustThingZ
			handle_tid(ln, spec_arg[0], act_ThrustThingZ);
		break;
		case 130: // Thing_Activate
			handle_tid(ln, spec_arg[0], act_Thing_Activate);
		break;
		case 131: // Thing_Deactivate
			handle_tid(ln, spec_arg[0], act_Thing_Deactivate);
		break;
		case 132: // Thing_Remove
			handle_tid(ln, spec_arg[0], act_Thing_Remove);
		break;
		case 133: // Thing_Destroy
			handle_tid(ln, spec_arg[0] > 0 ? spec_arg[0] : -1, act_Thing_Destroy);
		break;
		case 134: // Thing_Projectile
			value_mult = 2;
			value_offs = 1;
			goto thing_spawn;
		break;
		case 135: // Thing_Spawn
			value_mult = 1;
			value_offs = 1;
			if(spec_arg[0] != spec_arg[3])
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
			if(spec_arg[0] != spec_arg[3])
				goto thing_spawn;
		break;
		case 139: // Thing_SpawnFacing
			value_mult = 0;
			value_offs = !spec_arg[2];
			if(spec_arg[0] == spec_arg[3])
				break;
thing_spawn:
			if(spec_arg[1])
			{
				spawn_type = mobj_by_spawnid(spec_arg[1]);
				if(spawn_type >= 0)
					handle_tid(ln, spec_arg[0], act_Thing_Spawn);
			}
		break;
		case 172: // Plat_UpNearestWaitDownStay
			value_mult = 1;
			spec_success = handle_tag(ln, spec_arg[0], act_Plat_Bidir);
		break;
		case 173: // NoiseAlert
			spec_success = do_NoiseAlert();
		break;
		case 176: // Thing_ChangeTID
			if(!spec_arg[1] || spec_arg[0] != spec_arg[1])
				handle_tid(ln, spec_arg[0], act_Thing_ChangeTID);
		break;
		case 179: // ChangeSkill
			if(spec_arg[0] > sk_nightmare)
				gameskill = sk_nightmare;
			else
			if(spec_arg[0] < sk_baby)
				gameskill = sk_baby;
			else
				gameskill = spec_arg[0];
			spec_success = 1;
		break;
		case 191: // SetPlayerProperty
			spec_success = do_SetPlayerProperty();
		break;
		case 195: // Ceiling_CrushRaiseAndStayA
			value_mult = 0;
			spec_success = handle_tag(ln, spec_arg[0], act_Ceiling_Crush);
		break;
		case 196: // Ceiling_CrushAndRaiseA
			value_mult = 1;
			spec_success = handle_tag(ln, spec_arg[0], act_Ceiling_Crush);
		break;
		case 198: // Ceiling_RaiseByValueTimes8
			value_mult = 8;
			spec_success = handle_tag(ln, spec_arg[0], act_Ceiling_ByValue);
		break;
		case 199: // Ceiling_LowerByValueTimes8
			value_mult = -8;
			spec_success = handle_tag(ln, spec_arg[0], act_Ceiling_ByValue);
		break;
		case 200: // Generic_Floor
			spec_success = handle_tag(ln, spec_arg[0], act_Generic_Floor);
		break;
		case 201: // Generic_Ceiling
			spec_success = handle_tag(ln, spec_arg[0], act_Generic_Ceiling);
		break;
		case 206: // Plat_DownWaitUpStayLip
			value_mult = -1;
			value_offs = spec_arg[3] * FRACUNIT;
			spec_success = handle_tag(ln, spec_arg[0], act_Plat_Bidir);
		break;
		case 212: // Sector_SetColor
			spec_success = handle_tag(ln, spec_arg[0], act_SetColor);
		break;
		case 213: // Sector_SetFade
			spec_success = handle_tag(ln, spec_arg[0], act_SetFade);
		break;
		case 237: // ChangeCamera
			spec_success = do_ChangeCamera();
		break;
		case 239: // Floor_RaiseByValueTxTy
			value_mult = 1;
			spec_success = handle_tag(ln, spec_arg[0], act_Floor_ByValue);
		break;
		case 243: // Exit_Normal
			secretexit = 0;
			gameaction = ga_completed;
			map_start_id = spec_arg[0];
			spec_success = 1;
		break;
		case 244: // Exit_Secret
			secretexit = 1;
			gameaction = ga_completed;
			map_start_id = spec_arg[0];
			spec_success = 1;
		break;
		default:
		{
			player_t *pl = players + consoleplayer;
			// doom_printf("special %u; side %u; mo 0x%08X; pl 0x%08X\n", spec_special, !!back_side, mo, mo->player);
			pl->text.tic = leveltime + 5 * 35;
			pl->text.font = 0;
			pl->text.text = "Unsupported special!";
			pl->text.lines = 1;
		}
		break;
	}

	if(ln && spec_success)
		do_line_switch(ln, !!(ln->flags & ML_REPEATABLE));
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

__attribute((regparm(2),no_caller_saved_registers))
uint32_t EV_Teleport(line_t *ln, uint32_t side)
{
	register mobj_t *mo asm("ebx"); // hack to extract 3rd argument

	if(side)
		return 0;

	if(mo->flags1 & MF1_NOTELEPORT)
		return 0;

	for(thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		mobj_t *target;

		if(th->function != (void*)P_MobjThinker)
			continue;

		target = (mobj_t*)th;

		if(target->type != 41) // MT_TELEPORTMAN
			continue;

		if(target->subsector->sector->tag == ln->tag)
			return mobj_teleport(mo, target->x, target->y, target->z, target->angle, TELEF_USE_ANGLE | TELEF_FOG);
	}

	return 0;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// 'PTR_UseTraverse' checks for ln->special (8bit)
	{0x0002BCA9, CODE_HOOK | HOOK_UINT32, 0x00127B80},
	{0x0002BCAD, CODE_HOOK | HOOK_UINT8, 0x90},
	// replace 'EV_Teleport'
	{0x00031E40, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)EV_Teleport},
};

