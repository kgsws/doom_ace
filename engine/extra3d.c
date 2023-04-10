// kgsws' ACE Engine
////
// Extra (3D) floors!
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "map.h"
#include "render.h"
#include "draw.h"
#include "ldr_flat.h"
#include "extra3d.h"

static visplane_t *e3dplanes;
static uint32_t max_e3d_planes;

static visplane_t *cur_plane;

static uint32_t extra_clip_count;
int16_t *e3d_floorclip;
int16_t *e3d_ceilingclip;

static extra_height_t *e3d_heights;
static extra_height_t *cur_height;
extra_height_t *e3d_up_height;
extra_height_t *e3d_dn_height;

fixed_t tmextrafloor;
fixed_t tmextraceiling;
fixed_t tmextradrop;

uint_fast8_t e3d_plane_move;

//
// funcs

void add_floor_plane(extraplane_t **dest, sector_t *sec, line_t *line, uint32_t flags, uint16_t alpha)
{
	extraplane_t *pl = *dest;
	extraplane_t *new;
	fixed_t height;

	if(flags & E3D_SWAP_PLANES)
		height = sec->floorheight;
	else
		height = sec->ceilingheight;

	while(pl)
	{
		// sort, top to bottom
		if(height > *pl->height)
			break;
		dest = &pl->next;
		pl = pl->next;
	}

	new = Z_Malloc(sizeof(extraplane_t), PU_LEVEL_E3D, NULL);
	*dest = new;
	new->next = pl;
	new->line = line;
	new->source = sec;
	new->texture = &sides[line->sidenum[0]].midtexture;
	new->rowoffset = &sides[line->sidenum[0]].rowoffset;
	new->flags = flags;
	new->alpha = alpha;

	if(flags & E3D_SWAP_PLANES)
	{
		new->height = &sec->floorheight;
		new->pic = &sec->floorpic;
		new->light = NULL;
	} else
	{
		new->height = &sec->ceilingheight;
		new->pic = &sec->ceilingpic;
		new->light = &sec->lightlevel;
	}
}

void add_ceiling_plane(extraplane_t **dest, sector_t *sec, line_t *line, uint32_t flags, uint16_t alpha)
{
	extraplane_t *pl = *dest;
	extraplane_t *new;
	fixed_t height;

	if(flags & E3D_SWAP_PLANES)
		height = sec->ceilingheight;
	else
		height = sec->floorheight;

	while(pl)
	{
		// sort, bottom to top
		if(height < *pl->height)
			break;
		dest = &pl->next;
		pl = pl->next;
	}

	new = Z_Malloc(sizeof(extraplane_t), PU_LEVEL_E3D, NULL);
	*dest = new;
	new->next = pl;
	new->line = line;
	new->source = sec;
	new->flags = flags;
	new->alpha = alpha;

	if(flags & E3D_SWAP_PLANES)
	{
		new->height = &sec->ceilingheight;
		new->pic = &sec->ceilingpic;
	} else
	{
		new->height = &sec->floorheight;
		new->pic = &sec->floorpic;
	}
}

//
// extra planes

visplane_t *e3d_find_plane(fixed_t height, uint32_t picnum, uint16_t light, uint16_t alpha)
{
	visplane_t *check;

	for(check = e3dplanes; check < cur_plane; check++)
	{
		if(height == check->height && picnum == check->picnum && light == check->light && alpha == check->alpha)
			break;
	}

	if(check < cur_plane)
		return check;

	if(cur_plane - e3dplanes >= max_e3d_planes)
		return NULL;

	cur_plane++;

	check->height = height;
	check->picnum = picnum;
	check->light = light;
	check->alpha = alpha;
	check->minx = SCREENWIDTH;
	check->maxx = -1;

	memset(check->top, 255, sizeof(check->top));

	return check;
}

visplane_t *e3d_check_plane(visplane_t *pl, int32_t start, int32_t stop)
{
	int32_t intrl;
	int32_t intrh;
	int32_t unionl;
	int32_t unionh;
	int32_t x;

	if(start < pl->minx)
	{
		intrl = pl->minx;
		unionl = start;
	} else
	{
		unionl = pl->minx;
		intrl = start;
	}

	if(stop > pl->maxx)
	{
		intrh = pl->maxx;
		unionh = stop;
	} else
	{
		unionh = pl->maxx;
		intrh = stop;
	}

	for(x = intrl; x <= intrh; x++)
		if(pl->top[x] != 255)
			break;

	if(x > intrh)
	{
		pl->minx = unionl;
		pl->maxx = unionh;
		return pl;
	}

	if(cur_plane - e3dplanes >= max_e3d_planes)
		return NULL;

	cur_plane->height = pl->height;
	cur_plane->picnum = pl->picnum;
	cur_plane->lightlevel = pl->lightlevel;

	pl = cur_plane++;
	pl->minx = start;
	pl->maxx = stop;

	memset(pl->top, 255, sizeof(pl->top));

	return pl;
}

//
// extra heights

void e3d_add_height(fixed_t height)
{
	extra_height_t *add;
	extra_height_t *check;
	extra_height_t **pnext;

	if(height < viewz)
	{
		pnext = &e3d_dn_height;
		check = e3d_dn_height;
		while(check)
		{
			if(check->height == height)
				return;
			if(check->height > height)
				break;
			pnext = &check->next;
			check = check->next;
		}

		add = cur_height++;
		add->next = check;
		add->height = height;
		*pnext = add;
	} else
	{
		pnext = &e3d_up_height;
		check = e3d_up_height;
		while(check)
		{
			if(check->height == height)
				return;
			if(check->height < height)
				break;
			pnext = &check->next;
			check = check->next;
		}

		add = cur_height++;
		add->next = check;
		add->height = height;
		*pnext = add;
	}
}

//
// plane movement

void e3d_update_planes(sector_t *src, uint32_t planes)
{
	extraplane_t *pl;
	extraplane_t **pr;

	if(!src->e3d_origin)
		return;

	for(uint32_t i = 0; i < numsectors; i++)
	{
		sector_t *sec = sectors + i;

		if(!sec->ed3_multiple)
			continue;

		for(uint32_t i = 0; i < src->linecount; i++)
		{
			line_t *li = src->lines[i];

			if(li->e3d_tag == sec->tag)
			{
				if(planes & 1)
				{
					pl = sec->exfloor;
					pr = &sec->exfloor;
					while(pl->next)
					{
						if(*pl->height < *pl->next->height)
						{
							// swap pair
							extraplane_t *tmp = pl->next;
							pl->next = tmp->next;
							tmp->next = pl;
							*pr = tmp;
							// start again
							pl = sec->exfloor;
							pr = &sec->exfloor;
							continue;
						}
						pr = &pl->next;
						pl = pl->next;
					}
				}

				if(planes & 2)
				{
					pl = sec->exceiling;
					pr = &sec->exceiling;
					while(pl->next)
					{
						if(*pl->height > *pl->next->height)
						{
							// swap pair
							extraplane_t *tmp = pl->next;
							pl->next = tmp->next;
							tmp->next = pl;
							*pr = tmp;
							// start again
							pl = sec->exceiling;
							pr = &sec->exceiling;
							continue;
						}
						pr = &pl->next;
						pl = pl->next;
					}
				}

				break;
			}
		}
	}
}

//
// API

void e3d_check_midtex(mobj_t *mo, line_t *ln, uint32_t no_step)
{
	fixed_t bot, top;
	fixed_t z;
	side_t *side;

	tmextrafloor = tmfloorz;
	tmextraceiling = tmceilingz;

	if(!(ln->iflags & MLI_3D_MIDTEX))
		return;

	side = sides + ln->sidenum[0];

	if(!side->midtexture)
		return;

	if(ln->flags & ML_DONTPEGBOTTOM)
	{
		fixed_t hhh = ln->frontsector->floorheight;
		if(ln->backsector->floorheight > hhh)
			hhh = ln->backsector->floorheight;
		bot = ln->frontsector->floorheight + side->rowoffset;
		top = bot + textureheight[side->midtexture];
	} else
	{
		fixed_t hhh = ln->frontsector->ceilingheight;
		if(ln->backsector->ceilingheight < hhh)
			hhh = ln->backsector->ceilingheight;
		top = ln->frontsector->ceilingheight + side->rowoffset;
		bot = top - textureheight[side->midtexture];
	}

	z = mo->z;
	if(!no_step)
		z += mo->info->step_height;

	if(top <= z && top > tmextrafloor)
		tmextrafloor = top;

	if(mo->z < tmextrafloor)
	{
		z = tmextrafloor;
		tmextradrop = 0;
	} else
		z = mo->z;

	if(z >= bot && z < top)
	{
		tmextraceiling = tmextrafloor;
		return;
	}

	if(bot >= z && bot < tmextraceiling)
		tmextraceiling = bot;
}

extraplane_t *e3d_check_inside(sector_t *sec, fixed_t z, uint32_t flags)
{
	extraplane_t *pl = sec->exfloor;

	if(!pl)
		return NULL;

	while(pl)
	{
		if(pl->flags & flags)
		{
			if(z < pl->source->ceilingheight && z >= pl->source->floorheight)
				return pl;
		}
		pl = pl->next;
	}

	return NULL;
}

void e3d_check_heights(mobj_t *mo, sector_t *sec, uint32_t no_step)
{
	extraplane_t *pl;
	fixed_t z;

	tmextrafloor = tmfloorz;
	tmextraceiling = tmceilingz;
	tmextradrop = 0x7FFFFFFF;

	if(!sec->exfloor)
		return;

	z = mo->z;
	if(!no_step)
		z += mo->info->step_height;

	pl = sec->exfloor;
	while(pl)
	{
		if(pl->flags & E3D_SOLID && (!e3d_plane_move || mo->z > pl->source->floorheight || pl->source->floorheight == pl->source->ceilingheight))
		{
			if(*pl->height <= z && *pl->height > tmextrafloor)
				tmextrafloor = *pl->height;
			if(*pl->height <= mo->z)
				tmextradrop = mo->z - *pl->height;
		}
		pl = pl->next;
	}

	if(mo->z < tmextrafloor)
	{
		z = tmextrafloor;
		tmextradrop = 0;
	} else
		z = mo->z;

	pl = sec->exceiling;
	while(pl)
	{
		if(pl->flags & E3D_SOLID)
		{
			if(*pl->height > z && *pl->height < tmextraceiling)
				tmextraceiling = *pl->height;
		}
		pl = pl->next;
	}
}

void e3d_check_water(mobj_t *mo)
{
	extraplane_t *pl = mo->subsector->sector->exfloor;
	fixed_t zc = mo->z + mo->height / 2;
	fixed_t zt;

	mo->waterlevel = 0;

	if(mo->player)
		zt = mo->z + mo->info->player.view_height;
	else
		zt = mo->z + mo->height;

	while(pl)
	{
		if(pl->flags & E3D_WATER)
		{
			if(	mo->z < pl->source->ceilingheight &&
				zc >= pl->source->floorheight 
			){
				mo->waterlevel = 1;
				if(zc < pl->source->ceilingheight)
				{
					if(zt < pl->source->ceilingheight)
						mo->waterlevel = 3;
					else
						mo->waterlevel = 2;
				}
			}
		}
		pl = pl->next;
	}
}

void e3d_draw_height(fixed_t height)
{
	for(visplane_t *pl = e3dplanes; pl < cur_plane; pl++)
	{
		if(pl->height == height)
		{
			// setup alpha / style
			if(pl->alpha > 255)
			{
				spanfunc = R_DrawSpanTint0;
				dr_tinttab = render_add;
			} else
			if(pl->alpha > 250)
			{
				spanfunc = R_DrawSpan;
			} else
			if(pl->alpha > 178)
			{
				spanfunc = R_DrawSpanTint0;
				dr_tinttab = render_trn0;
			} else
			if(pl->alpha > 127)
			{
				spanfunc = R_DrawSpanTint0;
				dr_tinttab = render_trn1;
			} else
			if(pl->alpha > 76)
			{
				spanfunc = R_DrawSpanTint1;
				dr_tinttab = render_trn1;
			} else
			{
				spanfunc = R_DrawSpanTint1;
				dr_tinttab = render_trn0;
			}
			// masked?
			if(pl->picnum >= numflats)
			{
				ds_maskcolor = flattexture_mask[pl->picnum - numflats];
				if(spanfunc == R_DrawSpan)
					spanfunc = R_DrawMaskedSpan;
				else
				if(spanfunc == R_DrawSpanTint0)
					spanfunc = R_DrawMaskedSpanTint0;
				else
					spanfunc = R_DrawMaskedSpanTint1;
			}
			// draw
			r_draw_plane(pl);
		}
	}
}

void e3d_draw_planes()
{
	// not really used
	for(visplane_t *pl = e3dplanes; pl < cur_plane; pl++)
		r_draw_plane(pl);
}

void e3d_reset()
{
	// planes
	cur_plane = e3dplanes;

	// clipping
	for(uint32_t i = 0; i < extra_clip_count; i++)
	{
		e3d_floorclip[i] = viewheight;
		e3d_ceilingclip[i] = -1;
	}

	// heights
	cur_height = e3d_heights;
	e3d_up_height = NULL;
	e3d_dn_height = NULL;
}

void e3d_create()
{
	uint32_t top_count = 0;
	uint32_t height_count = 0;

	if(map_format == MAP_FORMAT_DOOM)
	{
		extra_clip_count = 0;
		height_count = 0;
		return;
	}

	// spawn extra floors
	for(uint32_t i = 0; i < numlines; i++)
	{
		line_t *ln = lines + i;
		sector_t *src;
		side_t *side;
		uint32_t tag;
		uint32_t flags;
		uint16_t alpha;

		if(ln->special != 160) // Sector_Set3dFloor
			continue;

		tag = ln->arg1 & ~(4 | 8 | 16 | 32);

		if(	tag < 1 || tag > 3 ||
			ln->arg2 & ~64
		)
			engine_error("EX3D", "Unsupported extra floor type!");

		side = sides + ln->sidenum[0];

		if(side->textureoffset)
			engine_error("EX3D", "Texture X offset is not supported!");

		src = side->sector;

		if(tag == 1)
			flags = E3D_SOLID | E3D_BLOCK_HITSCAN | E3D_BLOCK_SIGHT;
		else
			flags = 0;

		if(tag == 2)
			flags |= E3D_WATER | E3D_DRAW_INISIDE;

		tag = ln->arg0;

		if(ln->arg1 & 4)
			flags |= E3D_DRAW_INISIDE;

		if(ln->arg1 & 8)
			ln->id = ln->arg4;
		else
			tag |= ln->arg4 * 256;

		if(ln->arg1 & 16)
			flags ^= E3D_BLOCK_SIGHT;

		if(ln->arg1 & 32)
			flags ^= E3D_BLOCK_HITSCAN;

		if(ln->arg2 & 64)
		{
			alpha = 256; // additive
			if(ln->arg3 > 192)
				alpha++;
		} else
			alpha = ln->arg3;

		if(!tag)
			engine_error("EX3D", "Do not use zero tag!");

		if(src->e3d_origin < 2)
			src->e3d_origin++;

		ln->e3d_tag = tag;

		M_ClearBox(src->extra->bbox);

		for(uint32_t j = 0; j < numsectors; j++)
		{
			sector_t *sec = sectors + j;

			if(sec->tag == tag)
			{
				add_floor_plane(&sec->exfloor, src, ln, flags, alpha);
				add_ceiling_plane(&sec->exceiling, src, ln, flags, alpha);
				if(flags & (E3D_WATER | E3D_DRAW_INISIDE))
				{
					add_floor_plane(&sec->exfloor, src, ln, E3D_SWAP_PLANES, alpha);
					add_ceiling_plane(&sec->exceiling, src, ln, E3D_SWAP_PLANES | (flags & E3D_WATER), alpha);
				}
			}
		}

		// clear special
		ln->special = 0;
		ln->arg0 = 0;
		ln->args = 0;

		// check if other walls have Sector_Set3dFloor
		for(tag = 0; tag < src->linecount; tag++)
		{
			line_t *li = src->lines[tag];
			if(li->special == 160)
				break;
		}
		if(tag >= src->linecount)
			// nope; increment height count
			height_count++;
	}

	// find maximum extra floor count per sector
	// update model sector bounding box
	// mark sectors containing more than 1 extra floor
	for(uint32_t i = 0; i < numsectors; i++)
	{
		sector_t *sec = sectors + i;

		// count
		if(sec->exfloor)
		{
			extraplane_t *pl;
			uint32_t count = 0;

			pl = sec->exfloor;
			while(pl)
			{
				count++;
				pl = pl->next;
			}

			if(count > 1)
				sec->ed3_multiple = 1;

			if(count > top_count)
				top_count = count;
		}

		// model
		if(sec->e3d_origin)
		{
			int32_t block;

			for(uint32_t j = 0; j < numsectors; j++)
			{
				extraplane_t *pl;

				pl = sectors[j].exfloor;
				while(pl)
				{
					if(pl->source == sec)
					{
						for(uint32_t k = 0; k < sectors[j].linecount; k++)
						{
							line_t *li = sectors[j].lines[k];
							M_AddToBox(sec->extra->bbox, li->v1->x, li->v1->y);
							M_AddToBox(sec->extra->bbox, li->v2->x, li->v2->y);
						}
						break;
					}
					pl = pl->next;
				}
			}

			block = (sec->extra->bbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;
			block = block >= bmapheight ? bmapheight-1 : block;
			sec->blockbox[BOXTOP] = block;

			block = (sec->extra->bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
			block = block < 0 ? 0 : block;
			sec->blockbox[BOXBOTTOM] = block;

			block = (sec->extra->bbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
			block = block >= bmapwidth ? bmapwidth-1 : block;
			sec->blockbox[BOXRIGHT] = block;

			block = (sec->extra->bbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
			block = block < 0 ? 0 : block;
			sec->blockbox[BOXLEFT] = block;
		}
	}

	// mark linedefs that make extra floor boundary
	for(uint32_t i = 0; i < numlines; i++)
	{
		extraplane_t *pl;
		line_t *ln = lines + i;
		sector_t *frontsector = ln->frontsector;
		sector_t *backsector = ln->backsector;

		if(!backsector)
			continue;

		if(frontsector->tag == backsector->tag)
			continue;

		// find flipped extra floors
		pl = frontsector->exfloor;
		while(pl)
		{
			if(pl->alpha)
			{
				if(!pl->light)
					// masked when viewed from front
					ln->iflags |= MLI_EXTRA_FRONT;
				else
					// masked when viewed from back
					ln->iflags |= MLI_EXTRA_BACK;
			}
			pl = pl->next;
		}

		// find normal extra floors
		pl = backsector->exfloor;
		while(pl)
		{
			if(pl->alpha)
			{
				if(!pl->light)
					// masked when viewed from back
					ln->iflags |= MLI_EXTRA_BACK;
				else
					// masked when viewed from front
					ln->iflags |= MLI_EXTRA_FRONT;
			}
			pl = pl->next;
		}
	}

	// allocate extra clip
	extra_clip_count = top_count * SCREENWIDTH;
	if(extra_clip_count)
	{
		e3d_floorclip = Z_Malloc(extra_clip_count * sizeof(int16_t), PU_LEVEL_E3D, NULL);
		e3d_ceilingclip = Z_Malloc(extra_clip_count * sizeof(int16_t), PU_LEVEL_E3D, NULL);
	}

	// allocate extra heights
	if(height_count)
	{
		height_count++;
		height_count *= 2;
		e3d_heights = Z_Malloc(height_count * sizeof(extra_height_t), PU_LEVEL_E3D, NULL);
	}
}

void e3d_init(uint32_t count)
{
	ldr_alloc_message = "Extra 3D";
	max_e3d_planes = count;
	e3dplanes = ldr_malloc(count * sizeof(visplane_t));
	memset(e3dplanes, 0, count * sizeof(visplane_t));
}

