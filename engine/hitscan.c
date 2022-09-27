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

#define PIT_AddLineIntercepts	((void*)0x0002C5F0 + doom_code_segment)
#define PIT_AddThingIntercepts	((void*)0x0002C720 + doom_code_segment)

static uint32_t *validcount;

static intercept_t *intercepts;
static intercept_t **intercept_p;

static divline_t *trace;

//
// DEBUG

static void debug_box(fixed_t x, fixed_t y, uint32_t type)
{
	mobj_t *mo;

	x <<= MAPBLOCKSHIFT;
	y <<= MAPBLOCKSHIFT;

	x += *bmaporgx;
	y += *bmaporgy;

	x += (MAPBLOCKUNITS / 2) * FRACUNIT;
	y += (MAPBLOCKUNITS / 2) * FRACUNIT;

	mo = P_SpawnMobj(x, y, -0x7FFFFFFF, type);
	if(mo->subsector)
		mo->z = mo->subsector->sector->floorheight;
}

//
// functions

static void add_intercepts(int32_t x, int32_t y, uint32_t flags)
{
	if(flags & PT_ADDLINES)
		P_BlockLinesIterator(x, y, PIT_AddLineIntercepts);
	if(flags & PT_ADDTHINGS)
		P_BlockThingsIterator(x, y, PIT_AddThingIntercepts);
}

//
// API

__attribute((regparm(2),no_caller_saved_registers))
uint32_t line_side_check(line_t *l0, line_t *l1)
{
	// this fixes most of elastic collisions
	fixed_t x, y;
	fixed_t dx;
	fixed_t dy;
	fixed_t left;
	fixed_t right;

	if(l0)
	{
		x = l0->v1->x;
		y = l0->v1->y;
	} else
	{
		x = l1->v2->x;
		y = l1->v2->y;
	}

	if(!trace->dx)
	{
		if(x <= trace->x)
			return trace->dy > 0;
		return trace->dy < 0;
	}

	if(!trace->dy)
	{
		if(y <= trace->y)
			return trace->dx < 0;
		return trace->dx > 0;
	}

	dx = (x - trace->x);
	dy = (y - trace->y);

	if((trace->dy ^ trace->dx ^ dx ^ dy) & 0x80000000)
	{
		if((trace->dy ^ dx) & 0x80000000)
			return 1;
		return 0;
	}

	left = FixedMul(trace->dy, dx);
	right = FixedMul(dy, trace->dx);

	if(right < left)
		return 0;
	return 1;
}

uint32_t path_traverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, uint32_t (*trav)(intercept_t*), uint32_t flags)
{
	// TODO: solve intercept overflow
	int32_t dx, dy;
	int32_t ia, ib, ic;

	*validcount = *validcount + 1;
	*intercept_p = intercepts;

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
	{0x0002C028, DATA_HOOK | HOOK_IMPORT, (uint32_t)&intercept_p},
};

