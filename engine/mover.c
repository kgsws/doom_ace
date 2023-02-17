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
	angle_t aa;

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

		if(mo->tracer)
		{
			fixed_t oldz = mo->tracer->z;
			mo->tracer->z = mo->z;
			if(	!(mo->tracer->flags & MF_SOLID) ||
				P_CheckPosition(mo->tracer, mo->x, mo->y)
			){
				P_UnsetThingPosition(mo->tracer);
				mo->tracer->x = mo->x;
				mo->tracer->y = mo->y;
				P_SetThingPosition(mo->tracer);
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
			// set angle
			mo->angle = mo->target->angle;

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
			mo->mover.x = (th->x - mo->x) / mo->threshold;
			mo->mover.y = (th->y - mo->y) / mo->threshold;
			mo->mover.z = (th->z - mo->z) / mo->threshold;

			if(mo->special.arg[2] & 2)
			{
				if(th->angle != mo->angle)
				{
					angle_t angle = th->angle - mo->angle;
					if(angle > 0x80000000)
					{
						angle = -angle;
						mo->lastlook = angle / mo->threshold;
						mo->lastlook = -mo->lastlook;
					} else
						mo->lastlook = angle / mo->threshold;
				} else
					mo->lastlook = 0;
			}
		} else
		if(mo->tracer)
			mo->tracer->iflags &= ~MFI_FOLLOW_MOVE;
		return;
	}

	// change position

	xx = mo->x + mo->mover.x;
	yy = mo->y + mo->mover.y;
	zz = mo->z + mo->mover.z;
	if(mo->special.arg[2] & 2)
		aa = mo->angle + mo->lastlook;

	if(mo->tracer)
	{
		fixed_t oldz = mo->tracer->z;
		mo->tracer->z = zz;
		if(	!(mo->tracer->flags & MF_SOLID) ||
			P_CheckPosition(mo->tracer, xx, yy)
		){
			P_UnsetThingPosition(mo->tracer);
			mo->tracer->x = xx;
			mo->tracer->y = yy;
			P_SetThingPosition(mo->tracer);
			if(mo->special.arg[2] & 2)
				mo->tracer->angle = aa;
			mo->tracer->momx = 0;
			mo->tracer->momy = 0;
			mo->tracer->momz = 0;
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

	if(mo->special.arg[2] & 2)
		mo->angle = aa;

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

