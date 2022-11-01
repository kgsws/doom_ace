// kgsws' ACE Engine
////
// Generic floor / ceiling mover.
// Replaces all original types. Used only in Hexen map format.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "think.h"
#include "generic.h"

//
// thinkers

static __attribute((regparm(2),no_caller_saved_registers))
void think_ceiling(generic_mover_t *gm)
{
	sector_t *sec;

	if(gm->wait)
	{
		gm->wait--;
		return;
	}

	sec = gm->sector;

	if(gm->direction == DIR_UP)
	{
		sec->ceilingheight += gm->speed;
		if(sec->ceilingheight >= gm->top_height)
		{
			sec->ceilingheight = gm->top_height;
			if(gm->flags & MVF_TOP_REVERSE)
			{
				gm->direction = !gm->direction;
				gm->wait = gm->delay;
				return;
			}
			goto finish_move;
		}
	} else
	{
		sec->ceilingheight -= gm->speed;
		if(sec->ceilingheight <= gm->bot_height)
		{
			sec->ceilingheight = gm->bot_height;
			if(gm->flags & MVF_BOT_REVERSE)
			{
				gm->direction = !gm->direction;
				gm->wait = gm->delay;
				return;
			}
			goto finish_move;
		}
	}

	return;

finish_move:
	gm->thinker.function = (void*)-1;
	sec->specialactive &= ~ACT_CEILING;
}

//
// API

generic_mover_t *generic_ceiling(sector_t *sec)
{
	generic_mover_t *gm;

	if(sec->specialactive & ACT_CEILING)
		return NULL;
	sec->specialactive |= ACT_CEILING;

	gm = Z_Malloc(sizeof(generic_mover_t), PU_LEVELSPEC, NULL);
	memset(gm, 0, sizeof(generic_mover_t));
	gm->thinker.function = think_ceiling;
	gm->sector = sec;
	think_add(&gm->thinker);

	return gm;
}

generic_mover_t *generic_ceiling_by_sector(sector_t *sec)
{
	for(thinker_t *th = thcap.next; th != &thcap; th = th->next)
	{
		generic_mover_t *gm;

		if(th->function != think_ceiling)
			continue;

		gm = (generic_mover_t*)th;
		if(gm->sector == sec)
			return gm;
	}

	return NULL;
}

