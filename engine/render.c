// kgsws' ACE Engine
////
// This handles both game and mod configuration.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "config.h"
#include "player.h"
#include "map.h"
#include "stbar.h"
#include "render.h"

uint32_t *viewheight;
uint32_t *viewwidth;

int32_t *centerx;
int32_t *centery;

int32_t *viewwindowx;
int32_t *viewwindowy;

uint32_t *screenblocks;

uint32_t *usegamma;

fixed_t *centeryfrac;
fixed_t *yslope;

uint32_t *dc_x;
uint32_t *dc_yl;
uint32_t *dc_yh;
uint8_t **dc_source;

fixed_t *sprtopscreen;
fixed_t *spryscale;

int16_t **mfloorclip;
int16_t **mceilingclip;

void (**colfunc)();

static fixed_t cy_weapon;
static fixed_t cy_look;

static fixed_t mlook_pitch;

static void *ptr_visplanes;
static void *ptr_vissprites;
static void *ptr_drawsegs;

uint8_t r_palette[768];

// mouselook scale
static const uint16_t look_scale_table[] =
{
	1370,
	1030,
	820,
	685,
	585,
	515,
	455,
};

// basic colors
uint8_t r_color_black;

// hooks
static hook_t hook_drawseg[];
static hook_t hook_visplane[];
static hook_t hook_vissprite[];

//
// API

uint8_t r_find_color(uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t ret = 0;
	uint32_t best = 0xFFFFFFFF;
	uint8_t *pal = r_palette;

	for(uint32_t i = 0; i < 256; i++)
	{
		int32_t tmp;
		uint32_t value;

		if(pal[0] == r && pal[1] == g && pal[2] == b)
			return i;

		tmp = pal[0];
		tmp -= r;
		tmp *= tmp;
		value = tmp;

		tmp = pal[1];
		tmp -= g;
		tmp *= tmp;
		value += tmp;

		tmp = pal[2];
		tmp -= b;
		tmp *= tmp;
		value += tmp;

		pal += 3;

		if(value < best)
		{
			ret = i;
			best = value;
		}
	}

	return ret;
}

void r_init_palette(uint8_t *palette)
{
	memcpy(r_palette, palette, 768);
}

void init_render()
{
	ldr_alloc_message = "Render";

	// find some colors
	r_color_black = r_find_color(0, 0, 0);

	// drawseg limit
	if(mod_config.drawseg_count > 256)
	{
		doom_printf("[RENDER] New drawseg limit %u\n", mod_config.drawseg_count);
		// allocate new memory
		ptr_drawsegs = ldr_malloc(mod_config.drawseg_count * sizeof(drawseg_t));
		// update values in hooks
		hook_drawseg[0].value = (uint32_t)ptr_drawsegs + mod_config.drawseg_count * sizeof(drawseg_t);
		for(int i = 1; i <= 5; i++)
			hook_drawseg[i].value = (uint32_t)ptr_drawsegs;
		// install hooks
		utils_install_hooks(hook_drawseg, 6);
	}

	// visplane limit
	if(mod_config.visplane_count > 128)
	{
		doom_printf("[RENDER] New visplane limit %u\n", mod_config.visplane_count);
		// allocate new memory
		ptr_visplanes = ldr_malloc(mod_config.visplane_count * sizeof(visplane_t));
		memset(ptr_visplanes, 0, mod_config.visplane_count * sizeof(visplane_t));
		// update values in hooks
		for(int i = 0; i <= 4; i++)
			hook_visplane[i].value = (uint32_t)ptr_visplanes;
		for(int i = 5; i <= 6; i++)
			hook_visplane[i].value = mod_config.visplane_count;
		// install hooks
		utils_install_hooks(hook_visplane, 7);
	}

	// vissprite limit
	if(mod_config.vissprite_count > 128)
	{
		doom_printf("[RENDER] New vissprite limit %u\n", mod_config.vissprite_count);
		// allocate new memory
		ptr_vissprites = ldr_malloc(mod_config.vissprite_count * sizeof(vissprite_t));
		// update values in hooks
		for(int i = 0; i <= 5; i++)
			hook_vissprite[i].value = (uint32_t)ptr_vissprites;
		hook_vissprite[6].value = (uint32_t)ptr_vissprites + mod_config.vissprite_count * sizeof(vissprite_t);
		// install hooks
		utils_install_hooks(hook_vissprite, 7);
	}
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void custom_SetupFrame(player_t *pl)
{
	fixed_t pn;
	int32_t div;

	if(*screenblocks < 10)
		div = look_scale_table[*screenblocks - 3];
	else
		div = 410;

	if(extra_config.mouse_look > 1)
		pn = 0;
	else
		pn = finetangent[(pl->mo->pitch + ANG90) >> ANGLETOFINESHIFT] / div;

	if(mlook_pitch != pn)
	{
		fixed_t cy = *viewheight / 2;

		mlook_pitch = pn;

		if(pn > 0)
			// allow weapon offset when looking up
			// but not full range
			cy_weapon = cy + (pn >> 4);
		else
			// and not at all when looking down
			cy_weapon = cy;

		cy += pn;
		cy_look = cy;
		*centery = cy;
		*centeryfrac = cy << FRACBITS;

		// tables for planes
		for(int i = 0; i < *viewheight; i++)
		{
			int dy = ((i - cy) << FRACBITS) + FRACUNIT/2;
			dy = abs(dy);
			yslope[i] = FixedDiv(*viewwidth / 2*FRACUNIT, dy);
		}
	}

	// now run the original
	R_SetupFrame(pl);
}

static __attribute((regparm(2),no_caller_saved_registers))
void custom_DrawPlayerSprites()
{
	*centery = cy_weapon;
	*centeryfrac = cy_weapon << FRACBITS;

	R_DrawPlayerSprites();

	*centery = cy_look;
	*centeryfrac = cy_look << FRACBITS;
}

static __attribute((regparm(2),no_caller_saved_registers))
void hook_RenderPlayerView(player_t *pl)
{
	if(!*automapactive)
		// render 3D view
		R_RenderPlayerView(pl);

	// status bar
	stbar_draw(pl);
}

// expanded drawseg limit
static hook_t hook_drawseg[] =
{
	// change limit check pointer in 'R_StoreWallRange'
	{0x00036d0f, CODE_HOOK | HOOK_UINT32, 0},
	// drawseg base pointer at various locations
	{0x00033596, CODE_HOOK | HOOK_UINT32, 0}, // R_ClearDrawSegs
	{0x000383e4, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawSprite
	{0x00038561, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawSprite
	{0x0003861f, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawMasked
	{0x0003863d, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawMasked
};

// expanded visplane limit
static hook_t hook_visplane[] =
{
	// visplane base pointer at various locations
	{0x000361f1, CODE_HOOK | HOOK_UINT32, 0}, // R_ClearPlanes
	{0x0003628e, CODE_HOOK | HOOK_UINT32, 0}, // R_FindPlane
	{0x000362bb, CODE_HOOK | HOOK_UINT32, 0}, // R_FindPlane
	{0x000364fe, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawPlanes
	{0x00036545, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawPlanes
	// visplane max count at various locations
	{0x000362d1, CODE_HOOK | HOOK_UINT32, 0}, // R_FindPlane
	{0x0003650f, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawPlanes
};

// expanded vissprite limit
static hook_t hook_vissprite[] =
{
	// vissprite base pointer at various locations
	{0x00037a66, CODE_HOOK | HOOK_UINT32, 0}, // R_ClearSprites
	{0x000382c0, CODE_HOOK | HOOK_UINT32, 0}, // R_SortVisSprites
	{0x000382e4, CODE_HOOK | HOOK_UINT32, 0}, // R_SortVisSprites
	{0x0003830c, CODE_HOOK | HOOK_UINT32, 0}, // R_SortVisSprites
	{0x00038311, CODE_HOOK | HOOK_UINT32, 0}, // R_SortVisSprites
	{0x000385ee, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawMasked
	// vissprite max pointer
	{0x00037e5a, CODE_HOOK | HOOK_UINT32, 0}, // R_ProjectSprite
};

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'R_RenderPlayerView' in 'D_Display'
	{0x0001D361, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_RenderPlayerView},
	// replace call to 'R_SetupFrame' in 'R_RenderPlayerView'
	{0x00035FB0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)custom_SetupFrame},
	// replace call to 'R_DrawPlayerSprites' in 'R_DrawMasked'
	{0x0003864C, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)custom_DrawPlayerSprites},
	// replace 'yslope' calculation in 'R_ExecuteSetViewSize'
	{0x00035C10, CODE_HOOK | HOOK_UINT32, 0xFFFFFFB8},
	{0x00035C14, CODE_HOOK | HOOK_UINT16, 0xA37F},
	{0x00035C16, CODE_HOOK | HOOK_UINT32, (uint32_t)&mlook_pitch},
	{0x00035C1A, CODE_HOOK | HOOK_UINT16, 0x5AEB},
	// allow screen size over 11
	{0x00035A8A, CODE_HOOK | HOOK_UINT8, 0x7C},
	{0x00022D2A, CODE_HOOK | HOOK_UINT8, 10},
	{0x000235F0, CODE_HOOK | HOOK_UINT8, 10},
	// call 'R_RenderPlayerView' even in automap
	{0x0001D32B, CODE_HOOK | HOOK_UINT16, 0x07EB},
	// disable "drawseg overflow" error in 'R_DrawPlanes'
	{0x000364C9, CODE_HOOK | HOOK_UINT16, 0x2BEB},
	// required variables
	{0x0003952C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&centeryfrac},
	{0x00039534, DATA_HOOK | HOOK_IMPORT, (uint32_t)&centerx},
	{0x00039538, DATA_HOOK | HOOK_IMPORT, (uint32_t)&centery},
	{0x00032310, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewwindowx},
	{0x00032314, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewwindowy},
	{0x0004F920, DATA_HOOK | HOOK_IMPORT, (uint32_t)&yslope},
	{0x00032304, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewheight},
	{0x0003230C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewwidth},
	{0x0002B698, DATA_HOOK | HOOK_IMPORT, (uint32_t)&screenblocks},
	{0x00074FC0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&usegamma},
	// import render variables
	{0x00039010, DATA_HOOK | HOOK_IMPORT, (uint32_t)&colfunc},
	{0x000322EC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&dc_x},
	{0x000322F8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&dc_yl},
	{0x000322F4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&dc_yh},
	{0x000322E8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&dc_source},
	{0x0005C888, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sprtopscreen},
	{0x0005C8A0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spryscale},
	{0x0005C890, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mfloorclip},
	{0x0005C88C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mceilingclip},
};

