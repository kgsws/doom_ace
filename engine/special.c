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
		// TODO: update things
		return 1;
	}

	gm = generic_ceiling(sec);
	if(!gm)
		return 0;

	gm->top_height = sec->ceilingheight;
	gm->bot_height = sec->floorheight;
	gm->speed = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->direction = DIR_DOWN;

	// todo 'lighttag' arg2

	return 1;
}

static uint32_t act_Door_Raise(sector_t *sec, line_t *ln)
{
	generic_mover_t *gm;
	fixed_t height;

	height = P_FindLowestCeilingSurrounding(sec) - 4 * FRACUNIT;

	if(sec->ceilingheight == height)
	{
		// find existing door
		gm = generic_ceiling_by_sector(sec);
		if(gm && gm->type == MVT_DOOR && gm->wait)
			gm->wait = 0;
		return 1;
	}

	if(sec->ceilingheight > height)
	{
		sec->ceilingheight = height;
		// TODO: update things
		return 1;
	}

	gm = generic_ceiling(sec);
	if(!gm)
	{
		// find existing door, reverse direction
		gm = generic_ceiling_by_sector(sec);

		if(!gm)
			return 0;

		if(gm->type != MVT_DOOR)
			return 0;

		if(gm->wait)
			gm->wait = 0;
		else
			gm->direction = !gm->direction;

		return 1;
	}

	gm->top_height = height;
	gm->bot_height = sec->floorheight;
	gm->speed = (fixed_t)ln->arg1 * (FRACUNIT/8);
	gm->type = MVT_DOOR;
	gm->direction = DIR_UP;
	gm->delay = ln->arg2;
	gm->flags = MVF_TOP_REVERSE;

	// todo 'lighttag' arg3

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
	uint32_t success = 0;

	if(!ln)
	{
		ln = &spec_magic_line;
		if(ln->tag == 0)
			return;
	} else
	switch(ln->flags & ML_ACT_MASK)
	{
		case MLA_PLR_CROSS:
			if((type & SPEC_ACT_TYPE_MASK) != SPEC_ACT_CROSS)
				return;
			if(mo->player)
				break;
			if(!(ln->flags & ML_MONSTER_ACT))
				return;
			if(!(mo->flags1 & MF1_ISMONSTER))
				return;
		break;
		case MLA_PLR_USE:
			if(type != SPEC_ACT_USE) // ignore back side
				return;
			if(!(mo->player) && !(ln->flags & ML_MONSTER_ACT))
				return;
		break;
		case MLA_MON_CROSS:
			if(!(mo->flags1 & MF1_ISMONSTER))
				return;
			if((type & SPEC_ACT_TYPE_MASK) != SPEC_ACT_CROSS)
				return;
		break;
		case MLA_ATK_HIT:
			if(	(type & SPEC_ACT_TYPE_MASK) == SPEC_ACT_CROSS ||
				(type & SPEC_ACT_TYPE_MASK) == SPEC_ACT_BUMP
			)
			{
				if(!(mo->flags & MF_MISSILE))
					return;
				break;
			}
			if(type != SPEC_ACT_SHOOT) // there is no 'back side'
				return;
		break;
		case MLA_PLR_BUMP:
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
			if((type & SPEC_ACT_TYPE_MASK) != SPEC_ACT_CROSS)
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
			success = handle_tag(ln, ln->arg0, act_Door_Close);
		break;
		case 12: // Door_Raise
			success = handle_tag(ln, ln->arg0, act_Door_Raise);
		break;
		default:
			doom_printf("special %u; side %u; mo 0x%08X; pl 0x%08X\n", ln->special, !!(type & SPEC_ACT_BACK_SIDE), mo, mo->player);
		break;
	}

	if(success && ln != &spec_magic_line)
		do_line_switch(ln, ln->flags & ML_REPEATABLE);
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
void spec_line_cross(uint32_t lidx, uint32_t side)
{
	register mobj_t *mo asm("ebx"); // hack to extract 3rd argument
	spec_activate(lines + lidx, mo, (side * SPEC_ACT_BACK_SIDE) | SPEC_ACT_CROSS);
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t spec_line_use(mobj_t *mo, line_t *ln)
{
	register uint32_t side asm("ebx"); // hack to extract 3rd argument

	if(side)
		return 0;

	spec_activate(ln, mo, SPEC_ACT_USE);

	return 0;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// 'PTR_UseTraverse' checks for ln->special (8bit)
	{0x0002BCA9, CODE_HOOK | HOOK_UINT32, 0x00127B80},
	{0x0002BCAD, CODE_HOOK | HOOK_UINT8, 0x90},
};

