// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "map.h"
#include "mobj.h"
#include "think.h"
#include "special.h"
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
uint8_t *polybmap;

static uint_fast8_t poly_blocked;
static polyobj_t *poly_check;
static fixed_t poly_thrust;

//
// functions

static polyobj_t *find_poly(uint32_t id, uint32_t create)
{
	polyobj_t *poly = polyobj;

	if(!id || id > 255)
		I_Error("[POLY] Invalid polyobject ID %u!", id);

	for(uint32_t i = 0; i < poly_count; i++, poly++)
	{
		if(poly->id == id)
			return poly;
	}

	if(!create)
		I_Error("[POLY] Invalid polyobject ID %u!", id);

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

static int32_t blockminmax(int32_t value, int32_t limit)
{
	if(value < 0)
		return 0;
	if(value > limit)
		return limit;
	return value;
}

static void update_position(polyobj_t *poly)
{
	uint8_t *bm;
	int32_t bw, bh, bs;

	// remove from blockmap
	bw = 1 + poly->box[BOXRIGHT] - poly->box[BOXLEFT];
	bh = 1 + poly->box[BOXTOP] - poly->box[BOXBOTTOM];
	bs = bw - bw;
	bm = polybmap + poly->box[BOXLEFT] + poly->box[BOXBOTTOM] * bmapwidth;
	for(int32_t y = 0; y < bh; y++)
	{
		for(int32_t x = 0; x < bw; x++)
		{
			if(*bm)
				*bm -= 1;
			bm++;
		}
		bm += bs;
	}

	// reset blockmap
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

	// limit block range
	poly->box[BOXLEFT] = blockminmax(poly->box[BOXLEFT], bmapwidth - 1);
	poly->box[BOXRIGHT] = blockminmax(poly->box[BOXRIGHT], bmapwidth - 1);
	poly->box[BOXBOTTOM] = blockminmax(poly->box[BOXBOTTOM], bmapheight - 1);
	poly->box[BOXTOP] = blockminmax(poly->box[BOXTOP], bmapheight - 1);

	// add to blockmap
	bw = 1 + poly->box[BOXRIGHT] - poly->box[BOXLEFT];
	bh = 1 + poly->box[BOXTOP] - poly->box[BOXBOTTOM];
	bs = bw - bw;
	bm = polybmap + poly->box[BOXLEFT] + poly->box[BOXBOTTOM] * bmapwidth;
	for(int32_t y = 0; y < bh; y++)
	{
		for(int32_t x = 0; x < bw; x++)
		{
			*bm += 1;
			bm++;
		}
		bm += bs;
	}
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

	poly = find_poly(mt->angle, 1);

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
	if(poly_count)
	{
		polybmap = Z_Malloc(bmapwidth * bmapheight, PU_LEVEL, NULL);
		memset(polybmap, 0, bmapwidth * bmapheight);
	} else
		polybmap = NULL;

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
			poly = find_poly(id, 0);
			if(!poly)
				I_Error("[POLY] Bad polyobject %u!", id);
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
			poly->box[0] = 0;
			poly->box[1] = 0;
			poly->box[2] = 0;
			poly->box[3] = 0;
			update_position(poly);

			// check mirror
			if(seg->linedef->arg1)
				poly->mirror = find_poly(seg->linedef->arg1, 0);
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

//
// blocking logic

static void thrust_and_damage(mobj_t *mo, seg_t *seg)
{
	uint32_t angle;
	fixed_t mx, my;

	if(!(mo->flags & MF_SHOOTABLE) && !mo->player)
		return;

	angle = (seg->angle - ANG90) >> ANGLETOFINESHIFT;

	mx = FixedMul(poly_thrust, finecosine[angle]);
	my = FixedMul(poly_thrust, finesine[angle]);

	mo->momx += mx;
	mo->momy += my;

	if(poly_check->type > 0)
	{
		if(poly_check->type == 2 || !P_CheckPosition(mo, mo->x + mo->momx, mo->y + mo->momy))
			mobj_damage(mo, NULL, NULL, 3, NULL);
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t check_blocking(mobj_t *mo)
{
	fixed_t box[4];

	if(!(mo->flags & MF_SOLID) && !mo->player)
		return 1;

	box[BOXTOP] = mo->y + mo->radius;
	box[BOXBOTTOM] = mo->y - mo->radius;
	box[BOXLEFT] = mo->x - mo->radius;
	box[BOXRIGHT] = mo->x + mo->radius;

	for(uint32_t i = 0; i < poly_check->segcount; i++)
	{
		seg_t *seg = poly_check->segs[i];
		line_t *line = seg->linedef;

		if(	box[BOXRIGHT] <= line->bbox[BOXLEFT] ||
			box[BOXLEFT] >= line->bbox[BOXRIGHT] ||
			box[BOXTOP] <= line->bbox[BOXBOTTOM] ||
			box[BOXBOTTOM] >= line->bbox[BOXTOP]
		)
			continue;

		if(P_BoxOnLineSide(box, line) != -1)
			continue;

		thrust_and_damage(mo, seg);
		poly_blocked = 1;
	}

	return 1;
}

//
// thinker

__attribute((regparm(2),no_caller_saved_registers))
void think_poly_move(poly_move_t *pm)
{
	fixed_t ox, oy;
	polyobj_t *poly = pm->poly;
	uint32_t finished = 0;

	if(pm->wait)
	{
		pm->wait--;
		if(pm->wait)
			return;
	}

	ox = poly->x;
	oy = poly->y;

	if(pm->spd_x)
	{
		if(poly->x < pm->dst_x)
		{
			poly->x += pm->spd_x;
			if(poly->x >= pm->dst_x)
				finished = 1;
		} else
		{
			poly->x -= pm->spd_x;
			if(poly->x <= pm->dst_x)
				finished = 1;
		}
	}

	if(pm->spd_y)
	{
		if(poly->y < pm->dst_y)
		{
			poly->y += pm->spd_y;
			if(poly->y >= pm->dst_y)
				finished = 1;
		} else
		{
			poly->y -= pm->spd_y;
			if(poly->y <= pm->dst_y)
				finished = 1;
		}
	}

	if(finished)
	{
		poly->x = pm->dst_x;
		poly->y = pm->dst_y;
	}

	update_position(poly);

	poly_blocked = 0;
	poly_check = poly;
	poly_thrust = pm->thrust;

	for(uint32_t y = poly->box[BOXBOTTOM]; y <= poly->box[BOXTOP] && !poly_blocked; y++)
		for(uint32_t x = poly->box[BOXLEFT]; x <= poly->box[BOXRIGHT] && !poly_blocked; x++)
			P_BlockThingsIterator(x, y, check_blocking);

	if(poly_blocked)
	{
		poly->x = ox;
		poly->y = oy;
		update_position(poly);
		return;
	}

	if(finished)
	{
		if(pm->delay)
		{
			pm->wait = pm->delay;
			pm->dst_x = pm->org_x;
			pm->dst_y = pm->org_y;
			pm->delay = 0;
			return;
		}

		poly->busy = 0;
		pm->thinker.function = (void*)-1;
	}
}

static poly_move_t *poly_mover(polyobj_t *poly)
{
	poly_move_t *pm;

	pm = Z_Malloc(sizeof(poly_move_t), PU_LEVELSPEC, NULL);
	memset(pm, 0, sizeof(poly_move_t));
	pm->thinker.function = think_poly_move;
	pm->poly = poly;
	think_add(&pm->thinker);

	return pm;
}

//
// line actions

uint32_t poly_door_slide(polyobj_t *mirror)
{
	polyobj_t *poly;
	poly_move_t *pm;
	fixed_t vector[2];
	angle_t angle;
	fixed_t dist;

	if(!mirror)
	{
		poly = find_poly(spec_arg[0], 0);
		if(!poly)
			return 0;
	} else
		poly = mirror;

	if(poly->busy)
		return 0;

	poly->busy = 1;

	angle = spec_arg[2];
	if(mirror)
		angle = (angle + 128) & 255;

	switch(angle)
	{
		case 0:
			vector[0] = 1 << FRACBITS;
			vector[1] = 0;
		break;
		case 64:
			vector[0] = 0;
			vector[1] = 1 << FRACBITS;
		break;
		case 128:
			vector[0] = -(1 << FRACBITS);
			vector[1] = 0;
		break;
		case 192:
			vector[0] = 0;
			vector[1] = -(1 << FRACBITS);
		break;
		default:
		{
			angle <<= 5;
			vector[0] = finecosine[angle];
			vector[1] = finesine[angle];
		}
		break;
	}

	dist = spec_arg[3];

	pm = poly_mover(poly);

	pm->org_x = poly->x;
	pm->org_y = poly->y;

	pm->dst_x = poly->x + vector[0] * dist;
	pm->dst_y = poly->y + vector[1] * dist;

	if(pm->dst_x == poly->x)
		pm->spd_x = 0;
	else
		pm->spd_x = abs((vector[0] * spec_arg[1]) / 8);

	if(pm->dst_y == poly->y)
		pm->spd_y = 0;
	else
		pm->spd_y = abs((vector[1] * spec_arg[1]) / 8);

	pm->delay = spec_arg[4] + 1;

	pm->thrust = spec_arg[1] << 10;
	if(pm->thrust < (1 << 16))
		pm->thrust = 1 << 16;
	else
	if(pm->thrust > (4 << 16))
		pm->thrust = 4 << 16;

	if(!mirror && poly->mirror)
		poly_door_slide(poly->mirror);

	return 0;
}

