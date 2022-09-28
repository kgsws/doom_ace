// kgsws' ACE Engine
////
// This handles both game and mod configuration.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "config.h"
#include "player.h"
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

uint8_t r_palette[768];

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
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void custom_SetupFrame(player_t *pl)
{
	fixed_t pn;

	if(extra_config.mouse_look > 1)
		pn = 0;
	else
		pn = finetangent[(pl->mo->pitch + ANG90) >> ANGLETOFINESHIFT] / 410; // TODO: depends on view height

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

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'R_SetupFrame' in 'R_RenderPlayerView'
	{0x00035FB0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)custom_SetupFrame},
	// replace call to 'R_DrawPlayerSprites' in 'R_DrawMasked'
	{0x0003864C, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)custom_DrawPlayerSprites},
	// replace 'yslope' calculation in 'R_ExecuteSetViewSize'
	{0x00035C10, CODE_HOOK | HOOK_UINT32, 0xFFFFFFB8},
	{0x00035C14, CODE_HOOK | HOOK_UINT16, 0xA37F},
	{0x00035C16, CODE_HOOK | HOOK_UINT32, (uint32_t)&mlook_pitch},
	{0x00035C1A, CODE_HOOK | HOOK_UINT16, 0x5AEB},
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

