// kgsws' ACE Engine
////
// This is a fullscreen status bar.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "dehacked.h"
#include "stbar.h"

static uint32_t *screenblocks;

static patch_t **tallnum;
static patch_t **tallpercent;
static patch_t **keygfx;

static uint32_t tallnum_height;
static uint32_t tallnum_width;

static uint32_t stbar_y;
static uint32_t stbar_hp_x;
static uint32_t stbar_ar_x;

static uint32_t ammo_value;

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
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void hook_RenderPlayerView(player_t *pl)
{
	uint32_t tmp;

	// actually render 3D view
	R_RenderPlayerView(pl);

	// status bar on top
	if(*screenblocks != 11)
		return;
	if(pl->playerstate)
		return;

	// health
	stbar_big_number_r(stbar_hp_x, stbar_y, pl->health, 3);
	V_DrawPatchDirect(stbar_hp_x, stbar_y, 0, *tallpercent);

	// armor
	if(pl->armorpoints)
	{
		stbar_big_number_r(stbar_ar_x, stbar_y, pl->armorpoints, 3);
		V_DrawPatchDirect(stbar_ar_x, stbar_y, 0, *tallpercent);
	}
/*
	// AMMO
	if(deh_weaponinfo[pl->readyweapon].ammo < NUMAMMO)
		stbar_big_number_r(SCREENWIDTH - 4, stbar_y, pl->ammo[deh_weaponinfo[pl->readyweapon].ammo], -4);

	// keys
	tmp = 1;
	for(uint32_t i = 0; i < 3; i++)
	{
		if(pl->cards[i] || pl->cards[i+3])
		{
			V_DrawPatchDirect(SCREENWIDTH - 8, tmp, 0, keygfx[i]);
			tmp += 8;
		}
	}
*/
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
	// some variables
	{0x0002B698, DATA_HOOK | HOOK_IMPORT, (uint32_t)&screenblocks},
	{0x000752f0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallnum},
	{0x00075458, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallpercent},
	{0x00075458, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallpercent},
	{0x00075208, DATA_HOOK | HOOK_IMPORT, (uint32_t)&keygfx},
};

