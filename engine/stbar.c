// kgsws' ACE Engine
////
// This is a fullscreen status bar.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "decorate.h"
#include "stbar.h"
#include "inventory.h"

#define STBAR_Y	(SCREENHEIGHT-2)

#define NOT_A_NUMBER	0x7FFFFFFF
#define MAX_KEY_ICONS	(6*6)

//

static uint32_t *screenblocks;

static patch_t **tallnum;
static patch_t **tallpercent;

static uint32_t tallnum_height;
static uint32_t tallnum_width;

static uint32_t stbar_y;
static uint32_t stbar_hp_x;
static uint32_t stbar_ar_x;

static uint32_t ammo_value;

static uint16_t *ammo_pri;
static uint16_t *ammo_sec;

static mobjinfo_t *keyinv[MAX_KEY_ICONS];

//
// draw

static void stbar_big_number_r(int x, int y, int value, int digits)
{
	if(digits < 0)
		// negative digit count means also show zero
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

//
// init

static __attribute((regparm(2),no_caller_saved_registers))
void hook_st_init()
{
	// call original first
	ST_Init();

	// initialize
	doom_printf("[ACE] init status bar\n");

	tallnum_height = tallnum[0]->height;
	tallnum_width = tallnum[0]->width;

	stbar_y = STBAR_Y - tallnum_height;
	stbar_hp_x = 4 + tallnum_width * 3;
	stbar_ar_x = stbar_hp_x * 2 + tallnum_width + 4;

	// cache all icons
	for(uint32_t i = 0; i < num_mobj_types; i++)
	{
		mobjinfo_t *info = mobjinfo + i;
		int32_t lump;

		if(!inventory_is_valid(info))
			continue;

		lump = (int32_t)info->inventory.icon;
		info->inventory.icon = W_CacheLumpNum(lump, PU_CACHE);
	}
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void hook_RenderPlayerView(player_t *pl)
{
	uint32_t tx, ty, cc, cm;

	// actually render 3D view
	R_RenderPlayerView(pl);

	// status bar on top
	if(*screenblocks != 11)
	{
		// update ammo for old status bar
		if(ammo_pri)
			ammo_value = *ammo_pri;
		else
		if(ammo_sec)
			ammo_value = *ammo_sec;
		else
			ammo_value = NOT_A_NUMBER;
		// and do nothing
		return;
	}

	if(pl->playerstate != PST_LIVE)
	{
		ammo_value = NOT_A_NUMBER;
		return;
	}

	// health
	stbar_big_number_r(stbar_hp_x, stbar_y, pl->health, 3);
	V_DrawPatchDirect(stbar_hp_x, stbar_y, 0, *tallpercent);

	// armor
	if(pl->armorpoints)
	{
		stbar_big_number_r(stbar_ar_x, stbar_y, pl->armorpoints, 3);
		V_DrawPatchDirect(stbar_ar_x, stbar_y, 0, *tallpercent);
	}

	// AMMO
	ty = stbar_y;
	if(ammo_pri)
	{
		stbar_big_number_r(SCREENWIDTH - 4, ty, *ammo_pri, -4);
		ty -= tallnum_height;
	}
	if(ammo_sec)
	{
		stbar_big_number_r(SCREENWIDTH - 4, ty, *ammo_sec, -4);
		ty -= tallnum_height;
	}

	// keys
	cm = 0;
	cc = 0;
	tx = SCREENWIDTH - 1;
	ty = 1;
	for(uint32_t i = 0; i < MAX_KEY_ICONS; i++)
	{
		patch_t *patch;
		int16_t ox, oy; // hack for right align

		if(!keyinv[i])
			break;

		patch = keyinv[i]->inventory.icon;

		ox = patch->x;
		oy = patch->y;
		patch->x = patch->width;
		patch->y = 0;

		V_DrawPatchDirect(tx, ty, 0, patch);

		patch->x = ox;
		patch->y = oy;

		ty += patch->height + 1;

		if(patch->width > cm)
			cm = patch->width;

		cc++;
		if(cc >= 6)
		{
			tx -= cm + 1;
			ty = 1;
			cc = 0;
			cm = 0;
		}
	}
}

//
// update

static void update_keys(player_t *pl)
{
	uint32_t idx = 0;

	for(uint32_t i = 0; i < num_mobj_types; i++)
	{
		mobjinfo_t *info = mobjinfo + i;

		if(info->extra_type != ETYPE_KEY)
			continue;

		if(!info->inventory.icon)
			continue;

		if(!inventory_check(pl->mo, i))
			continue;

		keyinv[idx++] = info;

		if(idx >= MAX_KEY_ICONS)
			break;
	}

	if(idx < MAX_KEY_ICONS)
		keyinv[idx] = NULL;
}

static void update_weapon(player_t *pl)
{
	// weapon changed
	mobjinfo_t *info = pl->readyweapon;
	inventory_t *inv;

	if(!info)
		return;

	// primary ammo
	ammo_pri = NULL;
	if(info->weapon.ammo_type[0])
	{
		inv = inventory_find(pl->mo, info->weapon.ammo_type[0]);
		if(inv)
			ammo_pri = &inv->count;
	}

	// secondary ammo
	ammo_sec = NULL;
	if(info->weapon.ammo_type[1])
	{
		inv = inventory_find(pl->mo, info->weapon.ammo_type[1]);
		if(inv)
			ammo_sec = &inv->count;
	}
}

//
// API

void stbar_update(player_t *pl)
{
	if(pl->stbar_update & STU_WEAPON)
		update_weapon(pl);

	if(pl->stbar_update & STU_KEYS)
		update_keys(pl);

	pl->stbar_update = 0;
}

void stbar_start(player_t *pl)
{
	// original status bar
	ST_Start();

	// update weapon ammo
	update_weapon(pl);

	// clear keys
	memset(keyinv, 0, sizeof(keyinv));
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// hook status bar init
	{0x0001E950, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_st_init},
	// hook 3D render
	{0x0001D361, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_RenderPlayerView},
	// replace ammo pointer in normal status bar
	{0x0003ABE2, CODE_HOOK | HOOK_UINT8, 0xB8},
	{0x0003ABE3, CODE_HOOK | HOOK_UINT32, (uint32_t)&ammo_value},
	{0x0003ABE7, CODE_HOOK | HOOK_UINT16, 0x0DEB},
	{0x0003ABFB, CODE_HOOK | HOOK_SET_NOPS, 2},
	{0x0003A282, CODE_HOOK | HOOK_UINT16, 0x3FEB},
	// change 'not-a-number' value in 'STlib_drawNum'
	{0x0003B0D3, CODE_HOOK | HOOK_UINT32, NOT_A_NUMBER},
	// some variables
	{0x0002B698, DATA_HOOK | HOOK_IMPORT, (uint32_t)&screenblocks},
	{0x000752f0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallnum},
	{0x00075458, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallpercent},
	{0x00075458, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallpercent},
	// allow screen size over 11 // TODO: move to 'render'
	{0x00035A8A, CODE_HOOK | HOOK_UINT8, 0x7C},
	{0x00022D2A, CODE_HOOK | HOOK_UINT8, 9},
	{0x000235F0, CODE_HOOK | HOOK_UINT8, 9},
};

