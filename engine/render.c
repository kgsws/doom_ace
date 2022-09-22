// kgsws' ACE Engine
////
// This handles both game and mod configuration.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "render.h"

uint32_t *viewheight;
uint32_t *viewwidth;

static fixed_t *centery;
static fixed_t *centeryfrac;
static fixed_t *yslope;

static fixed_t cy_weapon;
static fixed_t cy_look;

static fixed_t mlook_pitch;

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void custom_SetupFrame(player_t *pl)
{
	fixed_t pn = finesine[pl->mo->pitch >> ANGLETOFINESHIFT] / 410; // TODO: depends on view height

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
	{0x00039538, DATA_HOOK | HOOK_IMPORT, (uint32_t)&centery},
	{0x0004F920, DATA_HOOK | HOOK_IMPORT, (uint32_t)&yslope},
	{0x00032304, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewheight},
	{0x0003230C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewwidth},
};

