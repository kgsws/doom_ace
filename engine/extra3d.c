// kgsws' ACE Engine
////
// Extra (3D) floors!
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "map.h"
#include "render.h"
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

//
// funcs

void add_floor_plane(extraplane_t **dest, sector_t *sec, line_t *line)
{
	extraplane_t *pl = *dest;
	extraplane_t *new;

	while(pl)
	{
		if(sec->ceilingheight < *pl->height)
			break;
		dest = &pl->next;
		pl = pl->next;
	}

	new = Z_Malloc(sizeof(extraplane_t), PU_LEVEL, NULL);
	*dest = new;
	new->next = pl;
	new->line = line;
	new->source = sec;
	new->height = &sec->ceilingheight;
	new->pic = &sec->ceilingpic;
	new->texture = &sides[line->sidenum[0]].midtexture;
	new->light = &sec->lightlevel;
	new->validcount = 0;
}

void add_ceiling_plane(extraplane_t **dest, sector_t *sec, line_t *line)
{
	extraplane_t *pl = *dest;
	extraplane_t *new;

	while(pl)
	{
		if(sec->floorheight > *pl->height)
			break;
		dest = &pl->next;
		pl = pl->next;
	}

	new = Z_Malloc(sizeof(extraplane_t), PU_LEVEL, NULL);
	*dest = new;
	new->next = pl;
	new->line = line;
	new->source = sec;
	new->height = &sec->floorheight;
	new->pic = &sec->floorpic;
	new->texture = &sides[line->sidenum[0]].midtexture;
	new->light = &sec->lightlevel;
	new->validcount = 0;
}

//
// extra planes

visplane_t *e3d_find_plane(fixed_t height, uint32_t picnum, uint32_t light)
{
	visplane_t *check;

	for(check = e3dplanes; check < cur_plane; check++)
	{
		if(height == check->height && picnum == check->picnum && light == check->lightlevel)
			break;
	}

	if(check < cur_plane)
		return check;

	if(cur_plane - e3dplanes >= max_e3d_planes)
		return NULL;

	cur_plane++;

	check->height = height;
	check->picnum = picnum;
	check->lightlevel = light;
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
// API

void e3d_check_heights(mobj_t *mo, sector_t *sec)
{
	extraplane_t *pl;
	fixed_t z = mo->z;

	tmextrafloor = tmfloorz;
	tmextraceiling = tmceilingz;
	tmextradrop = 0x7FFFFFFF;

	if(!(mo->flags & MF_MISSILE))
		z += mo->info->step_height;

	pl = sec->exfloor;
	while(pl)
	{
		if(*pl->height <= z && *pl->height > tmextrafloor)
			tmextrafloor = *pl->height;
		if(*pl->height <= mo->z)
			tmextradrop = mo->z - *pl->height;
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
		if(*pl->height >= z && *pl->height < tmextraceiling)
			tmextraceiling = *pl->height;
		pl = pl->next;
	}
}

void e3d_draw_height(fixed_t height)
{
	for(visplane_t *pl = e3dplanes; pl < cur_plane; pl++)
	{
		if(pl->height == height)
			r_draw_plane(pl);
	}
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
		uint32_t tag;

		if(ln->special != 160) // Sector_Set3dFloor
			continue;

		if(	(ln->arg1 != 1 && ln->arg1 != 3) ||
			ln->arg2
		)
			I_Error("[EX3D] Unsupported extra floor type!");

		src = sides[ln->sidenum[0]].sector;

		tag = ln->arg0 + ln->arg4 * 256;

		for(uint32_t j = 0; j < numsectors; j++)
		{
			sector_t *sec = sectors + j;

			if(sec->tag == tag)
			{
				add_floor_plane(&sec->exfloor, src, ln);
				add_ceiling_plane(&sec->exceiling, src, ln);
			}

			// TODO: update things
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

	// find maxium extra floor count per sector
	for(uint32_t i = 0; i < numsectors; i++)
	{
		extraplane_t *pl;
		sector_t *sec = sectors + i;
		uint32_t count = 0;

		pl = sec->exfloor;
		while(pl)
		{
			count++;
			pl = pl->next;
		}

		if(count > top_count)
			top_count = count;
	}

	// allocate extra clip
	extra_clip_count = top_count * SCREENWIDTH;
	if(extra_clip_count)
	{
		e3d_floorclip = Z_Malloc(extra_clip_count * sizeof(int16_t), PU_LEVEL, NULL);
		e3d_ceilingclip = Z_Malloc(extra_clip_count * sizeof(int16_t), PU_LEVEL, NULL);
	}

	// allocate extra heights
	if(height_count)
	{
		height_count++;
		height_count *= 2;
		e3d_heights = Z_Malloc(height_count * sizeof(extra_height_t), PU_LEVEL, NULL);
	}
}

void e3d_init(uint32_t count)
{
	ldr_alloc_message = "Extra 3D";
	max_e3d_planes = count;
	e3dplanes = ldr_malloc(count * sizeof(visplane_t));
	memset(e3dplanes, 0, count * sizeof(visplane_t));
}

