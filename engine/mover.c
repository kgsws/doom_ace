// kgsws' ACE Engine
////
// Path followers.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "mobj.h"
#include "mover.h"

//
// API

void mover_tick(mobj_t *mo)
{
	uint32_t next_tid;
	mobj_t *th;
	fixed_t xx, yy, zz;
	angle_t aa, pp;

	if(mo->iflags & MFI_FOLLOW_INIT)
	{
		// interpolation check
		if(!(mo->special.arg[2] & 1))
			engine_error("MOVER", "Invalid interpolation!");

		// actor mover
		if(mo->spawnpoint.type == 9074)
		{
			// find actor to move
			th = mobj_by_tid_first(mo->special.arg[3]);
			if(!th)
				goto stop_moving;
			mo->tracer = th;
		}

		// instantly move to first point
		mo->iflags &= ~MFI_FOLLOW_INIT;

		next_tid = mo->special.arg[0];
		next_tid += (uint32_t)mo->special.arg[1] * 256;

		th = mobj_by_tid_first(next_tid);
		if(!th)
			goto stop_moving;
		if(th->spawnpoint.type != 9070) // interpolation point
			goto stop_moving;

		mo->target = th;

		P_UnsetThingPosition(mo);
		mo->x = th->x;
		mo->y = th->y;
		mo->z = th->z;
		// TODO: ZDoom sets angle after first delay
		P_SetThingPosition(mo);
		mo->floorz = mo->subsector->sector->floorheight;
		mo->ceilingz = mo->subsector->sector->ceilingheight;

		if(mo->tracer)
		{
			fixed_t oldz = mo->tracer->z;
			mo->tracer->z = mo->z;
			if(P_TryMove(mo->tracer, mo->x, mo->y))
			{
				mo->tracer->momx = 0;
				mo->tracer->momy = 0;
				mo->tracer->momz = 0;
			} else
				mo->tracer->z = oldz;
		}

		// setup next point right away
		mo->reactiontime = ((uint32_t)mo->target->special.arg[2] * 35) / 8;
		mo->reactiontime++;
	}

	if(mo->spawnpoint.type == 9074 && !mo->tracer)
		// thing was removed
		goto stop_moving;

	if(mo->reactiontime)
	{
		// delay
		mo->reactiontime--;
		if(!mo->reactiontime)
		{
			// set angles
			mo->angle = mo->target->angle;
			mo->pitch = mo->target->pitch;

			// move time
			mo->threshold = ((uint32_t)mo->target->special.arg[1] * 35) / 8;
			mo->threshold++;

			// get new point
			next_tid = mo->target->special.arg[3];
			next_tid += (uint32_t)mo->target->special.arg[4] * 256;
			if(!next_tid)
				goto stop_moving;

			// find new point
			th = mobj_by_tid_first(next_tid);
			if(!th)
				goto stop_moving;
			if(th->spawnpoint.type != 9070) // interpolation point
				goto stop_moving;

			mo->target = th;

			// calculate steps
			mo->momx = (th->x - mo->x) / mo->threshold;
			mo->momy = (th->y - mo->y) / mo->threshold;
			mo->momz = (th->z - mo->z) / mo->threshold;

			if(mo->special.arg[2] & 2)
			{
				if(th->angle != mo->angle)
				{
					angle_t angle = th->angle - mo->angle;
					if(angle > 0x80000000)
					{
						angle = -angle;
						mo->mover.angle = angle / mo->threshold;
						mo->mover.angle = -mo->mover.angle;
					} else
						mo->mover.angle = angle / mo->threshold;
				} else
					mo->mover.angle = 0;
			}

			if(mo->special.arg[2] & 4)
			{
				if(th->pitch != mo->pitch)
				{
					mo->mover.pitch = (int32_t)(th->pitch + ANG90) - (int32_t)(mo->pitch + ANG90);
					mo->mover.pitch /= mo->threshold;
				} else
					mo->mover.pitch = 0;
			}
		} else
		if(mo->tracer)
			mo->tracer->iflags &= ~MFI_FOLLOW_MOVE;
		return;
	}

	// change position

	xx = mo->x + mo->momx;
	yy = mo->y + mo->momy;
	zz = mo->z + mo->momz;
	if(mo->special.arg[2] & 2)
		aa = mo->angle + mo->mover.angle;
	if(mo->special.arg[2] & 4)
		pp = mo->pitch + mo->mover.pitch;

	if(mo->tracer)
	{
		fixed_t oldz = mo->tracer->z;
		mo->tracer->z = zz;
		if(P_TryMove(mo->tracer, xx, yy))
		{
			if(mo->special.arg[2] & 2)
				mo->tracer->angle = aa;
			if(mo->special.arg[2] & 4)
				mo->tracer->pitch = pp;
			mo->tracer->momx = mo->momx;
			mo->tracer->momy = mo->momy;
			mo->tracer->momz = mo->momz;
			mo->tracer->iflags |= MFI_FOLLOW_MOVE;
		} else
		{
			mo->tracer->z = oldz;
			mo->tracer->iflags &= ~MFI_FOLLOW_MOVE;
			// pause movement
			return;
		}
	}

	P_UnsetThingPosition(mo);
	mo->x = xx;
	mo->y = yy;
	mo->z = zz;
	P_SetThingPosition(mo);
	mo->floorz = mo->subsector->sector->floorheight;
	mo->ceilingz = mo->subsector->sector->ceilingheight;

	if(mo->special.arg[2] & 2)
		mo->angle = aa;

	if(mo->special.arg[2] & 4)
		mo->pitch = pp;

	// step
	mo->threshold--;
	if(mo->threshold)
		return;

	// next point delay
	mo->reactiontime = ((uint32_t)mo->target->special.arg[2] * 35) / 8;
	mo->reactiontime++;

	return;

stop_moving:
	mo->iflags &= ~MFI_FOLLOW_PATH;
	if(mo->tracer)
		mo->tracer->iflags &= ~MFI_FOLLOW_MOVE;
}

