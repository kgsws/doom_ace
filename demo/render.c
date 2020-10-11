// kgsws' Doom ACE
// Doom renderer changes.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "render.h"
#include "map.h"

static void render_planeColormap(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static void render_segColormap(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static void render_maskedColormap() __attribute((regparm(2),no_caller_saved_registers));
static void render_spriteColormap(mobj_t*,vissprite_t*) __attribute((regparm(2),no_caller_saved_registers));
static vissprite_t *render_pspColormap(vissprite_t*,player_t*) __attribute((regparm(2),no_caller_saved_registers));
static void render_spriteColfunc(void*) __attribute((regparm(2),no_caller_saved_registers));
static int render_planeLight(int) __attribute((regparm(2),no_caller_saved_registers));

static void **colormaps;
static void **ds_colormap;
static void **dc_colormap;
static void **dc_translation;
static void **fixedcolormap;
static uint32_t **spritelights;
static uint32_t *zlight;
static seg_t **curline;

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
	// change lightlevel in sector to take only 8bit values
	{0x000372d2, CODE_HOOK | HOOK_UINT32, 0x0C40B60F}, // 'movzbl 0xc(%eax),%eax' // walls
	{0x00036764, CODE_HOOK | HOOK_UINT32, 0x0C52B60F}, // 'movzbl 0xc(%edx),%edx' // mid walls
	{0x00037f9e, CODE_HOOK | HOOK_UINT32, 0x0C50B60F}, // 'movzbl 0xc(%eax),%edx' // sprites
	{0x00038221, CODE_HOOK | HOOK_UINT32, 0x0C52B60F}, // 'movzbl 0xc(%edx),%edx' // player sprites
	// plane light level hook
	{0x00036654, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_planeLight},
	// terminator
	{0}
};

void render_init()
{
	// install hooks
	utils_install_hooks(hook_list);

	// patch 'zlight' table
	for(int i = 0; i < LIGHTLEVELS * MAXLIGHTZ; i++)
		zlight[i] -= (uint32_t)*colormaps;
}

//
// colored light

static __attribute((regparm(2),no_caller_saved_registers))
void render_planeColormap(uint32_t offs)
{
	*ds_colormap = plane_colormap + offs;
}

static __attribute((regparm(2),no_caller_saved_registers))
void render_segColormap(uint32_t offs)
{
	if(*fixedcolormap)
	{
		*dc_colormap = *fixedcolormap;
		return;
	}

	register void *cmap;
	uint8_t map = curline[0]->frontsector->colormap;

	if(map)
		// custom colormap
		cmap = *colormaps + (2 * 256) + map * 32 * 256;
	else
		// normal colormap
		cmap = *colormaps;

	*dc_colormap = cmap + offs;
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

	register void *cmap;
	uint8_t map = curline[0]->frontsector->colormap;

	if(map)
		// custom colormap
		cmap = *colormaps + (2 * 256) + map * 32 * 256;
	else
		// normal colormap
		cmap = *colormaps;

	*dc_colormap = cmap + offs;
	*colfunc = *basecolfunc;
}

static __attribute((regparm(2),no_caller_saved_registers))
void render_spriteColormap(mobj_t *mo, vissprite_t *vis)
{
	register fixed_t xscale asm("edi");
	register uint32_t base;

	if(mo->flags & MF_SHADOW)
	{
		vis->colormap = NULL;
		return;
	}

	if(mo->extra.flags & MFN_COLORHACK)
	{
		// color hack for demo
		vis->colormap = *colormaps + 33 * 256;
		return;
	}

	if(*fixedcolormap)
	{
		vis->colormap = *fixedcolormap;
		return;
	}

	if(mo->frame & FF_FULLBRIGHT)
	{
		vis->colormap = *colormaps;
	} else
	{
		int idx = xscale >> (LIGHTSCALESHIFT - *detailshift);
		if(idx >= MAXLIGHTSCALE)
			idx = MAXLIGHTSCALE - 1;

		base = (*spritelights)[idx];

		register void *cmap;
		uint8_t map = mo->subsector->sector->colormap;

		if(map)
			// custom colormap
			cmap = *colormaps + (2 * 256) + map * 32 * 256;
		else
			// normal colormap
			cmap = *colormaps;

		vis->colormap = cmap + base;
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

	if(state->frame & FF_FULLBRIGHT)
	{
		vis->colormap = *colormaps;
		return vis;
	}

	register void *cmap;
	uint8_t map = pl->mo->subsector->sector->colormap;

	if(map)
		// custom colormap
		cmap = *colormaps + (2 * 256) + map * 32 * 256;
	else
		// normal colormap
		cmap = *colormaps;

	vis->colormap = cmap + (*spritelights)[MAXLIGHTSCALE - 1];
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

	if(vis->mobjflags & MF_TRANSLATION)
	{
		*colfunc = (void*)relocate_addr_code(0x00034C50); // R_DrawTranslatedColumn
		*dc_translation = *translationtables - 256 + ((vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT - 8));
		return;
	}

	*colfunc = *basecolfunc;
}

static __attribute((regparm(2),no_caller_saved_registers))
int render_planeLight(int lightlevel)
{
	register int extra_light asm("ebx");
	uint8_t map = lightlevel >> 8;

	if(map)
		// custom colormap
		plane_colormap = *colormaps + (2 * 256) + map * 32 * 256;
	else
		// normal colormap
		plane_colormap = *colormaps;

	return ((lightlevel & 0xFF) >> LIGHTSEGSHIFT) + extra_light;
}

