// kgsws' ACE Engine
////
// New sight check code, based on Hexen.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "map.h"
#include "hitscan.h"
#include "extra3d.h"
#include "render.h"
#include "polyobj.h"
#include "sight.h"

static fixed_t sightzstart;

extraplane_t *topplane;
extraplane_t *botplane;

//
// funcs

static inline void P_MakeDivline(line_t *li, divline_t *dl)
{
	dl->x = li->v1->x;
	dl->y = li->v1->y;
	dl->dx = li->dx;
	dl->dy = li->dy;
}

uint32_t sight_extra_3d_cover(sector_t *sec)
{
	// simple extra floor rejection; only rejects fully covered openings
	extraplane_t *pl;

	pl = sec->exfloor;
	while(pl)
	{
		if(	pl->flags & E3D_BLOCK_SIGHT &&
			pl->source->ceilingheight >= opentop &&
			pl->source->floorheight <= openbottom
		)
			return 1;
		pl = pl->next;
	}

	return 0;
}

static uint32_t PTR_SightTraverse(intercept_t *in)
{
	line_t *li;
	fixed_t slope;
	sector_t *back;
	extraplane_t *pl;
	uint32_t force;

	li = in->d.line;

	P_LineOpening(li);

	if(topplane && *topplane->height < opentop)
		opentop = *topplane->height;

	if(botplane && *botplane->height > openbottom)
		openbottom = *botplane->height;

	if(openbottom >= opentop)
		return 0;

	topplane = NULL;
	botplane = NULL;
	if(in->isaline)
		back = li->frontsector;
	else
		back = li->backsector;

	pl = back->exfloor;
	while(pl)
	{
		if(	pl->flags & E3D_BLOCK_SIGHT &&
			pl->source->ceilingheight < sightzstart &&
			pl->source->ceilingheight >= openbottom &&
			pl->source->floorheight <= openbottom
		){
			openbottom = pl->source->ceilingheight;
			botplane = pl;
			break;
		}
		pl = pl->next;
	}

	pl = back->exceiling;
	while(pl)
	{
		if(	pl->flags & E3D_BLOCK_SIGHT &&
			pl->source->floorheight > sightzstart &&
			pl->source->floorheight <= opentop &&
			pl->source->ceilingheight >= opentop
		){
			opentop = pl->source->floorheight;
			topplane = pl;
			break;
		}
		pl = pl->next;
	}

	force = li->frontsector->tag != li->backsector->tag && (li->frontsector->exfloor || li->backsector->exfloor);

	if(force || li->frontsector->floorheight != li->backsector->floorheight)
	{
		slope = FixedDiv(openbottom - sightzstart, in->frac);
		if(slope > botslope)
			botslope = slope;
	}

	if(force || li->frontsector->ceilingheight != li->backsector->ceilingheight)
	{
		slope = FixedDiv(opentop - sightzstart, in->frac);
		if(slope < topslope)
			topslope = slope;
	}

	if(topslope <= botslope)
		return 0;

	return 1;
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t P_SightLineCheck(line_t *ld)
{
	int32_t s1, s2;
	divline_t dl;

	s1 = P_PointOnDivlineSide(ld->v1->x, ld->v1->y, &trace);
	s2 = P_PointOnDivlineSide(ld->v2->x, ld->v2->y, &trace);
	if(s1 == s2)
		return 1;

	P_MakeDivline(ld, &dl);
	s1 = P_PointOnDivlineSide(trace.x, trace.y, &dl);
	s2 = P_PointOnDivlineSide(trace.x+trace.dx, trace.y+trace.dy, &dl);
	if(s1 == s2)
		return 1;

	if(!ld->backsector)
		return 0;

	if(ld->flags & ML_BLOCK_ALL)
		return 0;

	if(ld->frontsector->exfloor || ld->backsector->exfloor)
		P_LineOpening(ld);

	if(sight_extra_3d_cover(ld->frontsector))
		return 0;

	if(sight_extra_3d_cover(ld->backsector))
		return 0;

	if(intercept_p < intercepts + MAXINTERCEPTS)
	{
		intercept_p->d.line = ld;
		intercept_p->isaline = s1; // store line side
		intercept_p++;
	}

	return 1;
}

static uint32_t P_SightTraverseIntercepts()
{
	int32_t count;
	fixed_t dist;
	intercept_t *scan, *in;
	divline_t dl;

	count = intercept_p - intercepts;
	for(scan = intercepts; scan < intercept_p; scan++)
	{
		P_MakeDivline(scan->d.line, &dl);
		scan->frac = hs_intercept_vector(&trace, &dl);
	}

	while(count--)
	{
		dist = FRACUNIT * 2;
		for(scan = intercepts; scan < intercept_p; scan++)
			if(scan->frac < dist)
			{
				dist = scan->frac;
				in = scan;
			}

		if(!PTR_SightTraverse(in))
			return 0;

		in->frac = FRACUNIT * 2;
	}

	return 1;
}

static uint32_t P_SightPathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
	fixed_t xt1, yt1, xt2, yt2;
	fixed_t xstep, ystep;
	fixed_t partialx, partialy;
	fixed_t xintercept, yintercept;
	int32_t mapx, mapy, mapxstep, mapystep;
	int32_t count;

	validcount++;
	intercept_p = intercepts;

	if(((x1 - bmaporgx) & (MAPBLOCKSIZE - 1)) == 0)
		x1 += FRACUNIT;
	if(((y1 - bmaporgy) & (MAPBLOCKSIZE - 1)) == 0)
		y1 += FRACUNIT;

	trace.x = x1;
	trace.y = y1;
	trace.dx = x2 - x1;
	trace.dy = y2 - y1;

	x1 -= bmaporgx;
	y1 -= bmaporgy;
	xt1 = x1 >> MAPBLOCKSHIFT;
	yt1 = y1 >> MAPBLOCKSHIFT;

	x2 -= bmaporgx;
	y2 -= bmaporgy;
	xt2 = x2 >> MAPBLOCKSHIFT;
	yt2 = y2 >> MAPBLOCKSHIFT;

	if(xt1 < 0 || yt1 < 0 || xt1 >= bmapwidth || yt1 >= bmapheight || xt2 < 0 || yt2 < 0 || xt2 >= bmapwidth || yt2 >= bmapheight)
		return 0;

	if(xt2 > xt1)
	{
		mapxstep = 1;
		partialx = FRACUNIT - ((x1 >> MAPBTOFRAC) & (FRACUNIT - 1));
		ystep = FixedDiv(y2 - y1, abs(x2 - x1));
	} else
	if(xt2 < xt1)
	{
		mapxstep = -1;
		partialx = (x1 >> MAPBTOFRAC) & (FRACUNIT - 1);
		ystep = FixedDiv(y2 - y1, abs(x2 - x1));
	} else
	{
		mapxstep = 0;
		partialx = FRACUNIT;
		ystep = 256 * FRACUNIT;
	}
	yintercept = (y1 >> MAPBTOFRAC) + FixedMul(partialx, ystep);

	if(yt2 > yt1)
	{
		mapystep = 1;
		partialy = FRACUNIT - ((y1 >> MAPBTOFRAC) & (FRACUNIT - 1));
		xstep = FixedDiv(x2 - x1, abs(y2 - y1));
	} else
	if(yt2 < yt1)
	{
		mapystep = -1;
		partialy = (y1 >> MAPBTOFRAC) & (FRACUNIT - 1);
		xstep = FixedDiv(x2 - x1, abs(y2 - y1));
	} else
	{
		mapystep = 0;
		partialy = FRACUNIT;
		xstep = 256 * FRACUNIT;
	}
	xintercept = (x1 >> MAPBTOFRAC) + FixedMul(partialy, xstep);

	mapx = xt1;
	mapy = yt1;

	if(abs(xstep) == FRACUNIT && abs(ystep) == FRACUNIT)
	{
		if(ystep < 0)
			partialx = FRACUNIT - partialx;
		if(xstep < 0)
			partialy = FRACUNIT - partialy;
		if(partialx == partialy)
		{
			while(1)
			{
				if(!P_BlockLinesIterator(mapx, mapy, P_SightLineCheck))
					return 0;

				if(mapx == xt2 && mapy == yt2)
					break;

				if(	!P_BlockLinesIterator(mapx + mapxstep, mapy, P_SightLineCheck) ||
					!P_BlockLinesIterator(mapx, mapy + mapystep, P_SightLineCheck)
				)
					return 0;

				mapx += mapxstep;
				mapy += mapystep;
			}
			goto traverse;
		}
	}

	for(count = 0; count < 64; count++)
	{
		if(!P_BlockLinesIterator(mapx, mapy, P_SightLineCheck))
			return 0;

		if(mapx == xt2 && mapy == yt2)
			break;

		if((yintercept >> FRACBITS) == mapy)
		{
			yintercept += ystep;
			mapx += mapxstep;
		} else
		if((xintercept >> FRACBITS) == mapx)
		{
			xintercept += xstep;
			mapy += mapystep;
		} else
			count = 64;
	}

	if(count >= 64)
		// TODO: better solution
		return 0;
traverse:
	return P_SightTraverseIntercepts();
}

//
// API

__attribute((regparm(2),no_caller_saved_registers))
uint32_t P_CheckSight(mobj_t *t1, mobj_t *t2)
{
	extraplane_t *pl;

	if(t2->render_style >= RS_INVISIBLE && t1->flags1 & MF1_ISMONSTER && !(t1->flags & MF_CORPSE))
		// HACK: 'INVISIBLE' check should be in A_Chase
		return 0;

	if(rejectmatrix)
	{
		int32_t s1, s2;
		int32_t pnum, bytenum, bitnum;

		s1 = (t1->subsector->sector - sectors);
		s2 = (t2->subsector->sector - sectors);
		pnum = s1 * numsectors + s2;
		bytenum = pnum >> 3;
		bitnum = 1 << (pnum & 7);

		if(rejectmatrix[bytenum] & bitnum)
			return 0;
	}

	topplane = NULL;
	botplane = NULL;

	sightzstart = t1->z + t1->height - (t1->height >> 2);
	topslope = (t2->z + t2->height) - sightzstart;
	botslope = t2->z - sightzstart;

	pl = t1->subsector->sector->exceiling;
	while(pl)
	{
		if(	pl->flags & E3D_BLOCK_SIGHT &&
			*pl->height > sightzstart
		){
			topplane = pl;
			break;
		}
		pl = pl->next;
	}

	pl = t1->subsector->sector->exfloor;
	while(pl)
	{
		if(	pl->flags & E3D_BLOCK_SIGHT &&
			*pl->height < sightzstart
		){
			botplane = pl;
			break;
		}
		pl = pl->next;
	}

	if(!P_SightPathTraverse(t1->x, t1->y, t2->x, t2->y))
		return 0;

	if(topplane)
	{
		fixed_t slope = *topplane->height - sightzstart;
		if(slope < topslope)
			topslope = slope;
	}

	if(botplane)
	{
		fixed_t slope = *botplane->height - sightzstart;
		if(slope > botslope)
			botslope = slope;
	}

	if(topslope <= botslope)
		return 0;

	return 1;
}

