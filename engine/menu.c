// kgsws' ACE Engine
////
// Menu changes and new entries.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "vesa.h"
#include "config.h"
#include "stbar.h"
#include "controls.h"
#include "player.h"
#include "render.h"
#include "decorate.h"
#include "inventory.h"
#include "map.h"
#include "wadfile.h"
#include "font.h"
#include "draw.h"
#include "wipe.h"
#include "menu.h"

#define CONTROL_Y_BASE	(40 + mod_config.menu_font_height * 3)
#define CONTROL_X	80
#define CONTROL_X_DEF	220

#define PCLASS_Y_BASE	(40 + mod_config.menu_font_height * 3)

int_fast8_t menu_font_color = FCOL_ORIGINAL;

static uint8_t *auto_run;

static int32_t title_options;
static int32_t title_display;
static int32_t title_controls;
static int32_t title_mouse;
static int32_t title_player;
static int32_t title_pclass;
static patch_t *selector_normal;
static patch_t *selector_special;

static uint16_t multi_old;
static int16_t multi_pos;

static uint_fast8_t epi_sel;

static const uint8_t *const off_on[] = {"OFF", "ON", "FAKE"}; // fake is used in mouse look
static const uint8_t *const weapon_mode[] = {"ORIGINAL", "CENTER", "BOUNCY"};

// episodes
static menuitem_t episode_items[MAX_EPISODES];

// multi-menu

static void controls_set(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static void playerclass_set(uint32_t) __attribute((regparm(2),no_caller_saved_registers));

static menuitem_t multimenu_items[] =
{
	// dummy, just to track cursor movement
	{.status = 1},
	{.status = 1},
	{.status = 1},
};

// OPTIONS

static void options_next(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static menuitem_t options_items[] =
{
	{
		.text = "END GAME",
		.status = 1,
		.key = 'e'
	},
	{
		.text = "SOUND",
		.status = 1
	},
	{
		.text = "DISPLAY",
		.status = 1,
		.func = options_next
	},
	{
		.text = "CONTROLS",
		.status = 1,
		.func = options_next
	},
	{
		.text = "MOUSE",
		.status = 1,
		.func = options_next
	},
	{
		.text = "PLAYER",
		.status = 1,
		.func = options_next
	},
};

static void options_draw() __attribute((regparm(2),no_caller_saved_registers));
static menu_t options_menu =
{
	.numitems = sizeof(options_items) / sizeof(menuitem_t),
	.prev = NULL, // MainDef
	.menuitems = options_items,
	.draw = options_draw,
	.x = 100,
	.y = -50
};

// DISPLAY

static void change_wipe(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static void change_fps(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static void xhair_type(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static void xhair_color(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static void change_gamma(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static menuitem_t display_items[] =
{
	{
		.text = "Messages",
		.status = 2,
		.key = 'm'
	},
	{
		.text = "Screen Size",
		.status = 2,
		.key = 's'
	},
	{
		.text = "Wipe Style",
		.status = 2,
		.func = change_wipe,
		.key = 'w'
	},
	{
		.text = "Show FPS",
		.status = 2,
		.func = change_fps,
		.key = 'f'
	},
	{
		.text = "Gamma",
		.status = 2,
		.func = change_gamma,
		.key = 'g'
	},
	{
		.status = -1
	},
	{
		.status = -1
	},
	{
		.text = "Shape",
		.status = 2,
		.func = xhair_type,
		.key = 'x'
	},
	{
		.text = "Red",
		.status = 2,
		.func = xhair_color
	},
	{
		.text = "Green",
		.status = 2,
		.func = xhair_color
	},
	{
		.text = "Blue",
		.status = 2,
		.func = xhair_color
	},
};

static void display_draw() __attribute((regparm(2),no_caller_saved_registers));
static menu_t display_menu =
{
	.numitems = sizeof(display_items) / sizeof(menuitem_t),
	.prev = &options_menu,
	.menuitems = display_items,
	.draw = display_draw,
	.x = 100,
	.y = -50
};

// CONTROLS

static void controls_draw() __attribute((regparm(2),no_caller_saved_registers));
static menu_t controls_menu =
{
	.numitems = sizeof(multimenu_items) / sizeof(menuitem_t),
	.prev = &options_menu,
	.menuitems = multimenu_items,
	.draw = controls_draw,
	.x = -100
};

// MOUSE

static void mouse_change(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static menuitem_t mouse_items[] =
{
	{
		.text = "Sensitivity",
		.status = 2
	},
	{
		.text = NULL,
		.status = -1
	},
	{
		.text = "Attack",
		.status = 2,
		.func = mouse_change
	},
	{
		.text = "Alt Attack",
		.status = 2,
		.func = mouse_change
	},
	{
		.text = "Use",
		.status = 2,
		.func = mouse_change
	},
	{
		.text = "Inventory",
		.status = 2,
		.func = mouse_change
	},
	{
		.text = "Strafe",
		.status = 2,
		.func = mouse_change
	},
};

static void mouse_draw() __attribute((regparm(2),no_caller_saved_registers));
static menu_t mouse_menu =
{
	.numitems = sizeof(mouse_items) / sizeof(menuitem_t),
	.prev = &options_menu,
	.menuitems = mouse_items,
	.draw = mouse_draw,
	.x = 100,
	.y = -50
};

// PLAYER

static void player_change(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static void player_color(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static menuitem_t player_items[] =
{
	{
		.text = "Auto Run",
		.status = 2,
		.func = player_change,
		.key = 'r'
	},
	{
		.text = "Auto Switch",
		.status = 2,
		.func = player_change,
		.key = 's'
	},
	{
		.text = "Auto Aim",
		.status = 2,
		.func = player_change,
		.key = 'a'
	},
	{
		.text = "Mouse Look",
		.status = 2,
		.func = player_change,
		.key = 'l'
	},
	{
		.text = "Weapon",
		.status = 2,
		.func = player_change,
		.key = 'c'
	},
	{
		.text = "Quick Item",
		.status = 2,
		.func = player_change,
		.key = 'i'
	},
	{
		.status = -1
	},
	{
		.status = -1
	},
	{
		.status = -1
	},
	{
		.text = "Red",
		.status = 2,
		.func = player_color
	},
	{
		.text = "Green",
		.status = 2,
		.func = player_color
	},
	{
		.text = "Blue",
		.status = 2,
		.func = player_color
	},
};

static void player_draw() __attribute((regparm(2),no_caller_saved_registers));
static menu_t player_menu =
{
	.numitems = sizeof(player_items) / sizeof(menuitem_t),
	.prev = &options_menu,
	.menuitems = player_items,
	.draw = player_draw,
	.x = 100,
	.y = -50
};

// PLAYER CLASS

static void playerclass_draw() __attribute((regparm(2),no_caller_saved_registers));
static menu_t playerclass_menu =
{
	.numitems = sizeof(multimenu_items) / sizeof(menuitem_t),
	.prev = &NewDef,
	.menuitems = multimenu_items,
	.draw = playerclass_draw,
	.x = -100
};

//
// functions

static inline void M_SetupNextMenu(menu_t *menu)
{
	currentMenu = menu;
	menu_item_now = menu->last;
}

static void setup_multimenu(void *func)
{
	multi_pos = 0;
	for(uint32_t i = 0; i < sizeof(multimenu_items) / sizeof(menuitem_t); i++)
		multimenu_items[i].func = func;
}

//
// new game menu

static __attribute((regparm(2),no_caller_saved_registers))
void sel_episode(uint32_t sel)
{
	epi_sel = sel; // for player class menu

	if(map_episode_def[sel].map_lump < 0)
		map_lump.wame = 0xFF; // invalid name
	else
		strcpy(map_lump.name, lumpinfo[map_episode_def[sel].map_lump].name);

	if(num_player_classes > 1)
	{
		setup_multimenu(playerclass_set);
		M_SetupNextMenu(&playerclass_menu);
		multi_old = menu_item_now;
		return;
	}

	if(map_episode_def[sel].flags & EPI_FLAG_NO_SKILL_MENU)
	{
		G_DeferedInitNew(sk_medium, 0, 0);
		M_ClearMenus();
		return;
	}

	M_SetupNextMenu(&NewDef);
}

static __attribute((regparm(2),no_caller_saved_registers))
void sel_new_game()
{
	if(map_episode_count > 1)
	{
		M_SetupNextMenu(&EpiDef);
		return;
	}

	sel_episode(0);
}

//
// menu 'options'

static __attribute((regparm(2),no_caller_saved_registers))
void options_next(uint32_t sel)
{
	switch(sel)
	{
		case 2:
			M_SetupNextMenu(&display_menu);
		break;
		case 3:
			setup_multimenu(controls_set);
			M_SetupNextMenu(&controls_menu);
			multi_old = menu_item_now;
		break;
		case 4:
			M_SetupNextMenu(&mouse_menu);
		break;
		case 5:
			M_SetupNextMenu(&player_menu);
		break;
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
void options_draw()
{
	// title
	V_DrawPatchDirect(108, 15, W_CacheLumpNum(title_options, PU_CACHE));
}

//
// menu 'display'

static __attribute((regparm(2),no_caller_saved_registers))
void change_wipe(uint32_t dir)
{
	if(dir)
	{
		extra_config.wipe_type++;
		if(extra_config.wipe_type >= NUM_WIPE_TYPES)
			extra_config.wipe_type = 0;
	} else
	{
		if(extra_config.wipe_type)
			extra_config.wipe_type--;
		else
			extra_config.wipe_type = NUM_WIPE_TYPES - 1;
	}

	// disable wipe forced by mod
	mod_config.wipe_type = 255;
}

static __attribute((regparm(2),no_caller_saved_registers))
void change_fps(uint32_t dir)
{
	extra_config.show_fps = !extra_config.show_fps;
}

static __attribute((regparm(2),no_caller_saved_registers))
void xhair_type(uint32_t dir)
{
	if(dir)
		extra_config.crosshair_type++;
	else
		extra_config.crosshair_type--;
	stbar_set_xhair();
}

static __attribute((regparm(2),no_caller_saved_registers))
void xhair_color(uint32_t dir)
{
	uint8_t *ptr;
	uint32_t shift;
	uint32_t value = extra_config.crosshair_color;
	uint32_t color;

	switch(menu_item_now)
	{
		case 8:
			shift = 0;
		break;
		case 9:
			shift = 4;
		break;
		case 10:
			shift = 8;
		break;
		default:
			return;
	}

	color = (value >> shift) & 15;
	value &= ~(15 << shift);

	if(dir)
	{
		if(color == 15)
			return;
		color = color + 1;
	} else
	{
		if(!color)
			return;
		color = color - 1;
	}

	value |= color << shift;
	extra_config.crosshair_color = value;

	stbar_set_xhair();
}

static __attribute((regparm(2),no_caller_saved_registers))
void change_gamma(uint32_t dir)
{
	uint32_t value = usegamma;

	if(dir)
	{
		if(value < 4)
			value++;
		else
			value = 0;
	} else
	{
		if(value)
			value--;
		else
			value = 4;
	}

	usegamma = value;
	I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
}

static __attribute((regparm(2),no_caller_saved_registers))
void display_draw()
{
	uint8_t text[16];

	// title
	V_DrawPatchDirect(117, 15, W_CacheLumpNum(title_display, PU_CACHE));

	// 'crosshair' title
	menu_font_color = FCOL_WHITE;
	font_menu_text(options_menu.x + 20, -options_menu.y + mod_config.menu_font_height * 6, "CROSSHAIR");
	menu_font_color = FCOL_ORIGINAL;

	// messages
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 0, off_on[!!showMessages]);

	// screen size
	doom_sprintf(text, "%u", screenblocks);
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 1, text);

	// wipe
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 2, wipe_name[extra_config.wipe_type]);

	// FPS
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 3, off_on[!!extra_config.show_fps]);

	// gamma
	doom_sprintf(text, "%u", usegamma);
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 4, text);

	// crosshair type
	doom_sprintf(text, "%u", extra_config.crosshair_type);
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 7, text);

	// crosshair color
	doom_sprintf(text, "%u", extra_config.crosshair_color & 15);
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 8, text);
	doom_sprintf(text, "%u", (extra_config.crosshair_color >> 4) & 15);
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 9, text);
	doom_sprintf(text, "%u", (extra_config.crosshair_color >> 8) & 15);
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 10, text);
}

//
// menu 'controls'

static __attribute((regparm(2),no_caller_saved_registers,noreturn))
void control_input(uint8_t key)
{
	if(key == 27)
	{
		// remove key
		*control_list[multi_pos].ptr = 1;
		S_StartSound(NULL, 24);
	} else
	if(key)
	{
		// set new key
		control_clear_key(key);
		*control_list[multi_pos].ptr = key;
		S_StartSound(NULL, 1);
	}

	skip_message_cancel();
}

static __attribute((regparm(2),no_caller_saved_registers))
void controls_set(uint32_t sel)
{
	M_StartMessage("Press a key to set ...\n\n[ESC] to clear", control_input, 0);
}

static __attribute((regparm(2),no_caller_saved_registers))
void controls_draw()
{
	int32_t yy;
	int32_t old;

	// position changes
	if(menu_item_now != multi_old)
	{
		int32_t diff = menu_item_now - multi_old;

		multi_old = menu_item_now;

		if(diff > 1)
			diff = -1;
		else
		if(diff < -1)
			diff = 1;

		multi_pos += diff;
		if(multi_pos < 0)
			multi_pos = 0;
		if(multi_pos > NUM_CONTROLS - 1)
			multi_pos = NUM_CONTROLS - 1;
	}

	// title
	V_DrawPatchDirect(104, 15, W_CacheLumpNum(title_controls, PU_CACHE));

	// selector
	V_DrawPatchDirect(CONTROL_X - 10, CONTROL_Y_BASE, selector_normal);

	// extra offset
	old = control_list[0].group;
	yy += CONTROL_Y_BASE - multi_pos * mod_config.menu_font_height;
	for(uint32_t i = 0; i < NUM_CONTROLS && i < multi_pos+1; i++)
	{
		if(control_list[i].group != old)
		{
			old = control_list[i].group;
			yy -= mod_config.menu_font_height * 2;
		}
	}

	// entries
	old = control_list[0].group - 1;
	yy -= mod_config.menu_font_height * 2;
	for(uint32_t i = 0; i < NUM_CONTROLS; i++)
	{
		if(control_list[i].group != old)
		{
			old = control_list[i].group;
			yy += mod_config.menu_font_height;

			if(yy > 160)
				break;

			if(yy >= 40)
			{
				menu_font_color = FCOL_WHITE;
				font_menu_text(CONTROL_X + 40, yy, ctrl_group[old]);
				menu_font_color = FCOL_ORIGINAL;
			}

			yy += mod_config.menu_font_height;
		}

		if(yy > 160)
			break;

		if(yy >= 40)
		{
			font_menu_text(CONTROL_X, yy, control_list[i].name);
			font_menu_text(CONTROL_X_DEF, yy, control_key_name(*control_list[i].ptr));
		}

		yy += mod_config.menu_font_height;
	}
}

//
// menu 'mouse'

static __attribute((regparm(2),no_caller_saved_registers))
void mouse_change(uint32_t dir)
{
	uint32_t sel = menu_item_now - 2;
	int32_t value;

	value = *ctrl_mouse_ptr[sel];
	if(dir)
	{
		value++;
		if(value == NUM_MOUSE_BTNS)
			value = -1;
	} else
	{
		value--;
		if(value < -1)
			value = NUM_MOUSE_BTNS - 1;
	}

	*ctrl_mouse_ptr[sel] = value;
}

static __attribute((regparm(2),no_caller_saved_registers))
void mouse_draw()
{
	uint8_t text[16];

	// title
	V_DrawPatchDirect(123, 15, W_CacheLumpNum(title_mouse, PU_CACHE));

	// sensitivity
	doom_sprintf(text, "%u", mouseSensitivity);
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 0, text);

	// buttons
	for(uint32_t i = 0; i < NUM_MOUSE_CTRL; i++)
		font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * (i+2), control_btn_name(i));
}

//
// menu 'player'

static __attribute((regparm(2),no_caller_saved_registers))
void player_change(uint32_t dir)
{
	switch(menu_item_now)
	{
		case 0:
			*auto_run = !*auto_run;
			return;
		case 1:
			extra_config.auto_switch = !extra_config.auto_switch;
			player_info_changed = 1;
		break;
		case 2:
			extra_config.auto_aim = !extra_config.auto_aim;
			player_info_changed = 1;
		break;
		case 3:
			if(dir)
			{
				if(extra_config.mouse_look < 2)
					extra_config.mouse_look++;
				else
					extra_config.mouse_look = 0;
			} else
			{
				if(extra_config.mouse_look)
					extra_config.mouse_look--;
				else
					extra_config.mouse_look = 2;
			}

			if(extra_config.mouse_look == 2)
				stbar_set_xhair();

			player_info_changed = 1;
		break;
		case 4:
			if(dir)
			{
				if(extra_config.center_weapon < 2)
					extra_config.center_weapon++;
				else
					extra_config.center_weapon = 0;
			} else
			{
				if(extra_config.center_weapon)
					extra_config.center_weapon--;
				else
					extra_config.center_weapon = 2;
			}
		break;
		case 5:
		{
			int32_t dd, idx;

			if(dir)
				dd = 1;
			else
				dd = -1;

			idx = extra_config.quick_inv;

			for(uint32_t i = 0; i < num_mobj_types; i++)
			{
				mobjinfo_t *info;

				idx += dd;
				if(idx < 0)
					idx = num_mobj_types - 1;
				else
				if(idx >= num_mobj_types)
					idx = 0;

				if(inventory_is_usable(mobjinfo + idx))
				{
					extra_config.quick_inv = idx;
					extra_config.quick_inv_alias = mobjinfo[idx].alias;
					player_info_changed = 1;
					break;
				}
			}
		}
		break;
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
void player_color(uint32_t dir)
{
	uint8_t *ptr;
	uint32_t shift;
	uint32_t value = extra_config.player_color;
	uint32_t color;

	switch(menu_item_now)
	{
		case 9:
			shift = 0;
		break;
		case 10:
			shift = 4;
		break;
		case 11:
			shift = 8;
		break;
		default:
			return;
	}

	color = (value >> shift) & 15;
	value &= ~(15 << shift);

	if(dir)
	{
		if(color == 15)
			return;
		color = color + 1;
	} else
	{
		if(!color)
			return;
		color = color - 1;
	}

	value |= color << shift;
	extra_config.player_color = value;

	r_generate_player_color(consoleplayer);

	player_info_changed = 1;
}

static __attribute((regparm(2),no_caller_saved_registers))
void player_draw()
{
	uint8_t text[16];

	// title
	V_DrawPatchDirect(119, 15, W_CacheLumpNum(title_player, PU_CACHE));

	// 'crosshair' title
	menu_font_color = FCOL_WHITE;
	font_menu_text(options_menu.x + 20, -options_menu.y + mod_config.menu_font_height * 8, "COLOR");
	menu_font_color = FCOL_ORIGINAL;

	// quick item (intentionaly first)
	if(extra_config.quick_inv)
	{
		int16_t ox, oy;
		mobjinfo_t *info = mobjinfo + extra_config.quick_inv;
		patch_t *patch = stbar_icon_ptr(info->inventory.icon);

		ox = patch->x;
		oy = patch->y;
		patch->x = 0;
		patch->y = 0;

		V_DrawPatchDirect(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 5, patch);

		patch->x = ox;
		patch->y = oy;
	} else
		font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 5, "---");

	// auto run
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 0, off_on[*auto_run == 1]);

	// auto switch
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 1, off_on[!!extra_config.auto_switch]);

	// auto aim
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 2, off_on[!!extra_config.auto_aim]);

	// mouse look
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 3, off_on[extra_config.mouse_look]);

	// center weapon
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 4, weapon_mode[extra_config.center_weapon]);

	// player color
	doom_sprintf(text, "%u", extra_config.player_color & 15);
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 9, text);
	doom_sprintf(text, "%u", (extra_config.player_color >> 4) & 15);
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 10, text);
	doom_sprintf(text, "%u", (extra_config.player_color >> 8) & 15);
	font_menu_text(options_menu.x + 100, -options_menu.y + mod_config.menu_font_height * 11, text);
}

//
// menu 'player class'

static __attribute((regparm(2),no_caller_saved_registers))
void playerclass_set(uint32_t sel)
{
	player_info[consoleplayer].playerclass = multi_pos;

	if(map_episode_def[epi_sel].flags & EPI_FLAG_NO_SKILL_MENU)
	{
		G_DeferedInitNew(sk_medium, 0, 0);
		M_ClearMenus();
		return;
	}

	M_SetupNextMenu(&NewDef);
}

static __attribute((regparm(2),no_caller_saved_registers))
void playerclass_draw()
{
	int32_t yy;

	// position changes
	if(menu_item_now != multi_old)
	{
		int32_t diff = menu_item_now - multi_old;

		multi_old = menu_item_now;

		if(diff > 1)
			diff = -1;
		else
		if(diff < -1)
			diff = 1;

		multi_pos += diff;
		if(multi_pos < 0)
			multi_pos = 0;
		if(multi_pos > num_player_classes - 1)
			multi_pos = num_player_classes - 1;
	}

	// title
	V_DrawPatchDirect(78, 15, W_CacheLumpNum(title_pclass, PU_CACHE));

	// selector
	V_DrawPatchDirect(CONTROL_X - 10, PCLASS_Y_BASE, selector_normal);

	// entries
	yy = PCLASS_Y_BASE + mod_config.menu_font_height * -multi_pos;
	for(uint32_t i = 0; i < num_player_classes; i++)
	{
		if(yy > 160)
			break;

		if(yy >= 40)
		{
			uint8_t *name = mobjinfo[player_class[i]].player.name;
			if(!name)
				name = "Unnamed";
			font_menu_text(CONTROL_X, yy, name);
		}

		yy += mod_config.menu_font_height;
	}
}

//
// entry drawer

static __attribute((regparm(2),no_caller_saved_registers))
void menu_items_draw(menu_t *menu)
{
	int32_t x = menu->x;
	int32_t y = menu->y;
	patch_t *patch;

	if(x < 0)
		// drawer is disabled
		return;

	if(y < 0)
	{
		// small font
		y = -y;

		if(menu->menuitems[menu_item_now].status == 2)
			V_DrawPatchDirect(x + CURSORX_SMALL, y + menu_item_now * mod_config.menu_font_height, selector_special);
		else
			V_DrawPatchDirect(x + CURSORX_SMALL, y + menu_item_now * mod_config.menu_font_height, selector_normal);

		for(uint32_t i = 0; i < menu->numitems; i++)
		{
			if(menu->menuitems[i].text)
				font_menu_text(x, y, menu->menuitems[i].text);
			y += mod_config.menu_font_height;
		}
	} else
	{
		// original

		for(uint32_t i = 0; i < menu->numitems; i++)
		{
			if(menu->menuitems[i].name[0])
			{
				patch = W_CacheLumpName(menu->menuitems[i].name, PU_CACHE);
				V_DrawPatchDirect(x, y, patch);
			}
			y += LINEHEIGHT;
		}

		patch = W_CacheLumpName((uint8_t*)&dtxt_skull_name[!!(gametic & 8)], PU_CACHE);
		V_DrawPatchDirect(x + SKULLXOFF, menu->y - 5 + menu_item_now * LINEHEIGHT, patch);
	}
}

//
// API

void menu_init()
{
	uint8_t text[32];

	doom_sprintf(text, dtxt_stcfn, 42);
	selector_normal = W_CacheLumpName(text, PU_STATIC);
	doom_sprintf(text, dtxt_stcfn, 62);
	selector_special = W_CacheLumpName(text, PU_STATIC);
}

void init_menu()
{
	title_options = W_GetNumForName(dtxt_m_option);

	title_display = W_CheckNumForName("M_DISPL");
	if(title_display < 0)
		title_display = title_options;

	title_controls = W_CheckNumForName("M_CNTRL");
	if(title_controls < 0)
		title_controls = title_options;

	title_mouse = W_CheckNumForName("M_MOUSE");
	if(title_mouse < 0)
		title_mouse = title_options;

	title_player = W_CheckNumForName("M_PLAYER");
	if(title_player < 0)
		title_player = title_options;

	title_pclass = W_CheckNumForName("M_PCLASS");
	if(title_pclass < 0)
		title_pclass = W_GetNumForName(dtxt_m_ngame);
}

void menu_draw_slot_bg(uint32_t x, uint32_t y, uint32_t width)
{
	patch_t *pc;
	uint32_t count;
	uint32_t last;

	y += 7;

	last = x + width - 8;
	width -= 7;
	count = width / 8;

	pc = W_CacheLumpName(dtxt_m_lscntr, PU_CACHE);

	V_DrawPatchDirect(x, y, W_CacheLumpName(dtxt_m_lsleft, PU_CACHE));

	for(uint32_t i = 0; i < count; i++)
	{
		x += 8;
		V_DrawPatchDirect(x, y, pc);
	}

	V_DrawPatchDirect(last, y, W_CacheLumpName(dtxt_m_lsrght, PU_CACHE));
}

void menu_setup_episodes()
{
	EpiDef.numitems = map_episode_count;
	EpiDef.menuitems = episode_items;

	for(uint32_t i = 0; i < map_episode_count; i++)
	{
		menuitem_t *mi = episode_items + i;
		map_episode_t *epi = map_episode_def + i;
		mi->status = 1;
		mi->func = sel_episode;
		if(epi->title_lump < 0)
			strcpy(mi->name, dtxt_m_episod);
		else
			strcpy(mi->name, lumpinfo[epi->title_lump].name);
	}
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t menu_check_message()
{
	// fade background behind menu or message
	if(menuactive || messageToPrint)
		for(uint8_t *ptr = framebuffer; ptr < framebuffer + 320 * 200; ptr++)
			*ptr = colormaps[*ptr + 256 * 21];

	// check for message with custom font
	return font_message_to_print();
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// disable call to 'M_Init' - it was already called
	{0x0001E74D, CODE_HOOK | HOOK_SET_NOPS, 5},
	// replace item and cursor drawer
	{0x00023E96, CODE_HOOK | HOOK_UINT16, 0xF889},
	{0x00023E98, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)menu_items_draw},
	{0x00023E9D, CODE_HOOK | HOOK_JMP_DOOM, 0x00023F4E},
	// change 'messageToPrint' check in 'M_Drawer'
	{0x00023D0B, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)menu_check_message},
	{0x00023D10, CODE_HOOK | HOOK_UINT8, 0x90},
	{0x00023D17, CODE_HOOK | HOOK_UINT16, 0xC085},
	// replace 'options'
	{0x000229B2, CODE_HOOK | HOOK_UINT32, (uint32_t)&options_menu},
	{0x000229B8, CODE_HOOK | HOOK_UINT32, (uint32_t)&options_menu + offsetof(menu_t, last)},
	// replace menu setup in 'M_NewGame'
	{0x0002272D, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)sel_new_game},
	{0x00022732, CODE_HOOK | HOOK_UINT16, 0x2CEB},
	// disable 'M_ChangeDetail'
	{0x00022C90, CODE_HOOK | HOOK_UINT8, 0xC3},
	// import variables
	{0x0001FBC5, CODE_HOOK | HOOK_IMPORT, (uint32_t)&auto_run},
	// import functions
	{0x00022A60, CODE_HOOK | HOOK_IMPORT, (uint32_t)&options_items[0].func},
	{0x000225C0, CODE_HOOK | HOOK_IMPORT, (uint32_t)&options_items[1].func},
	{0x000229D0, CODE_HOOK | HOOK_IMPORT, (uint32_t)&display_items[0].func},
	{0x00022D00, CODE_HOOK | HOOK_IMPORT, (uint32_t)&display_items[1].func},
	{0x00022C60, CODE_HOOK | HOOK_IMPORT, (uint32_t)&mouse_items[0].func},
};

