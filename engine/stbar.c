// kgsws' ACE Engine
////
// This is a fullscreen status bar.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "decorate.h"
#include "inventory.h"
#include "wadfile.h"
#include "stbar.h"

#define STBAR_Y	(SCREENHEIGHT-2)
#define ST_Y	(SCREENHEIGHT-32)
#define BG	4
#define FG	0

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

static uint8_t do_evil_grin;

static st_number_t *w_ready;
static st_number_t *w_ammo;
static st_number_t *w_maxammo;
static st_multicon_t *w_arms;

static int32_t *keyboxes;

static uint16_t *ammo_pri;
static uint16_t *ammo_sec;

static mobjinfo_t *keyinv[MAX_KEY_ICONS];

//
// icon cache

static patch_t *get_icon_ptr(int32_t lump)
{
	if((*lumpcache)[lump])
		// already cachced, do not change TAG
		return (*lumpcache)[lump];

	// cache new
	return W_CacheLumpNum(lump, PU_CACHE);
}

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
	uint32_t tx, ty, cc, cm;

	// actually render 3D view
	R_RenderPlayerView(pl);

	// status bar on top
	if(*screenblocks != 11)
		return;

	if(pl->playerstate != PST_LIVE)
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

	// AMMO
	ty = stbar_y;
	if(ammo_pri)
	{
		stbar_big_number_r(SCREENWIDTH - 4, ty, *ammo_pri, -4);
		ty -= tallnum_height + 1;
	}
	if(ammo_sec)
		stbar_big_number_r(SCREENWIDTH - 4, ty, *ammo_sec, -4);

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

		patch = get_icon_ptr(keyinv[i]->inventory.icon);

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

static void update_weapon(player_t *pl)
{
	static uint32_t val[6];

	for(uint32_t i = 0; i < 6; i++)
	{
		uint16_t *ptr;

		w_arms[i].inum = val + (i);
		val[i] = 0;

		ptr = pl->mo->info->player.wpn_slot[i + 2];
		if(!ptr)
			continue;

		while(*ptr)
		{
			uint16_t type = *ptr++;
			if(inventory_check(pl->mo, type))
			{
				val[i] = 1;
				break;
			}
		}
	}
}

static void update_keys(player_t *pl)
{
	uint32_t idx = 0;

	for(uint32_t i = 0; i < 3; i++)
		keyboxes[i] = -1;

	for(uint32_t i = 0; i < num_mobj_types; i++)
	{
		mobjinfo_t *info = mobjinfo + i;

		if(info->extra_type != ETYPE_KEY)
			continue;

		if(!info->inventory.icon)
			continue;

		if(!inventory_check(pl->mo, i))
			continue;

		// original status bar
		switch(i)
		{
			case 47:
				keyboxes[0] = 0;
			break;
			case 48:
				keyboxes[2] = 2;
			break;
			case 49:
				keyboxes[1] = 1;
			break;
			case 50:
				keyboxes[1] = 4;
			break;
			case 51:
				keyboxes[2] = 5;
			break;
			case 52:
				keyboxes[0] = 3;
			break;
		}

		// new status bar
		keyinv[idx++] = info;

		if(idx >= MAX_KEY_ICONS)
			break;
	}

	if(idx < MAX_KEY_ICONS)
		keyinv[idx] = NULL;
}

static void update_backpack(player_t *pl)
{
	uint32_t mult;
	static uint16_t maxammo[NUMAMMO];

	mult = pl->backpack ? 2 : 1;

	for(uint32_t i = 0; i < NUMAMMO; i++)
	{
		w_maxammo[i].num = maxammo + i;
		maxammo[i] = ((uint32_t*)(0x00012D70 + doom_data_segment))[i] * mult; // maxammo
	}
}

static void update_inventory(player_t *pl)
{

}

static void update_ready_weapon(player_t *pl)
{
	// weapon changed
	mobjinfo_t *info = pl->readyweapon;
	inventory_t *item;

	ammo_pri = NULL;
	ammo_sec = NULL;
	w_ready->num = NULL;

	if(!info)
		return;

	// primary ammo
	if(info->weapon.ammo_type[0])
	{
		item = inventory_find(pl->mo, info->weapon.ammo_type[0]);
		if(item)
			ammo_pri = &item->count;
	}

	// secondary ammo
	if(info->weapon.ammo_type[1])
	{
		item = inventory_find(pl->mo, info->weapon.ammo_type[1]);
		if(item)
			ammo_sec = &item->count;
	}
	if(ammo_sec == ammo_pri)
		ammo_sec = NULL;

	// old stbar ammo pointer
	w_ready->num = ammo_pri ? ammo_pri : ammo_sec;
}

//
// API

void stbar_update(player_t *pl)
{
	if(pl->stbar_update & STU_WEAPON)
		update_weapon(pl);

	if(pl->stbar_update & STU_KEYS)
		update_keys(pl);

	if(pl->stbar_update & STU_BACKPACK)
		update_backpack(pl);

	if(pl->stbar_update & STU_INVENTORY)
		update_inventory(pl);

	if(pl->stbar_update & STU_WEAPON_NOW)
		update_ready_weapon(pl);

	if(pl->stbar_update & STU_WEAPON_NEW)
		do_evil_grin = 1;

	pl->stbar_update = 0;
}

void stbar_start(player_t *pl)
{
	inventory_t *item;

	// original status bar
	ST_Start();

	// setup (original) ammo
	item = inventory_find(pl->mo, 63); // Clip
	if(item)
		w_ammo[0].num = &item->count;
	item = inventory_find(pl->mo, 69); // Shell
	if(item)
		w_ammo[1].num = &item->count;
	item = inventory_find(pl->mo, 67); // RocketAmmo
	if(item)
		w_ammo[2].num = &item->count;
	item = inventory_find(pl->mo, 65); // Cell
	if(item)
		w_ammo[3].num = &item->count;

	// update weapon slots
	update_weapon(pl);

	// update (original) max ammo
	update_backpack(pl);

	// update current weapon ammo
	update_ready_weapon(pl);

	// clear keys
	memset(keyinv, 0, sizeof(keyinv));
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void st_draw_num(st_number_t *st, uint32_t refresh)
{
	uint32_t w, h;
	int32_t x;
	uint32_t value, len;

	// actually do a differential draw
	if(!refresh)
	{
		if(st->num)
		{
			// valid number
			if(*st->num == st->oldnum)
				return;
			st->oldnum = *st->num;
		} else
		{
			// not a number
			if(st->oldnum == 0xFFFFFFFF)
				return;
			st->oldnum = 0xFFFFFFFF;
		}
	}

	// no more negative numbers

	len = st->width;
	w = st->p[0]->width;
	h = st->p[0]->height;

	x = st->x - len * w;
	V_CopyRect(x, st->y - ST_Y, BG, len * w, h, x, st->y, FG);

	if(!st->num)
		return;

	value = *st->num;
	x = st->x;

	if(!value)
	{
		V_DrawPatch(x - w, st->y, FG, st->p[0]);
		return;
	}

	while(value && len--)
	{
		x -= w;
		V_DrawPatch(x, st->y, FG, st->p[value % 10]);
		value /= 10;
	}
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// hook status bar init
	{0x0001E950, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_st_init},
	// replace 'STlib_drawNum'
	{0x0003B020, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)st_draw_num},
	// hook 3D render
	{0x0001D361, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_RenderPlayerView},
	// remove ammo pointer in normal status bar
	{0x0003ABE2, CODE_HOOK | HOOK_UINT32, 0x10EBC031},
	{0x0003ABFB, CODE_HOOK | HOOK_SET_NOPS, 2},
	{0x0003A282, CODE_HOOK | HOOK_UINT16, 0x3FEB},
	// disable 'keyboxes' in original status bar
	{0x0003A2DB, CODE_HOOK | HOOK_UINT16, 0x48EB},
	// fix evil grin
	{0x00039FD4, CODE_HOOK | HOOK_UINT8, 0xB8},
	{0x00039FD5, CODE_HOOK | HOOK_UINT32, (uint32_t)&do_evil_grin},
	{0x00039FD9, CODE_HOOK | HOOK_UINT32, 0xDB84188A},
	{0x00039FDD, CODE_HOOK | HOOK_UINT32, 0x08FE5674},
	{0x00039FE1, CODE_HOOK | HOOK_UINT16, 0x2FEB},
	// some variables
	{0x0002B698, DATA_HOOK | HOOK_IMPORT, (uint32_t)&screenblocks},
	{0x000752f0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallnum},
	{0x00075458, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallpercent},
	{0x000751C8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&w_ready},
	{0x00075070, DATA_HOOK | HOOK_IMPORT, (uint32_t)&w_ammo},
	{0x00074FF0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&w_maxammo},
	{0x000750F0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&w_arms},
	{0x000753C0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&keyboxes},
	// allow screen size over 11 // TODO: move to 'render'
	{0x00035A8A, CODE_HOOK | HOOK_UINT8, 0x7C},
	{0x00022D2A, CODE_HOOK | HOOK_UINT8, 9},
	{0x000235F0, CODE_HOOK | HOOK_UINT8, 9},
};

