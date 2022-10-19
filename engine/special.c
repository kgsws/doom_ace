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
#include "special.h"

//
// API

void spec_activate(line_t *ln, mobj_t *mo, uint32_t type)
{
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

	doom_printf("special %u; side %u; mo 0x%08X; pl 0x%08X\n", ln->special, !!(type & SPEC_ACT_BACK_SIDE), mo, mo->player);
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

