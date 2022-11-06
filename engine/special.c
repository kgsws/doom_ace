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
#include "sound.h"
#include "animate.h"
#include "generic.h"
#include "special.h"

enum
{
	MVT_GENERIC,
	MVT_DOOR,
	MVT_PLAT,
};

//

line_t spec_magic_line;
uint32_t spec_success;

static uint_fast8_t door_monster_hack;
static fixed_t value_mult;

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
	gm->speed_start = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->speed_now = gm->speed_start;
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

		if(gm->direction != DIR_DOWN && door_monster_hack)
			return 0;

		if(gm->wait)
		{
			gm->wait = 0;
			if(gm->up_seq)
			{
				if(gm->up_seq->start)
					S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->start);
				gm->sndwait = gm->dn_seq->delay;
			}
		} else
			gm->direction = !gm->direction;

		return 1;
	}

	height = P_FindLowestCeilingSurrounding(sec) - 4 * FRACUNIT;

	if(sec->ceilingheight == height)
		return 1;

	gm = generic_ceiling(sec, DIR_UP, SNDSEQ_DOOR, ln->arg1 >= 64);
	gm->top_height = height;
	gm->bot_height = sec->floorheight;
	gm->speed_start = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->speed_now = gm->speed_start;
	gm->type = MVT_DOOR;
	gm->flags = MVF_TOP_REVERSE | MVF_BLOCK_GO_UP;
	gm->lighttag = ln->arg3;
	if(!ln->arg2)
	{
		gm->delay = 1;
		gm->flags |= MVF_WAIT_STOP;
	} else
		gm->delay = ln->arg2;

	return 1;
}

//
// floors

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

	gm = generic_floor(sec, value_mult < 0 ? DIR_DOWN : DIR_UP, SNDSEQ_STNMOV, 0);
	gm->top_height = dest;
	gm->bot_height = dest;
	gm->speed_start = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->speed_now = gm->speed_start;
	gm->flags = MVF_BLOCK_STAY;

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

	gm = generic_ceiling(sec, value_mult < 0 ? DIR_DOWN : DIR_UP, SNDSEQ_STNMOV, 0);
	gm->top_height = dest;
	gm->bot_height = dest;
	gm->speed_start = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->speed_now = gm->speed_start;
	gm->flags = MVF_BLOCK_STAY;

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
			if(!(ln->flags & ML_MONSTER_ACT))
				return;
			if(!(mo->flags1 & MF1_ISMONSTER))
				return;
		break;
		case MLA_PLR_USE:
			if(type != SPEC_ACT_USE)
				return;
			if(	!mo->player &&
				!(ln->flags & ML_MONSTER_ACT) &&
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

	switch(ln->special)
	{
		case 10: // Door_Close
			spec_success = handle_tag(ln, ln->arg0, act_Door_Close);
		break;
		case 12: // Door_Raise
			door_monster_hack = !mo->player;
			spec_success = handle_tag(ln, ln->arg0, act_Door_Raise);
		break;
		case 20: // Floor_LowerByValue
			value_mult = -1;
			spec_success = handle_tag(ln, ln->arg0, act_Floor_ByValue);
		break;
		case 23: // Floor_RaiseByValue
			value_mult = 1;
			spec_success = handle_tag(ln, ln->arg0, act_Floor_ByValue);
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

