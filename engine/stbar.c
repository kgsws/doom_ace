// kgsws' ACE Engine
////
// This is a fullscreen status bar.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "decorate.h"
#include "inventory.h"
#include "wadfile.h"
#include "config.h"
#include "player.h"
#include "map.h"
#include "mobj.h"
#include "render.h"
#include "draw.h"
#include "stbar.h"

#define STBAR_Y	(SCREENHEIGHT-2)
#define ST_Y	(SCREENHEIGHT-32)
#define BG	4
#define FG	0

#define INVBAR_Y	(SCREENHEIGHT-16)
#define INVBAR_STEP	32
#define INVBAR_COUNT	7
#define INVBAR_CENTER	(SCREENWIDTH / 2)
#define INVBAR_START	((INVBAR_CENTER - ((INVBAR_COUNT * INVBAR_STEP) / 2)) + (INVBAR_STEP / 2))
#define INVBAR_END	(INVBAR_START + INVBAR_COUNT * INVBAR_STEP)

#define NOT_A_NUMBER	0x7FFFFFFF
#define MAX_KEY_ICONS	(6*6)

//

uint_fast8_t stbar_refresh_force;

static uint_fast8_t invbar_was_on;

static uint16_t stbar_t_y;
static uint16_t stbar_t_hp_x;
static uint16_t stbar_t_ar_x;
static uint16_t stbar_s_y;
static uint16_t stbar_s_hp_x;
static uint16_t stbar_s_ar_x;

static uint8_t do_evil_grin;

static int32_t inv_box;
static int32_t inv_sel;

static uint16_t *ammo_pri;
static uint16_t *ammo_sec;

static mobjinfo_t *keyinv[MAX_KEY_ICONS];

static patch_t *xhair;
static patch_t *xhair_custom;

static uint32_t fps_value;
static uint32_t fps_diff;
static uint32_t last_gt;

static uint16_t ammo_dispmax[NUMAMMO] = {666};

static const uint8_t keyboxlock[] = {2, 3, 1, 5, 6, 4};

//
// crosshairs

typedef struct
{
	uint16_t width;
	uint16_t height;
	int16_t ox, oy;
	uint32_t offs[5];
	uint8_t dA[6];
	uint8_t dB[6];
	uint8_t dC[6];
	uint8_t dD[11];
	uint8_t dE[13];
	uint8_t dF[11];
	uint8_t dG[11];
} __attribute__((packed)) xhair_patch_t;

static xhair_patch_t xhair_data =
{
	.dA = {0x00, 0x01, 0x04, 0x04, 0x04, 0xFF},
	.dB = {0x01, 0x01, 0x04, 0x04, 0x04, 0xFF},
	.dC = {0x02, 0x01, 0x04, 0x04, 0x04, 0xFF},
	.dD = {0x00, 0x01, 0x04, 0x04, 0x04, 0x02, 0x01, 0x04, 0x04, 0x04, 0xFF},
	.dE = {0x00, 0x02, 0x04, 0x04, 0x04, 0x04, 0x03, 0x02, 0x04, 0x04, 0x04, 0x04, 0xFF},
	.dF = {0x01, 0x01, 0x04, 0x04, 0x04, 0x03, 0x01, 0x04, 0x04, 0x04, 0xFF},
	.dG = {0x00, 0x01, 0x04, 0x04, 0x04, 0x04, 0x01, 0x04, 0x04, 0x04, 0xFF},
};

//
// icon cache

static patch_t *get_icon_ptr(int32_t lump)
{
	if(lumpcache[lump])
		// already cachced, do not change TAG
		return lumpcache[lump];

	// cache new
	return W_CacheLumpNum(lump, PU_CACHE);
}

//
// draw

static void stbar_draw_number_r(int32_t x, int32_t y, int32_t value, int32_t digits, patch_t **numfont)
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
		x -= numfont[0]->width;
		V_DrawPatchDirect(x, y, numfont[value % 10]);
		value /= 10;
		if(!value)
			return;
	}
}

static void stbar_draw_center(int32_t x, int32_t y, int32_t lump)
{
	patch_t *patch;
	int16_t ox, oy;

	patch = get_icon_ptr(lump);

	ox = patch->x;
	oy = patch->y;
	patch->x = patch->width / 2;
	patch->y = patch->height / 2;

	V_DrawPatchDirect(x, y, patch);

	patch->x = ox;
	patch->y = oy;
}

static void sbar_draw_invslot(int32_t x, int32_t y, mobjinfo_t *info, invitem_t *item)
{
	stbar_draw_center(x, y, info->inventory.icon);
	if(!item->count || info->inventory.max_count > 1)
		stbar_draw_number_r(x + (INVBAR_STEP / 2) - 1, y - shortnum[0]->height + (INVBAR_STEP / 2) - 2, item->count, 5, shortnum);
}

//
// init

void stbar_init()
{
	uint32_t width;
	uint32_t height;
	int32_t lump;

	// initialize
	doom_printf("[ACE] init status bar\n");

	width = tallnum[0]->width;
	height = tallnum[0]->height;

	stbar_t_y = STBAR_Y - height;
	stbar_t_hp_x = 4 + width * 3;
	stbar_t_ar_x = stbar_t_hp_x * 2 + width + 4;

	width = shortnum[0]->width;
	height = shortnum[0]->height;

	stbar_s_y = STBAR_Y - height;
	stbar_s_hp_x = 4 + width * 3;
	stbar_s_ar_x = stbar_s_hp_x * 2 + width + 4;

	// inventory
	inv_box = W_CheckNumForName("ARTIBOX");
	if(inv_box < 0)
		inv_box = W_GetNumForName("STFB1");
	inv_sel = W_CheckNumForName("SELECTBO");
	if(inv_sel < 0)
		inv_sel = W_GetNumForName("STFB0");

	// custom crosshair
	lump = W_CheckNumForName("XHAIR");
	if(lump >= 0)
		xhair_custom = W_CacheLumpNum(lump, PU_STATIC);

	// old status bar ammo
	for(uint32_t i = 0; i < 4; i++)
	{
		lump = mobj_check_type(mod_config.ammo_type[i]);
		if(lump < 0)
			goto ammo_error;
		mod_config.ammo_type[i] = lump;
		if(mobjinfo[mod_config.ammo_type[i]].extra_type != ETYPE_AMMO)
			goto ammo_error;
	}

	//
	return;

ammo_error:
	engine_error("STBAR", "Invalid ammo type.");
	return;
}

//
// key overlay

static inline void draw_keybar(player_t *pl)
{
	uint32_t tx, ty, cc, cm;

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

		V_DrawPatchDirect(tx, ty, patch);

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
// fullscreen status bar

static inline void draw_full_stbar(player_t *pl)
{
	uint32_t ty;
	uint16_t stbar_y, stbar_hp_x, stbar_ar_x;
	patch_t **numfont;

	if(screenblocks < 11)
		return;

	if(screenblocks > 12)
		return;

	// keys overlay
	draw_keybar(pl);

	if(screenblocks > 11)
	{
		stbar_y = stbar_s_y;
		stbar_hp_x = stbar_s_hp_x;
		stbar_ar_x = stbar_s_ar_x;
		numfont = shortnum;
	} else
	{
		stbar_y = stbar_t_y;
		stbar_hp_x = stbar_t_hp_x;
		stbar_ar_x = stbar_t_ar_x;
		numfont = tallnum;

		V_DrawPatchDirect(stbar_hp_x, stbar_y, tallpercent);
		if(pl->armorpoints)
			V_DrawPatchDirect(stbar_ar_x, stbar_y, tallpercent);
	}

	// health
	stbar_draw_number_r(stbar_hp_x, stbar_y, pl->health, 3, numfont);

	// armor
	if(pl->armorpoints)
		stbar_draw_number_r(stbar_ar_x, stbar_y, pl->armorpoints, 3, numfont);

	// AMMO
	ty = stbar_y;
	if(ammo_pri)
	{
		stbar_draw_number_r(SCREENWIDTH - 4, ty, *ammo_pri, -4, numfont);
		ty -= numfont[0]->height + 1;
	}
	if(ammo_sec)
		stbar_draw_number_r(SCREENWIDTH - 4, ty, *ammo_sec, -4, numfont);
}

//
// inventory bar

static void draw_invbar(player_t *pl)
{
	if(pl->inv_tick)
	{
		mobjinfo_t *info;
		mobj_t *mo = pl->mo;
		uint32_t idx;
		int32_t item;

		if(screenblocks < 11)
			invbar_was_on = 1;

		for(uint32_t x = INVBAR_START; x < INVBAR_END; x += INVBAR_STEP)
			stbar_draw_center(x, INVBAR_Y, inv_box);
		stbar_draw_center(INVBAR_CENTER, INVBAR_Y, inv_sel);

		if(pl->inv_sel >= 0 && mo->inventory)
		{
			// previous items
			item = pl->inv_sel - 1;
			idx = 0;
			while(item >= 0 && idx < INVBAR_COUNT / 2)
			{
				info = mobjinfo + mo->inventory->slot[item].type;
				if(info->inventory.icon && info->eflags & MFE_INVENTORY_INVBAR)
				{
					sbar_draw_invslot(INVBAR_CENTER - INVBAR_STEP - idx * INVBAR_STEP, INVBAR_Y, info, mo->inventory->slot + item);
					idx++;
				}
				item--;
			}

			// next items
			item = pl->inv_sel + 1;
			idx = 0;
			while(item < mo->inventory->numslots && idx < INVBAR_COUNT / 2)
			{
				info = mobjinfo + mo->inventory->slot[item].type;
				if(info->inventory.icon && info->eflags & MFE_INVENTORY_INVBAR)
				{
					sbar_draw_invslot(INVBAR_CENTER + INVBAR_STEP + idx * INVBAR_STEP, INVBAR_Y, info, mo->inventory->slot + item);
					idx++;
				}
				item++;
			}

			// current selection
			info = mobjinfo + mo->inventory->slot[pl->inv_sel].type;
			sbar_draw_invslot(INVBAR_CENTER, INVBAR_Y, info, mo->inventory->slot + pl->inv_sel);
		}

		return;
	}

	if(screenblocks < 11 && invbar_was_on)
	{
		invbar_was_on = 0;
		stbar_refresh_force = 1;
	}

	if(pl->inv_sel >= 0 && pl->mo->inventory)
	{
		if(screenblocks == 11 && !automapactive)
		{
			// current selection
			invitem_t *item = pl->mo->inventory->slot + pl->inv_sel;
			mobjinfo_t *info = mobjinfo + item->type;
			sbar_draw_invslot(INVBAR_CENTER, INVBAR_Y, info, item);
		}
	}
}

//
// crosshair

static inline void draw_crosshair(player_t *pl)
{
	int32_t y;

	if(!xhair)
		return;

	y = viewwindowy + viewheight / 2;

	if(extra_config.mouse_look > 1)
	{
		int32_t height = viewwindowy + viewheight;
		y -= finetangent[(pl->mo->pitch + ANG90) >> ANGLETOFINESHIFT] / 410;
		if(y < viewwindowy + xhair->y)
			y = viewwindowy + xhair->y;
		else
		if(y > height - xhair->height + xhair->y)
			y = height - xhair->height + xhair->y;
	}

	V_DrawPatchDirect((SCREENWIDTH / 2), y, xhair);
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void hook_draw_stbar(uint32_t fullscreen, uint32_t refresh)
{
	static uint_fast8_t was_in_menu;

	ST_doPaletteStuff();

	if(is_title_map)
	{
		memset(screen_buffer + 53760, r_color_black, 10240);
		return;
	}

	if(automapactive)
		return;

	refresh |= stbar_refresh_force | menuactive | was_in_menu;
	stbar_refresh_force = 0;

	ST_Drawer(fullscreen, refresh);

	was_in_menu = menuactive;
}

//
// update

static void update_ammo(player_t *pl)
{
	static uint16_t zero;
	invitem_t *item;

	item = inventory_find(pl->mo, mod_config.ammo_bullet); // Clip
	if(item)
		w_ammo[0].num = &item->count;
	else
		w_ammo[0].num = &zero;

	item = inventory_find(pl->mo, mod_config.ammo_shell); // Shell
	if(item)
		w_ammo[1].num = &item->count;
	else
		w_ammo[1].num = &zero;

	item = inventory_find(pl->mo, mod_config.ammo_rocket); // RocketAmmo
	if(item)
		w_ammo[3].num = &item->count;
	else
		w_ammo[3].num = &zero;

	item = inventory_find(pl->mo, mod_config.ammo_cell); // Cell
	if(item)
		w_ammo[2].num = &item->count;
	else
		w_ammo[2].num = &zero;
}

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

	for(uint32_t i = 0; i < ORIGINAL_KEY_COUNT / 2; i++)
		keyboxes[i] = -1;

	for(uint32_t i = 0; i < ORIGINAL_KEY_COUNT; i++)
	{
		if(!mobj_check_keylock(pl->mo, keyboxlock[i], 0))
			keyboxes[i % 3] = i;
	}
}

static void update_backpack(player_t *pl)
{
	if(pl->backpack)
	{
		for(uint32_t i = 0; i < 4; i++)
		{
			w_maxammo[i].num = ammo_dispmax + i;
			ammo_dispmax[i] = mobjinfo[mod_config.ammo_type[i]].ammo.max_count;
		}
	} else
	{
		for(uint32_t i = 0; i < 4; i++)
		{
			w_maxammo[i].num = ammo_dispmax + i;
			ammo_dispmax[i] = mobjinfo[mod_config.ammo_type[i]].inventory.max_count;
		}
	}
}

static void update_ready_weapon(player_t *pl)
{
	// weapon changed
	mobjinfo_t *info = pl->readyweapon;
	invitem_t *item;

	ammo_pri = NULL;
	ammo_sec = NULL;
	w_ready.num = NULL;

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
	w_ready.num = ammo_pri ? ammo_pri : ammo_sec;
}

//
// API

void stbar_set_xhair()
{
	uint32_t type;
	uint8_t color;
	uint8_t r, g, b;

	if(extra_config.crosshair_type == 0xFF)
		extra_config.crosshair_type = 6;

	if(extra_config.crosshair_type > 6)
		extra_config.crosshair_type = 0;

	type = extra_config.crosshair_type;

	if(extra_config.mouse_look > 1 && !type)
		type = 1;

	switch(extra_config.crosshair_type)
	{
		case 1:
			// single dot
			xhair_data.width = 1;
			xhair_data.height = 1;
			xhair_data.ox = 0;
			xhair_data.oy = 0;
			xhair_data.offs[0] = offsetof(xhair_patch_t, dA);
		break;
		case 2:
			// + size 3
			xhair_data.width = 3;
			xhair_data.height = 3;
			xhair_data.ox = 1;
			xhair_data.oy = 1;
			xhair_data.offs[0] = offsetof(xhair_patch_t, dB);
			xhair_data.offs[1] = offsetof(xhair_patch_t, dD);
			xhair_data.offs[2] = offsetof(xhair_patch_t, dB);
		break;
		case 3:
			// + size 5
			xhair_data.width = 5;
			xhair_data.height = 5;
			xhair_data.ox = 2;
			xhair_data.oy = 2;
			xhair_data.offs[0] = offsetof(xhair_patch_t, dC);
			xhair_data.offs[1] = offsetof(xhair_patch_t, dC);
			xhair_data.offs[2] = offsetof(xhair_patch_t, dE);
			xhair_data.offs[3] = offsetof(xhair_patch_t, dC);
			xhair_data.offs[4] = offsetof(xhair_patch_t, dC);
		break;
		case 4:
			// x size 3
			xhair_data.width = 3;
			xhair_data.height = 3;
			xhair_data.ox = 1;
			xhair_data.oy = 1;
			xhair_data.offs[0] = offsetof(xhair_patch_t, dD);
			xhair_data.offs[1] = offsetof(xhair_patch_t, dB);
			xhair_data.offs[2] = offsetof(xhair_patch_t, dD);
		break;
		case 5:
			// x size 5
			xhair_data.width = 5;
			xhair_data.height = 5;
			xhair_data.ox = 2;
			xhair_data.oy = 2;
			xhair_data.offs[0] = offsetof(xhair_patch_t, dG);
			xhair_data.offs[1] = offsetof(xhair_patch_t, dF);
			xhair_data.offs[2] = offsetof(xhair_patch_t, dC);
			xhair_data.offs[3] = offsetof(xhair_patch_t, dF);
			xhair_data.offs[4] = offsetof(xhair_patch_t, dG);
		break;
		case 6:
			if(!xhair_custom)
			{
				// single dot, as a backup
				xhair_data.width = 1;
				xhair_data.height = 1;
				xhair_data.ox = 0;
				xhair_data.oy = 0;
				xhair_data.offs[0] = offsetof(xhair_patch_t, dA);
			} else
			{
				// use 'XHAIR' lump
				xhair = xhair_custom;
				return;
			}
		break;
		default:
			xhair = NULL;
			return;
	}

	xhair = (patch_t*)&xhair_data;

	// recolor
	r = extra_config.crosshair_color & 15;
	g = (extra_config.crosshair_color >> 4) & 15;
	b = (extra_config.crosshair_color >> 8) & 15;
	color = r_find_color((r << 4) | r, (g << 4) | g, (b << 4) | b);

	for(uint32_t i = 0; i < xhair->width; i++)
	{
		uint8_t *ptr = (uint8_t*)xhair + xhair->offs[i];

		while(*ptr++ != 0xFF)
		{
			uint32_t count = *ptr++;
			count += 2;
			for(uint32_t i = 0; i < count; i++)
				*ptr++ = color;
		}
	}
}

void stbar_update(player_t *pl)
{
	if(pl->stbar_update & STU_AMMO)
		update_ammo(pl);

	if(pl->stbar_update & STU_WEAPON)
		update_weapon(pl);

	if(pl->stbar_update & STU_KEYS)
		update_keys(pl);

	if(pl->stbar_update & STU_BACKPACK)
		update_backpack(pl);

	if(pl->stbar_update & STU_WEAPON_NOW)
		update_ready_weapon(pl);

	if(pl->stbar_update & STU_WEAPON_NEW)
		do_evil_grin = 1;

	pl->stbar_update = 0;
}

void stbar_start(player_t *pl)
{
	// reset FPS counter
	fps_value = 0;
	fps_diff = 0;
	last_gt = I_GetTime();

	// original status bar
	ST_Start();

	// update (original) ammo
	update_ammo(pl);

	// update weapon slots
	update_weapon(pl);

	// update (original) max ammo
	update_backpack(pl);

	// update current weapon ammo
	update_ready_weapon(pl);

	// clear keys
	memset(keyinv, 0, sizeof(keyinv));

	// crosshair
	stbar_set_xhair();
}

void stbar_draw(player_t *pl)
{
	// not in titlemap
	if(is_title_map)
		return;

	// not if dead
	if(pl->state != PST_LIVE)
	{
		if(screenblocks < 11 && invbar_was_on)
		{
			invbar_was_on = 0;
			stbar_refresh_force = 1;
		}
		return;
	}

	if(!automapactive)
	{
		if(consoleplayer != displayplayer || pl->camera->player)
		{
			// status bar
			draw_full_stbar(pl);

			// inventory bar
			draw_invbar(pl);

			// draw crosshair
			draw_crosshair(pl);
		}
	} else
		// inventory bar
		draw_invbar(pl);

	// FPS counter
	if(extra_config.show_fps)
	{
		uint32_t gettime = I_GetTime();
		uint32_t gdiff = gettime - last_gt;
		fps_diff++;
		if(gdiff >= 35)
		{
			fps_value = fps_diff * FRACUNIT;
			fps_value /= gdiff;
			fps_value *= 35;
			fps_value /= FRACUNIT;
			fps_diff = 0;
			last_gt = gettime;
		}
		stbar_draw_number_r(SCREENWIDTH - 2, 2, fps_value, 3, shortnum);
	}
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
	// replace 'STlib_drawNum'
	{0x0003B020, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)st_draw_num},
	// replace call to 'ST_Drawer' in 'D_Display'
	{0x0001D2D5, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_draw_stbar},
	// remove ammo pointer in normal status bar
	{0x0003ABE2, CODE_HOOK | HOOK_UINT32, 0x10EBC031},
	{0x0003ABFB, CODE_HOOK | HOOK_SET_NOPS, 2},
	{0x0003A282, CODE_HOOK | HOOK_UINT16, 0x3FEB},
	// disable 'keyboxes' in original status bar
	{0x0003A2DB, CODE_HOOK | HOOK_UINT16, 0x48EB},
	// disable 'ST_doPaletteStuff' in 'ST_Drawer'
	{0x0003A639, CODE_HOOK | HOOK_SET_NOPS, 5},
	// fix evil grin
	{0x00039FD4, CODE_HOOK | HOOK_UINT8, 0xB8},
	{0x00039FD5, CODE_HOOK | HOOK_UINT32, (uint32_t)&do_evil_grin},
	{0x00039FD9, CODE_HOOK | HOOK_UINT32, 0xDB84188A},
	{0x00039FDD, CODE_HOOK | HOOK_UINT32, 0x08FE5674},
	{0x00039FE1, CODE_HOOK | HOOK_UINT16, 0x2FEB},
};

