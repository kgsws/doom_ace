// kgsws' ACE Engine
////
// New hitscan handling.
// P_PathTraverse bugfix.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "player.h"
#include "mobj.h"
#include "map.h"

#define PIT_AddThingIntercepts	((void*)0x0002C720 + doom_code_segment)

static uint32_t *validcount;

static intercept_t *intercepts;
static intercept_t **d_intercept_p;
static divline_t *trace;

static intercept_t *intercept_p;

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
	if(	trace->dx > FRACUNIT*16 ||
		trace->dy > FRACUNIT*16 ||
		trace->dx < -FRACUNIT*16 ||
		trace->dy < -FRACUNIT*16
	) {
		if(	P_PointOnDivlineSide(v1->x, v1->y, trace) ==
			P_PointOnDivlineSide(v2->x, v2->y, trace)
		)
			return 1;
	} else
	{
		if(	check_divline_side(v1->x, v1->y, trace) ==
			check_divline_side(v2->x, v2->y, trace)
		)
			return 1;
	}

	return 0;
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

	frac = P_InterceptVector(trace, &dl);

	if(frac < 0)
		return 1;

	if(frac > FRACUNIT)
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

	if(intercept_p >= intercepts + MAXINTERCEPTS)
		return 0;

	if(trace->x < mo->x)
	{
		if(trace->y < mo->y)
		{
			v1.x = mo->x - mo->radius;
			v1.y = mo->y + mo->radius;
			v2.x = mo->x - mo->radius;
			v2.y = mo->y - mo->radius;
			v3.x = mo->x + mo->radius;
			v3.y = mo->y - mo->radius;
		} else
		{
			v1.x = mo->x - mo->radius;
			v1.y = mo->y - mo->radius;
			v2.x = mo->x - mo->radius;
			v2.y = mo->y + mo->radius;
			v3.x = mo->x + mo->radius;
			v3.y = mo->y + mo->radius;
		}
	} else
	{
		if(trace->y < mo->y)
		{
			v1.x = mo->x + mo->radius;
			v1.y = mo->y + mo->radius;
			v2.x = mo->x + mo->radius;
			v2.y = mo->y - mo->radius;
			v3.x = mo->x - mo->radius;
			v3.y = mo->y - mo->radius;
		} else
		{
			v1.x = mo->x + mo->radius;
			v1.y = mo->y - mo->radius;
			v2.x = mo->x + mo->radius;
			v2.y = mo->y + mo->radius;
			v3.x = mo->x - mo->radius;
			v3.y = mo->y + mo->radius;
		}
	}

	if(!check_trace_line(&v1, &v2))
	{
		dl.x = v1.x;
		dl.y = v1.y;
		dl.dx = v2.x - v1.x;
		dl.dy = v2.y - v1.y;

		tmpf = P_InterceptVector(trace, &dl);
		if(tmpf >= 0)
			frac = tmpf;
	}

	if(!check_trace_line(&v2, &v3))
	{
		dl.x = v2.x;
		dl.y = v2.y;
		dl.dx = v3.x - v2.x;
		dl.dy = v3.y - v2.y;

		tmpf = P_InterceptVector(trace, &dl);
		if(tmpf >= 0 && tmpf < frac)
			frac = tmpf;
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
	if(flags & PT_ADDLINES)
		P_BlockLinesIterator(x, y, add_line_intercepts);
	if(flags & PT_ADDTHINGS)
		P_BlockThingsIterator(x, y, add_thing_intercepts);
}

//
// API

uint32_t path_traverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, uint32_t (*trav)(intercept_t*), uint32_t flags)
{
	// TODO: solve intercept overflow
	int32_t dx, dy;
	int32_t ia, ib, ic;

	*validcount = *validcount + 1;
	intercept_p = intercepts;

	trace->x = x1;
	trace->y = y1;
	trace->dx = x2 - x1;
	trace->dy = y2 - y1;

	x1 -= *bmaporgx;
	y1 -= *bmaporgy;
	x2 -= *bmaporgx;
	y2 -= *bmaporgy;

	x1 >>= MAPBLOCKSHIFT;
	y1 >>= MAPBLOCKSHIFT;
	x2 >>= MAPBLOCKSHIFT;
	y2 >>= MAPBLOCKSHIFT;

	dx = x2 - x1;
	dy = y2 - y1;

	ia = dx < 0 ? -1 : 1;
	ib = dy < 0 ? -1 : 1;

	add_intercepts(x1, y1, flags);
	add_intercepts(x1 + ia, y1, flags);
	add_intercepts(x1, y1 + ib, flags);

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

	*d_intercept_p = intercept_p;
	return P_TraverseIntercepts(trav, FRACUNIT);
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x00013580, DATA_HOOK | HOOK_IMPORT, (uint32_t)&validcount},
	{0x0002BA10, DATA_HOOK | HOOK_IMPORT, (uint32_t)&trace},
	{0x0002BA20, DATA_HOOK | HOOK_IMPORT, (uint32_t)&intercepts},
	{0x0002C028, DATA_HOOK | HOOK_IMPORT, (uint32_t)&d_intercept_p},
};

