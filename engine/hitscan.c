// kgsws' ACE Engine
////
// New hitscan handling.
// P_PathTraverse bugfix.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "decorate.h"
#include "player.h"
#include "mobj.h"
#include "map.h"
#include "extra3d.h"
#include "special.h"
#include "hitscan.h"

#define PIT_AddThingIntercepts	((void*)0x0002C720 + doom_code_segment)

static uint32_t thing_slide_slope;

static fixed_t hitscanz;
static sector_t *hitscansector;

//
// functions

uint32_t check_divline_side(fixed_t x, fixed_t y, divline_t *line)
{
	fixed_t dx;
	fixed_t dy;
	fixed_t left;
	fixed_t right;

	if(!line->dx)
	{
		if(x <= line->x)
			return line->dy > 0;
		return line->dy < 0;
	}

	if(!line->dy)
	{
		if(y <= line->y)
			return line->dx < 0;
		return line->dx > 0;
	}

	dx = (x - line->x);
	dy = (y - line->y);

	if((line->dy ^ line->dx ^ dx ^ dy) & 0x80000000)
	{
		if((line->dy ^ dx) & 0x80000000 )
			return 1;
		return 0;
	}

	return FixedMul(dy, line->dx) >= FixedMul(line->dy, dx);
}

uint32_t check_trace_line(vertex_t *v1, vertex_t *v2)
{
	if(	trace.dx > FRACUNIT*16 ||
		trace.dy > FRACUNIT*16 ||
		trace.dx < -FRACUNIT*16 ||
		trace.dy < -FRACUNIT*16
	) {
		if(	P_PointOnDivlineSide(v1->x, v1->y, &trace) ==
			P_PointOnDivlineSide(v2->x, v2->y, &trace)
		)
			return 1;
	} else
	{
		if(	check_divline_side(v1->x, v1->y, &trace) ==
			check_divline_side(v2->x, v2->y, &trace)
		)
			return 1;
	}

	return 0;
}

fixed_t hs_intercept_vector(divline_t *v2, divline_t *v1)
{
	// this version should have less issues with overflow
	union
	{
		int64_t w;
		struct
		{
			uint32_t a, b;
		};
	} num;
	fixed_t den;
	fixed_t frac;
	fixed_t v1m2x = (v1->x - v2->x) >> 8;
	fixed_t v2m1y = (v2->y - v1->y) >> 8;
	fixed_t v1dx = v1->dx >> 8;
	fixed_t v1dy = v1->dy >> 8;
	fixed_t v2dx = v2->dx >> 8;
	fixed_t v2dy = v2->dy >> 8;

	den = ((int64_t)v1dy * (int64_t)v2dx - (int64_t)v1dx * (int64_t)v2dy) >> 16;
	if(den >= -1 && den <= 1)
		return FRACUNIT;

	num.w = (int64_t)v1m2x * (int64_t)v1dy + (int64_t)v2m1y * (int64_t)v1dx;

//	return num.w / den; // this should work, but GCC wants to use '__divdi3'

	asm(	"idiv %%ecx"
		: "=a" (frac)
		: "a" (num.a), "d" (num.b), "c" (den)
		: "cc");

	return frac;
}

//
// intercepts

__attribute((regparm(2),no_caller_saved_registers))
static uint32_t add_line_intercepts(line_t *li)
{
	fixed_t frac;
	divline_t dl;

	if(intercept_p >= intercepts + MAXINTERCEPTS)
		return 0;

	if(check_trace_line(li->v1, li->v2))
		return 1;

	dl.x = li->v1->x;
	dl.y = li->v1->y;
	dl.dx = li->dx;
	dl.dy = li->dy;

	frac = hs_intercept_vector(&trace, &dl);

	if(frac < 0)
		return 1;

	if(frac > FRACUNIT)
		return 1;

	if(	!frac &&
		P_PointOnLineSide(trace.x, trace.y, li) ==
		P_PointOnLineSide(trace.x + trace.dx, trace.y + trace.dy, li)
	)
		return 1;

	intercept_p->frac = frac;
	intercept_p->isaline = 1;
	intercept_p->d.line = li;
	intercept_p++;

	return intercept_p < intercepts + MAXINTERCEPTS;
}

__attribute((regparm(2),no_caller_saved_registers))
static uint32_t add_thing_intercepts(mobj_t *mo)
{
	vertex_t v1, v2, v3;
	divline_t dl;
	fixed_t tmpf;
	fixed_t frac = FRACUNIT * 2;
	uint8_t side[2];

	if(mo->validcount == validcount)
		return 1;

	mo->validcount = validcount;

	if(intercept_p >= intercepts + MAXINTERCEPTS)
		return 0;

	if(trace.x < mo->x)
	{
		side[0] = INTH_SIDE_LEFT;
		if(trace.y < mo->y)
		{
			v1.x = mo->x - mo->radius;
			v1.y = mo->y + mo->radius;
			v2.x = mo->x - mo->radius;
			v2.y = mo->y - mo->radius;
			v3.x = mo->x + mo->radius;
			v3.y = mo->y - mo->radius;
			side[1] = INTH_SIDE_BOT;
		} else
		{
			v1.x = mo->x - mo->radius;
			v1.y = mo->y - mo->radius;
			v2.x = mo->x - mo->radius;
			v2.y = mo->y + mo->radius;
			v3.x = mo->x + mo->radius;
			v3.y = mo->y + mo->radius;
			side[1] = INTH_SIDE_TOP;
		}
	} else
	{
		side[0] = INTH_SIDE_RIGHT;
		if(trace.y < mo->y)
		{
			v1.x = mo->x + mo->radius;
			v1.y = mo->y + mo->radius;
			v2.x = mo->x + mo->radius;
			v2.y = mo->y - mo->radius;
			v3.x = mo->x - mo->radius;
			v3.y = mo->y - mo->radius;
			side[1] = INTH_SIDE_BOT;
		} else
		{
			v1.x = mo->x + mo->radius;
			v1.y = mo->y - mo->radius;
			v2.x = mo->x + mo->radius;
			v2.y = mo->y + mo->radius;
			v3.x = mo->x - mo->radius;
			v3.y = mo->y + mo->radius;
			side[1] = INTH_SIDE_TOP;
		}
	}

	if(!check_trace_line(&v1, &v2))
	{
		dl.x = v1.x;
		dl.y = v1.y;
		dl.dx = v2.x - v1.x;
		dl.dy = v2.y - v1.y;

		tmpf = hs_intercept_vector(&trace, &dl);
		if(tmpf >= 0)
		{
			frac = tmpf;
			mo->intercept_side = side[0];
		}
	}

	if(!check_trace_line(&v2, &v3))
	{
		dl.x = v2.x;
		dl.y = v2.y;
		dl.dx = v3.x - v2.x;
		dl.dy = v3.y - v2.y;

		tmpf = hs_intercept_vector(&trace, &dl);
		if(tmpf >= 0 && tmpf < frac)
		{
			frac = tmpf;
			mo->intercept_side = side[1];
		}
	}

	if(frac > FRACUNIT)
		return 1;

	intercept_p->frac = frac;
	intercept_p->isaline = 0;
	intercept_p->d.thing = mo;
	intercept_p++;

	return intercept_p < intercepts + MAXINTERCEPTS;
}

static void add_intercepts(int32_t x, int32_t y, uint32_t flags)
{
	if(intercept_p >= intercepts + MAXINTERCEPTS)
		return;
	if(flags & PT_ADDLINES)
		P_BlockLinesIterator(x, y, add_line_intercepts);
	if(flags & PT_ADDTHINGS)
		P_BlockThingsIterator(x, y, add_thing_intercepts);
}

//
// API

uint32_t path_traverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, uint32_t (*trav)(intercept_t*), uint32_t flags)
{
	int32_t dx, dy;
	int32_t ia, ib, ic;

	if(shootthing && shootthing->subsector)
	{
		// hack for hitscan; this should be in P_LineAttack
		hitscanz = shootz;
		hitscansector = shootthing->subsector->sector;
	}

	validcount++;
	intercept_p = intercepts;

	trace.x = x1;
	trace.y = y1;
	trace.dx = x2 - x1;
	trace.dy = y2 - y1;

	x1 -= bmaporgx;
	y1 -= bmaporgy;
	x2 -= bmaporgx;
	y2 -= bmaporgy;

	x1 >>= MAPBLOCKSHIFT;
	y1 >>= MAPBLOCKSHIFT;
	x2 >>= MAPBLOCKSHIFT;
	y2 >>= MAPBLOCKSHIFT;

	dx = x2 - x1;
	dy = y2 - y1;

	ia = dx < 0 ? -1 : 1;
	ib = dy < 0 ? -1 : 1;

	add_intercepts(x1, y1, flags);
	add_intercepts(x1 + 1, y1, flags);
	add_intercepts(x1, y1 + 1, flags);
	add_intercepts(x1 - 1, y1, flags);
	add_intercepts(x1, y1 - 1, flags);
	add_intercepts(x1 + 1, y1 + 1, flags);
	add_intercepts(x1 - 1, y1 - 1, flags);
	add_intercepts(x1 - 1, y1 + 1, flags);
	add_intercepts(x1 + 1, y1 - 1, flags);

	dx = abs(dx);
	dy = abs(dy);

	if(dx >= dy)
	{
		ic = dx;
		for(int32_t i = dx; i > 0; i--)
		{
			x1 += ia;
			ic -= 2 * dy;
			if(ic <= 0)
			{
				y1 += ib;
				ic += 2 * dx;
			}
			add_intercepts(x1, y1, flags);
			add_intercepts(x1, y1 - 1, flags);
			add_intercepts(x1, y1 + 1, flags);
		}
	} else
	{
		ic = dy;
		for(int32_t i = dy; i > 0; i--)
		{
			y1 += ib;
			ic -= 2 * dx;
			if(ic <= 0)
			{
				x1 += ia;
				ic += 2 * dy;
			}
			add_intercepts(x1, y1, flags);
			add_intercepts(x1 - 1, y1, flags);
			add_intercepts(x1 + 1, y1, flags);
		}
	}

	dx = P_TraverseIntercepts(trav, FRACUNIT);

	shootthing = NULL; // again, a hack

	return dx;
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t hs_slide_traverse(intercept_t *in)
{
	line_t *li;
	sector_t *sec;
	fixed_t z;

	if(!in->isaline)
	{
		mobj_t *th = in->d.thing;

		if(th == slidemo)
			return 1;

		if(!(th->flags & MF_SOLID))
			return 1;

		if(in->frac < bestslidefrac)
		{
			bestslidefrac = in->frac;
			bestslideline = (void*)&thing_slide_slope - offsetof(line_t, slopetype);
			switch(th->intercept_side)
			{
				case INTH_SIDE_LEFT:
				case INTH_SIDE_RIGHT:
					thing_slide_slope = ST_VERTICAL;
				break;
				case INTH_SIDE_TOP:
				case INTH_SIDE_BOT:
					thing_slide_slope = ST_HORIZONTAL;
				break;
			}
		}

		return 0;
	}

	li = in->d.line;

	if(!(li->flags & ML_TWOSIDED))
	{
		if(P_PointOnLineSide(slidemo->x, slidemo->y, li))
			return 1;
		goto isblocking;
	}

	if(li->flags & (ML_BLOCKING | ML_BLOCK_ALL))
		goto isblocking;

	if(slidemo->player)
	{
		if(li->flags & ML_BLOCK_PLAYER)
			goto isblocking;
	} else
	{
		if(li->flags & ML_BLOCKMONSTERS)
			goto isblocking;
	}

	P_LineOpening(li);

	tmfloorz = openbottom;
	tmceilingz = opentop;

	if(P_PointOnLineSide(slidemo->x, slidemo->y, li))
		sec = li->frontsector;
	else
		sec = li->backsector;

	e3d_check_heights(slidemo, sec, slidemo->flags & MF_MISSILE);

	if(tmextraceiling - tmextrafloor < slidemo->height)
		goto isblocking;

	if(opentop > tmextraceiling)
		opentop = tmextraceiling;

	if(openbottom < tmextrafloor)
		openbottom = tmextrafloor;

	if(slidemo->z < openbottom)
		z = openbottom;
	else
		z = slidemo->z;

	e3d_check_midtex(slidemo, li, slidemo->flags & MF_MISSILE);

	if(tmextraceiling - tmextrafloor < slidemo->height)
		goto isblocking;

	if(opentop > tmextraceiling)
		opentop = tmextraceiling;

	if(openbottom < tmextrafloor)
		openbottom = tmextrafloor;

	if(e3d_check_inside(sec, z, E3D_SOLID))
		goto isblocking;

	if(openrange < slidemo->height)
		goto isblocking;

	if(opentop - slidemo->z < slidemo->height)
		goto isblocking;

	if(openbottom - slidemo->z > slidemo->info->step_height)
		goto isblocking;

	return 1;

isblocking:
	if(in->frac < bestslidefrac)
	{
		bestslidefrac = in->frac;
		bestslideline = li;
	}

	return 0;
}

static fixed_t check_e3d_hit(sector_t *sec, fixed_t frac, fixed_t *zz)
{
	fixed_t dz, z;

	dz = FixedMul(aimslope, FixedMul(frac, attackrange));
	z = shootz + dz;

	if(aimslope < 0)
	{
		extraplane_t *pl = sec->exfloor;
		while(pl)
		{
			if(pl->flags & E3D_BLOCK_HITSCAN && hitscanz > *pl->height && z < *pl->height)
			{
				frac = -FixedDiv(FixedMul(frac, shootz - *pl->height), dz);
				*zz = *pl->height;
				return frac;
			}
			pl = pl->next;
		}
	} else
	if(aimslope > 0)
	{
		extraplane_t *pl = sec->exceiling;
		while(pl)
		{
			if(pl->flags & E3D_BLOCK_HITSCAN && hitscanz < *pl->height && z > *pl->height)
			{
				frac = -FixedDiv(FixedMul(frac, shootz - *pl->height), dz);
				*zz = *pl->height;
				return frac;
			}
			pl = pl->next;
		}
	}

	return -1;
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t hs_shoot_traverse(intercept_t *in)
{
	fixed_t	x;
	fixed_t y;
	fixed_t z;
	fixed_t dist;

	if(in->isaline)
	{
		line_t *li = in->d.line;
		fixed_t frac, dz, z;
		sector_t *frontsector;
		sector_t *backsector;
		uint_fast8_t activate = map_format != MAP_FORMAT_DOOM;
		uint_fast8_t in_sky = 0;

		if(li->special && !activate)
			P_ShootSpecialLine(shootthing, li);

		if(!(li->flags & ML_TWOSIDED) || li->flags & ML_BLOCK_ALL)
			goto hitline;

		P_LineOpening(li);

		dist = FixedMul(attackrange, in->frac);

		if(li->frontsector->floorheight != li->backsector->floorheight)
		{
			if(FixedDiv(openbottom - shootz, dist) > aimslope)
				goto hitline;
		}

		if(li->frontsector->ceilingheight != li->backsector->ceilingheight)
		{
			if(FixedDiv(opentop - shootz, dist) < aimslope)
				goto hitline;
		}

		if(	activate &&
			li->special &&
			(li->flags & ML_ACT_MASK) == MLA_ATK_HIT &&
			FixedDiv(opentop - shootz, dist) >= aimslope &&
			FixedDiv(openbottom - shootz, dist) <= aimslope
		)
			spec_activate(li, shootthing, SPEC_ACT_SHOOT);

		if(P_PointOnLineSide(trace.x, trace.y, li))
		{
			backsector = li->frontsector;
			frontsector = li->backsector;
		} else
		{
			frontsector = li->frontsector;
			backsector = li->backsector;
		}

		hitscansector = backsector;

		if(!li->frontsector->exfloor && !li->backsector->exfloor)
			return 1;

		if(frontsector->exfloor)
		{
			frac = check_e3d_hit(frontsector, in->frac, &z);
			if(frac >= 0)
				goto do_puff;
		}

		if(frontsector->tag == backsector->tag)
			return 1;

		if(!backsector->exfloor)
			return 1;

		dz = FixedMul(aimslope, FixedMul(in->frac, attackrange));
		z = shootz + dz;

		if(e3d_check_inside(backsector, z, E3D_BLOCK_HITSCAN))
		{
			frac = in->frac - FixedDiv(4 * FRACUNIT, attackrange);
			dz = FixedMul(aimslope, FixedMul(frac, attackrange));
			z = shootz + dz;
			goto do_puff;
		}

		hitscanz = z;

		return 1;

hitline:

		if(P_PointOnLineSide(trace.x, trace.y, li))
		{
			backsector = li->frontsector;
			frontsector = li->backsector;
		} else
		{
			frontsector = li->frontsector;
			backsector = li->backsector;
		}

		if(!frontsector)
			// noclip?
			return 1;

		frac = in->frac - FixedDiv(4 * FRACUNIT, attackrange);
		dz = FixedMul(aimslope, FixedMul(frac, attackrange));
		z = shootz + dz;

		if(!(mobjinfo[mo_puff_type].flags1 & MF1_SKYEXPLODE))
		{
			// TODO: floor sky hack
			if(frontsector->ceilingpic == skyflatnum)
			{
				if(z > frontsector->ceilingheight)
					in_sky = 1;
				else
				if(backsector && backsector->ceilingpic == skyflatnum && backsector->ceilingheight < z)
					in_sky = 1;
			}
		}

		if(aimslope < 0)
		{
			if(z < frontsector->floorheight)
			{
				frac = -FixedDiv(FixedMul(frac, shootz - frontsector->floorheight), dz);
				z = frontsector->floorheight;
				activate = 0;
			}
		} else
		if(aimslope > 0)
		{
			if(z > frontsector->ceilingheight)
			{
				frac = FixedDiv(FixedMul(frac, frontsector->ceilingheight - shootz), dz);
				z = frontsector->ceilingheight;
				activate = 0;
			}
		}

		if(frontsector->exfloor)
		{
			fixed_t tf, tz;
			tf = check_e3d_hit(frontsector, in->frac, &tz);
			if(tf >= 0)
			{
				frac = tf;
				z = tz;
			}
		}

		if(in_sky)
			return 0;

do_puff:
		trace.x = trace.x + FixedMul(trace.dx, frac);
		trace.y = trace.y + FixedMul(trace.dy, frac);
		trace.dx = z;

		mobj_spawn_puff(&trace, NULL);

		if(activate && li->special)
			spec_activate(li, shootthing, SPEC_ACT_SHOOT);

		return 0;
	} else
	{
		mobj_t *th = in->d.thing;
		fixed_t thingtopslope;
		fixed_t thingbottomslope;

		if(th == shootthing)
			return 1;

		if(!(th->flags & MF_SHOOTABLE))
			return 1;

		if(th->flags1 & MF1_SPECTRAL && !(mobjinfo[mo_puff_type].flags1 & MF1_SPECTRAL))
			return 1;

		if(th->flags1 & MF1_GHOST && mobjinfo[mo_puff_type].flags1 & MF1_THRUGHOST)
			return 1;

		if(hitscansector)
		{
			fixed_t z, frac;
			frac = check_e3d_hit(hitscansector, in->frac, &z);
			if(frac >= 0)
			{
				trace.x = trace.x + FixedMul(trace.dx, frac);
				trace.y = trace.y + FixedMul(trace.dy, frac);
				trace.dx = z;
				mobj_spawn_puff(&trace, NULL);
				return 0;
			}
		}

		dist = FixedMul(attackrange, in->frac);
		thingtopslope = FixedDiv(th->z + th->height - shootz, dist);

		if(thingtopslope < aimslope)
			return 1;

		thingbottomslope = FixedDiv(th->z - shootz, dist);

		if(thingbottomslope > aimslope)
			return 1;

		trace.x = trace.x + FixedMul(trace.dx, in->frac);
		trace.y = trace.y + FixedMul(trace.dy, in->frac);
		trace.dx = shootz + FixedMul(aimslope, FixedMul(in->frac, attackrange));

		mobj_spawn_puff(&trace, th);
		mobj_spawn_blood(&trace, th, la_damage);

		if(la_damage)
			mobj_damage(th, shootthing, shootthing, la_damage, mobjinfo + mo_puff_type);

		return 0;
	}
}

