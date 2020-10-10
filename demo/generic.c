// kgsws' Doom ACE
// Generic sector mover.
#include "engine.h"
#include "defs.h"
#include "map.h"
#include "generic.h"

//
// DOOR
__attribute((regparm(1),no_caller_saved_registers))
void think_door_mover(generic_mover_t *gm)
{
	if(gm->info.sleeping)
	{
		gm->info.sleeping--;
		if(gm->info.sleeping)
			return;
		// start again
		if(gm->info.closesound)
			S_StartSound((mobj_t *)&gm->sector->soundorg, gm->info.closesound);
		// swap direction
		fixed_t tmp = gm->info.start;
		gm->info.start = gm->info.stop;
		gm->info.stop = tmp;
	}

	if(gm->info.start > gm->info.stop)
	{
		fixed_t height = gm->sector->ceilingheight;

		// lower
		gm->sector->ceilingheight = height - gm->info.speed;

		// do not overshoot
		if(gm->sector->ceilingheight < gm->info.stop)
			gm->sector->ceilingheight = gm->info.stop;

		if(P_ChangeSector(gm->sector, !!gm->info.crushspeed))
		{
			// move blocked
			if(gm->info.crushspeed > 0)
				// change speed and continue
				gm->info.speed = gm->info.crushspeed;
			else
			{
				// restore original height
				gm->sector->ceilingheight = height;
				P_ChangeSector(gm->sector, 0);
				// and change direction
				if(gm->info.delay >= 0)
				{
					fixed_t tmp = gm->info.start;
					gm->info.start = gm->info.stop;
					gm->info.stop = tmp;
					if(gm->info.opensound)
						S_StartSound((mobj_t *)&gm->sector->soundorg, gm->info.opensound);
				}
			}
		}
	} else
	{
		// raise
		gm->sector->ceilingheight += gm->info.speed;

		// do not overshoot
		if(gm->sector->ceilingheight > gm->info.stop)
			gm->sector->ceilingheight = gm->info.stop;

		P_ChangeSector(gm->sector, 0);
	}
	// light effect
	if(gm->info.lighttag)
	{
		int diff = abs(gm->info.start - gm->info.stop);
		diff = ((uint32_t)(gm->info.lightmax - gm->info.lightmin) << 16) / (diff >> (FRACBITS - 4));
		diff *= abs(gm->info.stop - gm->sector->ceilingheight) >> (FRACBITS - 4);
		diff >>= 16;

		if(gm->info.start < gm->info.stop)
			diff = gm->info.lightmax - diff;
		else
			diff += gm->info.lightmin;

		// set for all sectors
		sector_t *sec = *sectors;
		for(int i = 0; i < *numsectors; i++, sec++)
		{
			if(sec->tag == gm->info.lighttag)
				sec->lightlevel = diff;
		}
	}
	// finished?
	if(gm->sector->ceilingheight == gm->info.stop)
	{
		// yes
		if(gm->info.stopsound)
			S_StartSound((mobj_t *)&gm->sector->soundorg, gm->info.stopsound);

		if(gm->info.delay >= 0 && gm->info.start < gm->info.stop)
		{
			// opened; setup closing
			gm->info.sleeping = gm->info.delay + 1;
		} else
		{
			// finished for real
			P_RemoveThinker(&gm->thinker);
			gm->sector->specialdata = NULL;
		}
	} else
	if(gm->info.movesound && !(*leveltime & 7)) // maybe replace with something like S_HasSoundPlaying ?
		S_StartSound((mobj_t *)&gm->sector->soundorg, gm->info.movesound);
}

__attribute((regparm(2)))
void generic_door(sector_t *sec, generic_mover_info_t *info)
{
	generic_mover_t *gm;

	// remove old thinker
	if(sec->specialdata)
		P_RemoveThinker(sec->specialdata);

	// check speed
	if(!info->speed)
	{
		sec->specialdata = NULL;
		return;
	}

	// allocate memory
	gm = Z_Malloc(sizeof(generic_mover_t), PU_LEVELSPEC, NULL);
	P_AddThinker(&gm->thinker);
	sec->specialdata = gm;

	// copy settings
	gm->thinker.function = (void*)think_door_mover;
	gm->sector = sec;
	gm->info = *info;
}

__attribute((regparm(1)))
void generic_door_toggle(generic_mover_t *gm)
{
	// just change the direction
	if(gm->info.delay < 0)
		// not allowed
		return;

	if(gm->info.start < gm->info.stop)
	{
		// was opening
		if(gm->info.closesound)
			S_StartSound((mobj_t *)&gm->sector->soundorg, gm->info.closesound);
	} else
	{
		// was closing
		if(gm->info.opensound)
			S_StartSound((mobj_t *)&gm->sector->soundorg, gm->info.opensound);
	}

	// wake up
	gm->info.sleeping = 0;

	// swap direction
	fixed_t tmp = gm->info.start;
	gm->info.start = gm->info.stop;
	gm->info.stop = tmp;
}

//
// CEILING
static __attribute((regparm(1),no_caller_saved_registers))
void think_ceiling_mover(generic_mover_t *gm)
{
	if(gm->info.start > gm->info.stop)
	{
		fixed_t height = gm->sector->ceilingheight;

		// lower
		gm->sector->ceilingheight = height - gm->info.speed;

		// do not overshoot
		if(gm->sector->ceilingheight < gm->info.stop)
			gm->sector->ceilingheight = gm->info.stop;

		if(P_ChangeSector(gm->sector, !!gm->info.crushspeed))
		{
			// move blocked
			if(gm->info.crushspeed)
				// change speed and continue
				gm->info.speed = gm->info.crushspeed;
			else
			{
				// restore original height
				gm->sector->ceilingheight = height;
				P_ChangeSector(gm->sector, 0);
			}
		}
	} else
	{
		// raise
		gm->sector->ceilingheight += gm->info.speed;

		// do not overshoot
		if(gm->sector->ceilingheight > gm->info.stop)
			gm->sector->ceilingheight = gm->info.stop;

		P_ChangeSector(gm->sector, 0);
	}
	// finished?
	if(gm->sector->ceilingheight == gm->info.stop)
	{
		// yes
		if(gm->info.stopsound)
			S_StartSound((mobj_t *)&gm->sector->soundorg, gm->info.stopsound);

		gm->sector->ceilingpic = gm->info.texture;
		gm->sector->special = gm->info.special;
		gm->sector->lightlevel = gm->info.light;

		P_RemoveThinker(&gm->thinker);
		gm->sector->specialdata = NULL;
	} else
	if(gm->info.movesound && !(*leveltime & 7))
		S_StartSound((mobj_t *)&gm->sector->soundorg, gm->info.movesound);
}

__attribute((regparm(2)))
void generic_ceiling(sector_t *sec, generic_mover_info_t *info)
{
	generic_mover_t *gm;

	// remove old thinker
	if(sec->specialdata)
		P_RemoveThinker(sec->specialdata);

	// check speed
	if(!info->speed)
	{
		sec->specialdata = NULL;
		return;
	}

	// allocate memory
	gm = Z_Malloc(sizeof(generic_mover_t), PU_LEVELSPEC, NULL);
	P_AddThinker(&gm->thinker);
	sec->specialdata = gm;

	// copy settings
	gm->thinker.function = (void*)think_ceiling_mover;
	gm->sector = sec;
	gm->info = *info;
}

