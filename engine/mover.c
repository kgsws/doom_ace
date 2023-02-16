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

	if(mo->iflags & MFI_FOLLOW_INIT)
	{
		// interpolation check
		if(!(mo->special.arg[2] & 1))
			engine_error("MOVER", "Invalid interpolation!");

		// instantly move to first point
		mo->iflags &= ~MFI_FOLLOW_INIT;

		next_tid = mo->special.arg[0];
		next_tid += (uint32_t)mo->special.arg[1] * 256;

		th = mobj_by_tid_first(next_tid);
		if(!th)
			goto stop_moving;
		if(th->spawnpoint.type != 9070) // interpolation point
			goto stop_moving;

		mo->tracer = th;

		P_UnsetThingPosition(mo);
		mo->x = th->x;
		mo->y = th->y;
		mo->z = th->z;
		mo->angle = th->angle;
		P_SetThingPosition(mo);

		// setup point
		goto next_point;
	}

	if(mo->reactiontime)
	{
		// delay
		mo->reactiontime--;
		return;
	}

	// change position

	P_UnsetThingPosition(mo);

	mo->x += mo->mover.x;
	mo->y += mo->mover.y;
	mo->z += mo->mover.z;

	P_SetThingPosition(mo);

	if(mo->special.arg[2] & 2)
		mo->angle += mo->lastlook;

	// step
	mo->threshold--;
	if(mo->threshold)
		return;

next_point:

	// check
	if(!mo->tracer->special.arg[1])
		goto stop_moving;

	// move time
	mo->threshold = ((uint32_t)mo->tracer->special.arg[1] * 35) / 8;

	// delay
	mo->reactiontime = ((uint32_t)mo->tracer->special.arg[2] * 35) / 8;

	// get new point
	next_tid = mo->tracer->special.arg[3];
	next_tid += (uint32_t)mo->tracer->special.arg[4] * 256;

	// find new point
	th = mobj_by_tid_first(next_tid);
	if(!th)
		goto stop_moving;
	if(th->spawnpoint.type != 9070) // interpolation point
		goto stop_moving;

	mo->tracer = th;

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

	return;

stop_moving:
	mo->iflags &= ~MFI_FOLLOW_PATH;
}

