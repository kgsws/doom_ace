// kgsws' Doom ACE
// Custom code pointers.
#include "engine.h"
#include "defs.h"
#include "action.h"
#include "decorate.h"

__attribute((regparm(2),no_caller_saved_registers))
void A_NoBlocking(mobj_t *mo)
{
	arg_droplist_t *drop;
	mobj_t *item;

	// make non-solid
	mo->flags &= ~MF_SOLID;

	// drop items
	drop = mo->state->extra;
	if(!drop)
		return;

	for(uint32_t i = 0; i < drop->count+1; i++)
	{
		uint32_t tc = drop->typecombo[i];
		if(P_Random() > (tc >> 24))
			continue;
		item = P_SpawnMobj(mo->x, mo->y, mo->z + (8 << FRACBITS), tc & 0xFFFFFF);
		item->flags |= MF_DROPPED;
		item->angle = P_Random() << 24;
		item->momx = 2*FRACUNIT - (P_Random() << 9);
		item->momy = 2*FRACUNIT - (P_Random() << 9);
		item->momz = (3 << 15) + 2*(P_Random() << 9);
	}
}

