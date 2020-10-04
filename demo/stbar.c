// kgsws' Doom ACE
// Full screen status bar.
#include "engine.h"
#include "utils.h"
#include "defs.h"

#define STBAR_Y	(SCREENHEIGHT-2)	// ending of status bar

uint32_t *screenblocks;

static patch_t **tallnum;
static patch_t *tallpercent;

static int tallnum_height;
static int tallnum_width;

// precalculated positions
static int stbar_y;
static int stbar_hp_x;
static int stbar_ar_x;

static hook_t hook_list[] =
{
	{0x0002B698, DATA_HOOK | HOOK_IMPORT, (uint32_t)&screenblocks},
	{0x000752f0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallnum},
	{0x00075458, DATA_HOOK | HOOK_READ32, (uint32_t)&tallpercent},
	// terminator
	{0}
};

//
//

void stbar_big_number_r(int x, int y, int value, int digits)
{
	if(digits < 0)
		digits = -digits;
	else
		if(!value)
			return;

	if(value < 0) // TODO: minus
		value = -value;

	while(digits--)
	{
		x -= tallnum_width;
		V_DrawPatchDirect(x, y, 0, tallnum[value % 10]);
		value /= 10;
		if(!value)
			return;
	}
}

void stbar_init()
{
	// initialize variables
	utils_install_hooks(hook_list);

	tallnum_height = tallnum[0]->height;
	tallnum_width = tallnum[0]->width;

	stbar_y = STBAR_Y - tallnum_height;
	stbar_hp_x = 4 + tallnum_width * 3;
	stbar_ar_x = stbar_hp_x * 2 + tallnum_width + 4;
}

void stbar_draw(player_t *pl)
{
	if(*screenblocks < 11)
		return;

	if(pl->playerstate != PST_LIVE)
		return;

	stbar_big_number_r(stbar_hp_x, stbar_y, pl->health, 3);
	V_DrawPatchDirect(stbar_hp_x, stbar_y, 0, tallpercent);

	if(pl->armorpoints)
	{
		stbar_big_number_r(stbar_ar_x, stbar_y, pl->armorpoints, 3);
		V_DrawPatchDirect(stbar_ar_x, stbar_y, 0, tallpercent);
	}

	if(pl->readyweapon < NUMWEAPONS)
	{
		uint32_t ammo = weaponinfo[pl->readyweapon].ammo;
		if(ammo < NUMAMMO)
			stbar_big_number_r(SCREENWIDTH - 4, stbar_y, pl->ammo[ammo], 4);
	}
}

