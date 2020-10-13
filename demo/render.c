// kgsws' Doom ACE
// Doom renderer changes.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "render.h"
#include "map.h"

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

static void render_planeColormap(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static int render_planeLight(int) __attribute((regparm(2),no_caller_saved_registers));
static void render_segColormap(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static int render_segLight(int32_t,vertex_t*) __attribute((regparm(2),no_caller_saved_registers));
static void render_maskedColormap() __attribute((regparm(2),no_caller_saved_registers));
static int render_maskedLight() __attribute((regparm(2),no_caller_saved_registers));
static void render_spriteColormap(mobj_t*,vissprite_t*) __attribute((regparm(2),no_caller_saved_registers));
static int render_spriteLight() __attribute((regparm(2),no_caller_saved_registers));
static vissprite_t *render_pspColormap(vissprite_t*,player_t*) __attribute((regparm(2),no_caller_saved_registers));
static void render_spriteColfunc(void*) __attribute((regparm(2),no_caller_saved_registers));
static void *render_skyColormap() __attribute((regparm(2),no_caller_saved_registers));

static void **colormaps;
static void **ds_colormap;
static void **dc_colormap;
static void **dc_translation;
static void **fixedcolormap;
static uint32_t **spritelights;
static uint32_t *zlight;
static seg_t **curline;
static int *extralight;

static void **colfunc;
static void **basecolfunc;
static void **fuzzcolfunc;

static void **translationtables;

static void *plane_colormap;

static hook_t hook_list[] =
{
	// required variables
	{0x00030104, DATA_HOOK | HOOK_IMPORT, (uint32_t)&colormaps},
	{0x000322D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&ds_colormap},
	{0x000322D8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&dc_colormap},
	{0x000322B8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&dc_translation},
	{0x00038FFC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&fixedcolormap},
	{0x000322B4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&translationtables},
	{0x0005C550, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spritelights},
	{0x000323E0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&zlight},
	{0x000300A4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&curline},
	{0x00039014, DATA_HOOK | HOOK_IMPORT, (uint32_t)&extralight},
	// render function pointers
	{0x00039010, DATA_HOOK | HOOK_IMPORT, (uint32_t)&colfunc},
	{0x0003900C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&basecolfunc},
	{0x00039008, DATA_HOOK | HOOK_IMPORT, (uint32_t)&fuzzcolfunc},
	// disable 'colormaps' offset in 'scalelight' so any colormap can be used
	{0x00035d34, CODE_HOOK | HOOK_UINT16, 0x9090},
	// setup 'ds_colormap' in 'R_MapPlane'
	{0x00036189, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_planeColormap},
	// setup 'dc_colormap' in 'R_RenderSegLoop'
	{0x00036ac4, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_segColormap},
	// setup 'dc_colormap' in 'R_RenderMaskedSegRange'
	{0x000368e4, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_maskedColormap},
	// modify end of 'R_ProjectSprite' for custom colormaps
	{0x00037f31, CODE_HOOK | HOOK_UINT32, 0xda89f089}, // 'mov %esi,%eax' 'mov %ebx,%edx'
	{0x00037f35, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_spriteColormap},
	{0x00037f3a, CODE_HOOK | HOOK_UINT16, 0x11EB}, // 'jmp'
	// modify end of 'R_DrawPSprite' for custom colormaps
	{0x000381b2, CODE_HOOK | HOOK_UINT16, 0x0E8B}, // 'mov (%esi),%ecx'
	{0x000381b4, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_pspColormap},
	{0x000381b9, CODE_HOOK | HOOK_UINT16, 0x3DEB}, // 'jmp'
	// modify 'R_DrawVisSprite' for custom render functions
	{0x00037bb5, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_spriteColfunc},
	{0x00037bba, CODE_HOOK | HOOK_UINT16, 0x39EB}, // 'jmp'
	// custom light level handling for 'walls'
	{0x000372d8, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_segLight},
	{0x000372dd, CODE_HOOK | HOOK_UINT16, 0x10EB}, // 'jmp'
	// custom light level handling for 'mid walls'
	{0x0003676e, CODE_HOOK | HOOK_UINT8, 0x50}, // 'push %eax'
	{0x0003676f, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_maskedLight},
	{0x00036774, CODE_HOOK | HOOK_UINT32, 0xEB58c289}, // 'mov %eax,%edx' 'pop %eax' 'jmp'
	{0x00036778, CODE_HOOK | HOOK_UINT8, 0x18}, // jmp offset
	// custom light level handling for 'sprites'
	{0x00037fa2, CODE_HOOK | HOOK_UINT8, 0x50}, // 'push %eax'
	{0x00037fa3, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_spriteLight},
	{0x00037fa8, CODE_HOOK | HOOK_UINT32, 0x58c289}, // 'mov %eax,%edx' 'pop %eax'
	{0x00037fab, CODE_HOOK | HOOK_UINT16, 0x9090}, // 'nop's
	// change lightlevel in sector to take only 8bit values
	{0x00038221, CODE_HOOK | HOOK_UINT32, 0x0C52B60F}, // 'movzbl 0xc(%edx),%edx' // player sprites
	// plane light level hook
	{0x00036654, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_planeLight},
	// sky colormap
	{0x00036584, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_skyColormap},
	// disable player translation mobj flags // TODO: apply new translation
	{0x00031859, CODE_HOOK | HOOK_UINT8, 0xEB}, // 'jmp'
	// terminator
	{0}
};

// list of custom colormaps
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
	// install hooks
	utils_install_hooks(hook_list);

	// patch 'zlight' table
	for(int i = 0; i < LIGHTLEVELS * MAXLIGHTZ; i++)
		zlight[i] -= (uint32_t)*colormaps;

	// relocate colormap pointers
	for(int i = 0; i < COLORMAP_COUNT; i++)
		colormap_list[i].offset += (uint32_t)*colormaps;
}

//
// colored light

static __attribute((regparm(2),no_caller_saved_registers))
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

static __attribute((regparm(2),no_caller_saved_registers))
void render_planeColormap(uint32_t offs)
{
	*ds_colormap = plane_colormap + offs;
}

static __attribute((regparm(2),no_caller_saved_registers))
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

static __attribute((regparm(2),no_caller_saved_registers))
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

static __attribute((regparm(2),no_caller_saved_registers))
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

static __attribute((regparm(2),no_caller_saved_registers))
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

static __attribute((regparm(2),no_caller_saved_registers))
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

static __attribute((regparm(2),no_caller_saved_registers))
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

	if(mo->extra.flags & MFN_COLORHACK)
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
		int idx = xscale >> (LIGHTSCALESHIFT - *detailshift);
		if(idx >= MAXLIGHTSCALE)
			idx = MAXLIGHTSCALE - 1;

		base = (*spritelights)[idx];

		vis->colormap = (void*)colormap_list[map].offset + base;
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
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

static __attribute((regparm(2),no_caller_saved_registers))
void render_spriteColfunc(void *colormap)
{
	register vissprite_t *vis asm("edi");

	if(!colormap)
	{
		*colfunc = *fuzzcolfunc;
		return;
	}

	if(vis->mo && vis->mo->extra.translation)
	{
		*colfunc = (void*)relocate_addr_code(0x00034C50); // R_DrawTranslatedColumn
		*dc_translation = vis->mo->extra.translation;
		return;
	}

	*colfunc = *basecolfunc;
}

static __attribute((regparm(2),no_caller_saved_registers))
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

