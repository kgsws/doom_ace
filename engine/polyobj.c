// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "map.h"
#include "polyobj.h"

#define POLYOBJ_MAX	255

typedef struct polyobj_s
{
	seg_t **segs;
	vertex_t *origin;
	union
	{
		struct polyobj_s *mirror;
		subsector_t *subsec;
	};
	fixed_t x, y;
	int32_t box[4];
	uint32_t validcount;
	uint8_t segcount;
	uint8_t busy;
	uint8_t type;
	uint8_t id;
	degenmobj_t soundorg;
} polyobj_t;

//

static uint32_t poly_count;
static polyobj_t polyobj[POLYOBJ_MAX];

//
// functions

static polyobj_t *find_poly(uint32_t id)
{
	polyobj_t *poly = polyobj;

	if(!id || id > 255)
		I_Error("[POLY] Invalid polyobject ID %u!", id);

	for(uint32_t i = 0; i < poly_count; i++, poly++)
	{
		if(poly->id == id)
			return poly;
	}

	poly = polyobj + poly_count;
	poly->id = id;
	poly->segs = NULL;
	poly->subsec = NULL;
	poly_count++;

	return poly;
}

static uint32_t seg_search(seg_t *start, fixed_t x, fixed_t y, seg_t **dst)
{
	uint32_t segcount = 1;

	if(dst)
	{
		*dst = start;
		dst++;
	}

	while(1)
	{
		seg_t *pick = NULL;

		for(uint32_t i = 0; i < numsegs; i++)
		{
			seg_t *seg = segs + i;

			if(start->v2->x == x && start->v2->y == y)
				return segcount;

			if(seg->v2->x == x && seg->v2->y == y)
			{
				pick = seg;
				break;
			}
		}

		if(!pick)
			break;

		segcount++;

		if(dst)
		{
			*dst = pick;
			dst++;
		}

		x = pick->v1->x;
		y = pick->v1->y;
	}

	I_Error("[POLY] Unclosed polyobject!");
}

static void remove_from_subsector(seg_t *seg)
{
	// this removes ALL the lines
	// just use propper polyobject container sectors
	for(uint32_t i = 0; i < numsubsectors; i++)
	{
		subsector_t *ss = subsectors + i;
		seg_t *start = segs + ss->firstline;
		seg_t *end = start + ss->numlines;

		if(seg >= start && seg < end)
			ss->numlines = 0;
	}
}

static void update_position(polyobj_t *poly)
{
	poly->box[BOXLEFT] = 0x7FFFFFFF;
	poly->box[BOXRIGHT] = -0x7FFFFFFF;
	poly->box[BOXBOTTOM] = 0x7FFFFFFF;
	poly->box[BOXTOP] = -0x7FFFFFFF;

	// move vertextes
	for(uint32_t i = 0; i < poly->segcount; i++)
	{
		seg_t *seg = poly->segs[i];
		vertex_t *origin = poly->origin + i;

		seg->v1->x = origin->x + poly->x;
		seg->v1->y = origin->y + poly->y;
	}

	// fix linedefs, find bounding box
	for(uint32_t i = 0; i < poly->segcount; i++)
	{
		seg_t *seg = poly->segs[i];
		line_t *line = seg->linedef;

		if(seg->v1->x < seg->v2->x)
		{
			line->bbox[BOXLEFT] = seg->v1->x;
			line->bbox[BOXRIGHT] = seg->v2->x;
		} else
		{
			line->bbox[BOXLEFT] = seg->v2->x;
			line->bbox[BOXRIGHT] = seg->v1->x;
		}
		if(seg->v1->y < seg->v2->y)
		{
			line->bbox[BOXBOTTOM] = seg->v1->y;
			line->bbox[BOXTOP] = seg->v2->y;
		} else
		{
			line->bbox[BOXBOTTOM] = seg->v2->y;
			line->bbox[BOXTOP] = seg->v1->y;
		}

		if(seg->v1->x < poly->box[BOXLEFT])
			poly->box[BOXLEFT] = seg->v1->x;
		if(seg->v1->y < poly->box[BOXBOTTOM])
			poly->box[BOXBOTTOM] = seg->v1->y;

		if(seg->v1->x > poly->box[BOXRIGHT])
			poly->box[BOXRIGHT] = seg->v1->x;
		if(seg->v1->y > poly->box[BOXTOP])
			poly->box[BOXTOP] = seg->v1->y;
	}

	// update sound origin
	poly->soundorg.x = poly->box[BOXLEFT] + (poly->box[BOXRIGHT] - poly->box[BOXLEFT]) / 2;
	poly->soundorg.y = poly->box[BOXBOTTOM] + (poly->box[BOXTOP] - poly->box[BOXBOTTOM]) / 2;

	// add MAXRADIUS
	poly->box[BOXLEFT] -= MAXRADIUS;
	poly->box[BOXBOTTOM] -= MAXRADIUS;
	poly->box[BOXRIGHT] += MAXRADIUS;
	poly->box[BOXTOP] += MAXRADIUS;

	// convert to BLOCKMAP units
	poly->box[BOXLEFT] = (poly->box[BOXLEFT] - bmaporgx) >> MAPBLOCKSHIFT;
	poly->box[BOXBOTTOM] = (poly->box[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
	poly->box[BOXRIGHT] = (poly->box[BOXRIGHT] - bmaporgx) >> MAPBLOCKSHIFT;
	poly->box[BOXTOP] = (poly->box[BOXTOP] - bmaporgy) >> MAPBLOCKSHIFT;
}

//
// API

void poly_reset()
{
	poly_count = 0;
}

void poly_object(map_thinghex_t *mt)
{
	polyobj_t *poly;

	poly = find_poly(mt->angle);

	if(mt->type != 9300)
	{
		// start spot
		poly->type = mt->type - 9301;
		poly->x = (fixed_t)mt->x << FRACBITS;
		poly->y = (fixed_t)mt->y << FRACBITS;
		poly->subsec = R_PointInSubsector(poly->x, poly->y);
		return;
	}

	// anchor
	poly->box[0] = (fixed_t)mt->x << FRACBITS;
	poly->box[1] = (fixed_t)mt->y << FRACBITS;
}

void poly_create()
{
	for(uint32_t i = 0; i < numsegs; i++)
	{
		seg_t *seg = segs + i;

		if(seg->linedef->special == 1) // Polyobj_StartLine
		{
			polyobj_t *poly;
			subsector_extra_t *se;
			uint32_t segcount;
			uint32_t id = seg->linedef->arg0;

			// find polyobject
			poly = find_poly(id);
			seg->linedef->special = 0;
			seg->linedef->arg0 = 0;

			if(poly->segs)
				I_Error("[POLY] Duplicate polyobject %u!", id);

			if(!poly->subsec)
				I_Error("[POLY] Polyobject %u has no start spot!", id);

			// count segs
			segcount = seg_search(seg, seg->v1->x, seg->v1->y, NULL);
			if(segcount > 255)
				I_Error("[POLY] Too many segs in %u!", id);

			// allocate memory
			poly->segcount = segcount;
			poly->segs = Z_Malloc(segcount * sizeof(seg_t*), PU_LEVEL, NULL);
			poly->origin = Z_Malloc(segcount * sizeof(vertex_t), PU_LEVEL, NULL);

			// fill segs
			seg_search(seg, seg->v1->x, seg->v1->y, poly->segs);

			// calculate origin, fix front sector
			for(uint32_t i = 0; i < segcount; i++)
			{
				seg_t *s = poly->segs[i];
				vertex_t *v = poly->origin + i;

				v->x = s->v1->x - poly->box[0];
				v->y = s->v1->y - poly->box[1];

				s->linedef->frontsector = poly->subsec->sector;
				if(s->linedef->backsector)
					s->linedef->backsector = poly->subsec->sector;

				remove_from_subsector(s);
			}

			// add to subsector
			se = e_subsectors + (poly->subsec - subsectors);
			se->poly.segs = poly->segs;
			se->poly.segcount = segcount;

			// reset position
			update_position(poly);

			// check mirror
			if(seg->linedef->arg1)
				poly->mirror = find_poly(seg->linedef->arg1);
		}
	}
}

uint32_t poly_BlockLinesIterator(int32_t x, int32_t y, line_func_t func)
{
	for(uint32_t i = 0; i < poly_count; i++)
	{
		polyobj_t *poly = polyobj + i;

		if(poly->validcount == validcount)
			continue;

		if(x >= poly->box[BOXLEFT] && x <= poly->box[BOXRIGHT] && y >= poly->box[BOXBOTTOM] && y <= poly->box[BOXTOP])
		{
			poly->validcount = validcount;

			for(uint32_t j = 0; j < poly->segcount; j++)
			{
				seg_t *seg = poly->segs[j];
				line_t *line = seg->linedef;

				if(line->validcount == validcount)
					continue;

				line->validcount = validcount;

				if(!func(line))
					return 0;
			}
		}
	}

	return 1;
}

