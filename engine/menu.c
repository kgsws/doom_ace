// kgsws' ACE Engine
////
// Menu changes and new entries.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "config.h"
#include "stbar.h"
#include "controls.h"
#include "player.h"
#include "render.h"
#include "map.h"
#include "wadfile.h"
#include "draw.h"
#include "wipe.h"
#include "menu.h"

#define CONTROL_Y_BASE	(40 + LINEHEIGHT_SMALL * 3)
#define CONTROL_X	80
#define CONTROL_X_DEF	220

static uint8_t *auto_run;

static int32_t title_options;
static int32_t title_display;
static int32_t title_controls;
static int32_t title_mouse;
static int32_t title_player;
static int32_t small_selector;

static uint16_t control_old;
static int16_t control_pos;

static const uint8_t *const off_on[] = {"OFF", "ON", "FAKE"}; // fake is used in mouse look
static const uint8_t *const weapon_mode[] = {"ORIGINAL", "CENTER", "BOUNCY"};

// episodes
static menuitem_t episode_items[MAX_EPISODES];

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
		.text = "MESSAGES",
		.status = 2,
		.key = 'm'
	},
	{
		.text = "SCREEN SIZE",
		.status = 2,
		.key = 's'
	},
	{
		.text = "WIPE STYLE",
		.status = 2,
		.func = change_wipe,
		.key = 'w'
	},
	{
		.text = "SHOW FPS",
		.status = 2,
		.func = change_fps,
		.key = 'f'
	},
	{
		.text = "GAMMA",
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
		.text = "SHAPE",
		.status = 2,
		.func = xhair_type,
		.key = 'x'
	},
	{
		.text = "RED",
		.status = 2,
		.func = xhair_color
	},
	{
		.text = "GREEN",
		.status = 2,
		.func = xhair_color
	},
	{
		.text = "BLUE",
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

static void controls_set(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static menuitem_t controls_items[] =
{
	// dummy, just to track cursor movement
	{.status = 1, .func = controls_set},
	{.status = 1, .func = controls_set},
	{.status = 1, .func = controls_set},
};

static void controls_draw() __attribute((regparm(2),no_caller_saved_registers));
static menu_t controls_menu =
{
	.numitems = sizeof(controls_items) / sizeof(menuitem_t),
	.prev = &options_menu,
	.menuitems = controls_items,
	.draw = controls_draw,
	.x = -100
};

// MOUSE

static void mouse_change(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static menuitem_t mouse_items[] =
{
	{
		.text = "SENSITIVITY",
		.status = 2
	},
	{
		.text = NULL,
		.status = -1
	},
	{
		.text = "ATTACK",
		.status = 2,
		.func = mouse_change
	},
	{
		.text = "ALT ATTACK",
		.status = 2,
		.func = mouse_change
	},
	{
		.text = "USE",
		.status = 2,
		.func = mouse_change
	},
	{
		.text = "INVENTORY",
		.status = 2,
		.func = mouse_change
	},
	{
		.text = "STRAFE",
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
static menuitem_t player_items[] =
{
	{
		.text = "AUTO RUN",
		.status = 2,
		.func = player_change,
		.key = 'r'
	},
	{
		.text = "AUTO SWITCH",
		.status = 2,
		.func = player_change,
		.key = 's'
	},
	{
		.text = "AUTO AIM",
		.status = 2,
		.func = player_change,
		.key = 'a'
	},
	{
		.text = "MOUSE LOOK",
		.status = 2,
		.func = player_change,
		.key = 'l'
	},
	{
		.text = "WEAPON",
		.status = 2,
		.func = player_change,
		.key = 'c'
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

//
// functions

static inline void M_SetupNextMenu(menu_t *menu)
{
	currentMenu = menu;
	menu_item_now = menu->last;
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
			M_SetupNextMenu(&controls_menu);
			control_old = menu_item_now;
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

	switch(menu_item_now)
	{
		case 8:
			ptr = &extra_config.crosshair_red;
		break;
		case 9:
			ptr = &extra_config.crosshair_green;
		break;
		case 10:
			ptr = &extra_config.crosshair_blue;
		break;
		default:
			return;
	}

	if(dir)
	{
		if(*ptr == 0xFF)
			return;
		*ptr = *ptr + 1;
	} else
	{
		if(!*ptr)
			return;
		*ptr = *ptr - 1;
	}

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
	draw_patch_color = render_tables->cmap + 256 * FONT_COLOR_WHITE;
	M_WriteText(options_menu.x + 20, -options_menu.y + LINEHEIGHT_SMALL * 6, "CROSSHAIR");
	draw_patch_color = NULL;

	// messages
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 0, off_on[!!showMessages]);

	// screen size
	doom_sprintf(text, "%u", screenblocks);
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 1, text);

	// wipe
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 2, wipe_name[extra_config.wipe_type]);

	// FPS
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 3, off_on[!!extra_config.show_fps]);

	// gamma
	doom_sprintf(text, "%u", usegamma);
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 4, text);

	// crosshair type
	doom_sprintf(text, "%u", extra_config.crosshair_type);
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 7, text);

	// crosshair color
	doom_sprintf(text, "%u", extra_config.crosshair_red);
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 8, text);
	doom_sprintf(text, "%u", extra_config.crosshair_green);
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 9, text);
	doom_sprintf(text, "%u", extra_config.crosshair_blue);
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 10, text);
}

//
// menu 'controls'

static __attribute((regparm(2),no_caller_saved_registers,noreturn))
void control_input(uint8_t key)
{
	if(key == 27)
	{
		// remove key
		*control_list[control_pos].ptr = 0;
		S_StartSound(NULL, 24);
	} else
	{
		// set new key
		control_clear_key(key);
		*control_list[control_pos].ptr = key;
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
	if(menu_item_now != control_old)
	{
		int32_t diff = menu_item_now - control_old;

		control_old = menu_item_now;

		if(diff > 1)
			diff = -1;
		else
		if(diff < -1)
			diff = 1;

		control_pos += diff;
		if(control_pos < 0)
			control_pos = 0;
		if(control_pos > NUM_CONTROLS - 1)
			control_pos = NUM_CONTROLS - 1;
	}

	// title
	V_DrawPatchDirect(104, 15, W_CacheLumpNum(title_controls, PU_CACHE));

	// selector
	V_DrawPatchDirect(CONTROL_X - 10, CONTROL_Y_BASE, W_CacheLumpNum(small_selector, PU_STATIC));

	// extra offset
	old = control_list[0].group;
	yy += CONTROL_Y_BASE - control_pos * LINEHEIGHT_SMALL;
	for(uint32_t i = 0; i < NUM_CONTROLS && i < control_pos+1; i++)
	{
		if(control_list[i].group != old)
		{
			old = control_list[i].group;
			yy -= LINEHEIGHT_SMALL * 2;
		}
	}

	// entries
	old = control_list[0].group - 1;
	yy -= LINEHEIGHT_SMALL * 2;
	for(uint32_t i = 0; i < NUM_CONTROLS; i++)
	{
		if(control_list[i].group != old)
		{
			old = control_list[i].group;
			yy += LINEHEIGHT_SMALL;

			if(yy > 160)
				break;

			if(yy >= 40)
			{
				draw_patch_color = render_tables->cmap + 256 * FONT_COLOR_WHITE;
				M_WriteText(CONTROL_X + 40, yy, ctrl_group[old]);
				draw_patch_color = NULL;
			}

			yy += LINEHEIGHT_SMALL;
		}

		if(yy > 160)
			break;

		if(yy >= 40)
		{
			M_WriteText(CONTROL_X, yy, control_list[i].name);
			M_WriteText(CONTROL_X_DEF, yy, control_key_name(*control_list[i].ptr));
		}

		yy += LINEHEIGHT_SMALL;
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
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 0, text);

	// buttons
	for(uint32_t i = 0; i < NUM_MOUSE_CTRL; i++)
		M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * (i+2), control_btn_name(i));
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
		break;
		case 2:
			extra_config.auto_aim = !extra_config.auto_aim;
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

			return;
	}

	// this has to be sent in tick message
	player_flags_changed = 1;
}

static __attribute((regparm(2),no_caller_saved_registers))
void player_draw()
{
	// title
	V_DrawPatchDirect(119, 15, W_CacheLumpNum(title_player, PU_CACHE));

	// auto run
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 0, off_on[*auto_run == 1]);

	// auto switch
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 1, off_on[!!extra_config.auto_switch]);

	// auto aim
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 2, off_on[!!extra_config.auto_aim]);

	// mouse look
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 3, off_on[extra_config.mouse_look]);

	// center weapon
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 4, weapon_mode[extra_config.center_weapon]);
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

		patch = W_CacheLumpNum(small_selector, PU_STATIC);
		if(menu->menuitems[menu_item_now].status == 2)
			V_DrawPatchDirect(x + CURSORX_SMALL - 6, y + menu_item_now * LINEHEIGHT_SMALL, patch);
		V_DrawPatchDirect(x + CURSORX_SMALL, y + menu_item_now * LINEHEIGHT_SMALL, patch);

		for(uint32_t i = 0; i < menu->numitems; i++)
		{
			if(menu->menuitems[i].text)
				M_WriteText(x, y, menu->menuitems[i].text);
			y += LINEHEIGHT_SMALL;
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
// new game menu

static __attribute((regparm(2),no_caller_saved_registers))
void sel_episode(uint32_t sel)
{
	if(map_episode_def[sel].map_lump < 0)
		map_lump.wame = 0xFF; // invalid name
	else
		strcpy(map_lump.name, lumpinfo[map_episode_def[sel].map_lump].name);

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
		M_SetupNextMenu(&EpiDef);
	else
		sel_episode(0);
}

//
// API

void init_menu()
{
	small_selector = W_GetNumForName("STCFN042"); // it's from font, use PU_STATIC

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
	// fade background behind message
	if(messageToPrint)
		for(uint8_t *ptr = screen_buffer; ptr < screen_buffer + 320 * 200; ptr++)
			*ptr = colormaps[*ptr + 256 * 21];
	return messageToPrint;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
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
	// import variables
	{0x0001FBC5, CODE_HOOK | HOOK_IMPORT, (uint32_t)&auto_run},
	// import functions
	{0x00022A60, CODE_HOOK | HOOK_IMPORT, (uint32_t)&options_items[0].func},
	{0x000225C0, CODE_HOOK | HOOK_IMPORT, (uint32_t)&options_items[1].func},
	{0x000229D0, CODE_HOOK | HOOK_IMPORT, (uint32_t)&display_items[0].func},
	{0x00022D00, CODE_HOOK | HOOK_IMPORT, (uint32_t)&display_items[1].func},
	{0x00022C60, CODE_HOOK | HOOK_IMPORT, (uint32_t)&mouse_items[0].func},
};

