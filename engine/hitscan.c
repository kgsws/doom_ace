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

static uint32_t *validcount;

static intercept_t *intercepts;
static intercept_t **intercept_p;

static divline_t *trace;

static intercept_t *in_ptr;

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
/*	*validcount = *validcount + 1;
	in_ptr = intercepts;

	trace->x = x1;
	trace->y = y1;
	trace->dx = x2 - x1;
	trace->dy = y2 - y1;
*/
	return 0;
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

