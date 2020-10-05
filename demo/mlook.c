// kgsws' Doom ACE
// Mouse look. No aiming yet.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "mlook.h"

#define MLOOK_TOP	(180 << FRACBITS)
#define MLOOK_BOT	(-180 << FRACBITS)

// TODO: proper mouse aiming
// probably by abusing 'chatchar' in ticcmd

static void custom_SetupFrame(player_t*) __attribute((regparm(1),no_caller_saved_registers));
static void custom_DrawPlayerSprites() __attribute((no_caller_saved_registers));
static void custom_BuildTiccmd(ticcmd_t*) __attribute((regparm(1),no_caller_saved_registers));

int32_t *mousey;

fixed_t *centery;
fixed_t *centeryfrac;
fixed_t *yslope;

fixed_t cy_weapon;
fixed_t cy_look;

static hook_t hook_list[] =
{
	// custom R_SetupFrame
	{0x00035fb1, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)custom_SetupFrame},
	// custom R_DrawPlayerSprites
	{0x0003864d, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)custom_DrawPlayerSprites},
	// custom G_BuildTiccmd
	{0x0001d5b3, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)custom_BuildTiccmd},
	{0x0001f221, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)custom_BuildTiccmd},
	// required variables
	{0x0003952C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&centeryfrac},
	{0x00039538, DATA_HOOK | HOOK_IMPORT, (uint32_t)&centery},
	{0x0004F920, DATA_HOOK | HOOK_IMPORT, (uint32_t)&yslope},
	{0x0002b33c, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mousey},
	// terminator
	{0}
};

void mlook_init()
{
	utils_install_hooks(hook_list);
}

static __attribute((regparm(1),no_caller_saved_registers))
void custom_BuildTiccmd(ticcmd_t *cmd)
{
	// place for mouse look offset; only temporary solution
	register fixed_t mlookpos = players[*consoleplayer].frags[3];
	mlookpos += *mousey << (FRACBITS - 3);
	if(mlookpos > MLOOK_TOP)
		mlookpos = MLOOK_TOP;
	if(mlookpos < MLOOK_BOT)
		mlookpos = MLOOK_BOT;
	players[*consoleplayer].frags[3] = mlookpos;

	// disable mouse Y for original function
	*mousey = 0;
	G_BuildTiccmd(cmd);
}

static __attribute((regparm(1),no_caller_saved_registers))
void custom_SetupFrame(player_t *pl)
{
	static fixed_t pitch;
	fixed_t pn = pl->frags[3] >> FRACBITS;

	if(pitch != pn)
	{
		fixed_t cy = *viewheight / 2;
		pitch = pn;

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
			yslope[i] = FixedDiv((*viewwidth << *detailshift) / 2*FRACUNIT, dy);
		}		
	}

	// and now run the original
	R_SetupFrame(pl);
}

static __attribute((no_caller_saved_registers))
void custom_DrawPlayerSprites()
{
	*centery = cy_weapon;
	*centeryfrac = cy_weapon << FRACBITS;

	R_DrawPlayerSprites();

	*centery = cy_look;
	*centeryfrac = cy_look << FRACBITS;
}

