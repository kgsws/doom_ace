// kgsws' Doom ACE
// Doom renderer changes.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "render.h"
#include "map.h"
//#include "mlook.h"

#define CMAPF_NOFULLBRIGHT	1
#define CMAPF_NOFAKECONTRAST	2
#define CMAPF_NOEXTRALIGHT	4
#define CMAPF_NOFIXEDCOLORMAP	8 // TODO: implement; also add option for fullbright vs shaded
#define CMAPF_ISFOG	(CMAPF_NOFULLBRIGHT | CMAPF_NOFAKECONTRAST | CMAPF_NOEXTRALIGHT)
#define CMAPF_ISLIGHT	(CMAPF_NOFAKECONTRAST | CMAPF_NOEXTRALIGHT)

typedef struct
{
	uint32_t offset;
	uint32_t flags;
} colormap_t;

uint32_t *setsizeneeded;
void **ds_colormap;
void **dc_colormap;
void **dc_translation;
void **fixedcolormap;
uint32_t **spritelights;
seg_t **curline;
int *extralight;
void **colormaps;

void **colfunc;
void **basecolfunc;
void **fuzzcolfunc;

static void *plane_colormap;

// list of custom colormaps // TODO: propper colormap support
static colormap_t colormap_list[] =
{
	{0, 0}, // normal doom light
	{(1+32*1) * 256, CMAPF_ISLIGHT}, // red
	{(1+32*2) * 256, CMAPF_ISLIGHT}, // green
	{(1+32*3) * 256, CMAPF_ISLIGHT}, // yellow
	{(1+32*4) * 256, CMAPF_ISLIGHT}, // blue
	{(1+32*5) * 256, CMAPF_ISLIGHT}, // orange
	{(1+32*6) * 256, CMAPF_ISLIGHT}, // "cyan"
	{(1+32*7) * 256, CMAPF_ISFOG}, // reverse fog
	{(1+32*8) * 256, CMAPF_ISFOG}, // gray/white fog
	{(1+32*9) * 256, CMAPF_ISFOG}, // green fog
};
#define COLORMAP_COUNT	(sizeof(colormap_list) / sizeof(colormap_t))

void render_init()
{
	// relocate colormap pointers
	for(int i = 0; i < COLORMAP_COUNT; i++)
		colormap_list[i].offset += (uint32_t)*colormaps;

	// force recalculation
	*setsizeneeded = 1;
}

__attribute((regparm(2),no_caller_saved_registers))
void render_SetViewSize()
{
	// force mouselook to recalculate
//	mlook_recalc();
	// call original function
	R_ExecuteSetViewSize();
}

//
// colored light

__attribute((regparm(2),no_caller_saved_registers))
int render_planeLight(int lightlevel)
{
	register int extra_light asm("ebx");
	uint8_t map = lightlevel >> 8;

	if(map >= COLORMAP_COUNT)
		map = 0;

	plane_colormap = (void*)colormap_list[map].offset;
	if(colormap_list[map].flags & CMAPF_NOEXTRALIGHT)
		extra_light = 0;

	return ((lightlevel & 0xFF) >> LIGHTSEGSHIFT) + extra_light;
}

__attribute((regparm(2),no_caller_saved_registers))
void render_planeColormap(uint32_t offs)
{
	*ds_colormap = plane_colormap + offs;
}

__attribute((regparm(2),no_caller_saved_registers))
int render_segLight(int32_t lightnum, vertex_t *v1)
{
	register fixed_t extra_light asm("ecx");
	register vertex_t *v2 asm("ebx");
	uint8_t map = lightnum >> 8;

	if(map >= COLORMAP_COUNT)
		map = 0;

	lightnum = (lightnum & 0xFF) >> LIGHTSEGSHIFT;

	if(!(colormap_list[map].flags & CMAPF_NOEXTRALIGHT))
		lightnum += extra_light;

	if(!(colormap_list[map].flags & CMAPF_NOFAKECONTRAST))
	{
		// fake contrast
		if(v1->y == v2->y)
			lightnum--;
		if(v1->x == v2->x)
			lightnum++;
	}

	return lightnum;
}

__attribute((regparm(2),no_caller_saved_registers))
void render_segColormap(uint32_t offs)
{
	if(*fixedcolormap)
	{
		*dc_colormap = *fixedcolormap;
		return;
	}

	uint8_t map = curline[0]->frontsector->colormap;
	if(map >= COLORMAP_COUNT)
		map = 0;

	*dc_colormap = (void*)colormap_list[map].offset + offs;
}

__attribute((regparm(2),no_caller_saved_registers))
int render_maskedLight()
{
	register fixed_t extra_light asm("ebp");
	register int lightnum asm("edx");
	register line_t *cur_line asm("ebx");
	uint8_t map = lightnum >> 8;

	if(map >= COLORMAP_COUNT)
		map = 0;

	lightnum = (lightnum & 0xFF) >> LIGHTSEGSHIFT;

	if(!(colormap_list[map].flags & CMAPF_NOEXTRALIGHT))
		lightnum += extra_light;

	if(!(colormap_list[map].flags & CMAPF_NOFAKECONTRAST))
	{
		// fake contrast
		if(cur_line->v1->y == cur_line->v2->y)
			lightnum--;
		if(cur_line->v1->x == cur_line->v2->x)
			lightnum++;
	}

	return lightnum;
}

__attribute((regparm(2),no_caller_saved_registers))
void render_maskedColormap(uint32_t offs)
{
	if(curline[0]->linedef->special == 208 && curline[0]->linedef->arg2)
	{
		// hack for demo
		*colfunc = *fuzzcolfunc;
		return;
	}

	uint8_t map = curline[0]->frontsector->colormap;
	if(map >= COLORMAP_COUNT)
		map = 0;

	*dc_colormap = (void*)colormap_list[map].offset + offs;
	*colfunc = *basecolfunc;
}

__attribute((regparm(2),no_caller_saved_registers))
int render_spriteLight()
{
	register int lightnum asm("edx");
	register int extra;
	uint8_t map = lightnum >> 8;

	if(map >= COLORMAP_COUNT)
		map = 0;

	if(colormap_list[map].flags & CMAPF_NOEXTRALIGHT)
		extra = 0;
	else
		extra = *extralight;

	return ((lightnum & 0xFF) >> LIGHTSEGSHIFT) + extra;
}

__attribute((regparm(2),no_caller_saved_registers))
void render_spriteColormap(mobj_t *mo, vissprite_t *vis)
{
	register fixed_t xscale asm("edi");
	register uint32_t base;

	// replace 'mobjflags' with mobj_t* for vissprites
	vis->mo = mo;

	if(mo->flags & MF_SHADOW)
	{
		vis->colormap = NULL;
		return;
	}

	if(mo->new_flags & MFN_COLORHACK)
	{
		// color hack for demo
		vis->colormap = *colormaps + (1 + 32 * 10) * 256;
		return;
	}

	if(*fixedcolormap)
	{
		vis->colormap = *fixedcolormap;
		return;
	}

	uint8_t map = mo->subsector->sector->colormap;
	if(map >= COLORMAP_COUNT)
		map = 0;

	if(mo->frame & FF_FULLBRIGHT && !(colormap_list[map].flags & CMAPF_NOFULLBRIGHT))
	{
		vis->colormap = *colormaps;
	} else
	{
		int idx = xscale >> LIGHTSCALESHIFT;
		if(idx >= MAXLIGHTSCALE)
			idx = MAXLIGHTSCALE - 1;

		base = (*spritelights)[idx];

		vis->colormap = (void*)colormap_list[map].offset + base;
	}
}

__attribute((regparm(2),no_caller_saved_registers))
vissprite_t *render_pspColormap(vissprite_t *vis, player_t *pl)
{
	// ecx = *state_t
	// esi = *psp_t
	register state_t *state asm("ecx");

	// any rendering function can be used
	// need player-colored guns?
//	*colfunc = *basecolfunc;

	if(pl->powers[pw_invisibility] > 200 || pl->powers[pw_invisibility] & 8)
	{
		vis->colormap = NULL;
		return vis;
	}

	if(*fixedcolormap)
	{
		vis->colormap = *fixedcolormap;
		return vis;
	}

	uint8_t map = pl->mo->subsector->sector->colormap;
	if(map >= COLORMAP_COUNT)
		map = 0;

	if(state->frame & FF_FULLBRIGHT && !(colormap_list[map].flags & CMAPF_NOFULLBRIGHT))
	{
		vis->colormap = *colormaps;
		return vis;
	}

	vis->colormap = (void*)colormap_list[map].offset + (*spritelights)[MAXLIGHTSCALE - 1];
	return vis;
}

__attribute((regparm(2),no_caller_saved_registers))
void render_spriteColfunc(void *colormap)
{
	register vissprite_t *vis asm("edi");

	if(!colormap)
	{
		*colfunc = *fuzzcolfunc;
		return;
	}

	if(vis->mo && vis->mo->translation)
	{
		*colfunc = (void*)relocate_addr_code(0x00034C50); // R_DrawTranslatedColumn
		*dc_translation = vis->mo->translation;
		return;
	}

	*colfunc = *basecolfunc;
}

__attribute((regparm(2),no_caller_saved_registers))
void *render_skyColormap()
{
	// custom sky color can be implemented
	if(*fixedcolormap)
		// this fixes invulnerability sky bug
		return *fixedcolormap;
	return *colormaps;
}

//
// stuff
__attribute((regparm(2)))
void *render_get_translation(int num)
{
	if(!num)
		return NULL;
	return *colormaps + (32*10+num) * 256;
}

