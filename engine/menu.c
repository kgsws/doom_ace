// kgsws' ACE Engine
////
// Menu changes and new entries.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "stbar.h"
#include "controls.h"
#include "menu.h"

#define CONTROL_Y_BASE	(40 + LINEHEIGHT_SMALL * 3)
#define CONTROL_X	80
#define CONTROL_X_DEF	220

uint16_t *menu_item_now;

static menu_t **currentMenu;
static uint64_t *skull_name;
static uint32_t *showMessages;
static uint32_t *mouseSensitivity;

static int32_t title_options;
static int32_t title_controls;
static int32_t title_mouse;
static int32_t title_player;
static int32_t small_selector;

static uint16_t control_old;
static int16_t control_pos;

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
		.text = "MESSAGES",
		.status = 1,
		.key = 'm'
	},
	{
		.text = "SCREEN SIZE",
		.status = 2,
		.key = 's'
	},
	{
		.text = "SOUND",
		.status = 1
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

// CONTROLS

static void contols_set(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
static menuitem_t controls_items[] =
{
	// dummy, just to track cursor movement
	{.status = 1, .func = contols_set},
	{.status = 1, .func = contols_set},
	{.status = 1, .func = contols_set},
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

static menuitem_t player_items[] =
{
	{
		.text = "AUTO RUN",
		.status = 1,
		.key = 'r'
	},
	{
		.text = "AUTO SWITCH",
		.status = 1,
		.key = 's'
	},
	{
		.text = "MOUSE LOOK",
		.status = 1,
		.key = 'l'
	},
	{
		.text = "AUTO AIM",
		.status = 1,
		.key = 'a'
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
	*currentMenu = menu;
	*menu_item_now = menu->last;
}

//
// menu 'options'

static __attribute((regparm(2),no_caller_saved_registers))
void options_next(uint32_t sel)
{
	switch(sel)
	{
		case 4:
			M_SetupNextMenu(&controls_menu);
			control_old = *menu_item_now;
		break;
		case 5:
			M_SetupNextMenu(&mouse_menu);
		break;
		case 6:
			M_SetupNextMenu(&player_menu);
		break;
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
void options_draw()
{
	uint8_t tmp[16];

	// title
	V_DrawPatchDirect(108, 15, 0, W_CacheLumpNum(title_options, PU_CACHE));

	// messages
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 1, *showMessages ? "ON" : "OFF");

	// screen size
	doom_sprintf(tmp, "%u", *screenblocks);
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 2, tmp);
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
void contols_set(uint32_t sel)
{
	M_StartMessage("Press a key to set ...\n\n[ESC] to clear", control_input, 0);
}

static __attribute((regparm(2),no_caller_saved_registers))
void controls_draw()
{
	int32_t yy;
	int32_t old;

	// position changes
	if(*menu_item_now != control_old)
	{
		int32_t diff = *menu_item_now - control_old;

		control_old = *menu_item_now;

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
	V_DrawPatchDirect(104, 15, 0, W_CacheLumpNum(title_controls, PU_CACHE));

	// selector
	V_DrawPatchDirect(CONTROL_X - 10, CONTROL_Y_BASE, 0, W_CacheLumpNum(small_selector, PU_STATIC));

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
				M_WriteText(CONTROL_X + 40, yy, (uint8_t*)ctrl_group[old]);

			yy += LINEHEIGHT_SMALL;
		}

		if(yy > 160)
			break;

		if(yy >= 40)
		{
			M_WriteText(CONTROL_X, yy, (uint8_t*)control_list[i].name);
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
	uint32_t sel = *menu_item_now - 2;
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
	uint8_t tmp[16];

	// title
	V_DrawPatchDirect(123, 15, 0, W_CacheLumpNum(title_mouse, PU_CACHE));

	// sensitivity
	doom_sprintf(tmp, "%u", *mouseSensitivity);
	M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * 0, tmp);

	// buttons
	for(uint32_t i = 0; i < NUM_MOUSE_CTRL; i++)
		M_WriteText(options_menu.x + 100, -options_menu.y + LINEHEIGHT_SMALL * (i+2), control_btn_name(i));
}

//
// menu 'player'

static __attribute((regparm(2),no_caller_saved_registers))
void player_draw()
{
	// title
	V_DrawPatchDirect(119, 15, 0, W_CacheLumpNum(title_player, PU_CACHE));
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
		V_DrawPatchDirect(x + CURSORX_SMALL, y + *menu_item_now * LINEHEIGHT_SMALL, 0, patch);

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
				V_DrawPatchDirect(x, y, 0, patch);
			}
			y += LINEHEIGHT;
		}

		patch = W_CacheLumpName((uint8_t*)&skull_name[!!(*gametic & 8)], PU_CACHE);
		V_DrawPatchDirect(x + SKULLXOFF, menu->y - 5 + *menu_item_now * LINEHEIGHT, 0, patch);
	}
}

//
// API

void init_menu()
{
	small_selector = W_GetNumForName("STCFN042"); // it's from font, use PU_STATIC

	title_options = W_GetNumForName((uint8_t*)0x00012247 + doom_data_segment);

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

	pc = W_CacheLumpName((uint8_t*)0x0002246C + doom_data_segment, PU_CACHE);

	V_DrawPatchDirect(x, y, 0, W_CacheLumpName((uint8_t*)0x00022460 + doom_data_segment, PU_CACHE));

	for(uint32_t i = 0; i < count; i++)
	{
		x += 8;
		V_DrawPatchDirect(x, y, 0, pc);
	}

	V_DrawPatchDirect(last, y, 0, W_CacheLumpName((uint8_t*)0x00022478 + doom_data_segment, PU_CACHE));
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace item and cursor drawer
	{0x00023E96, CODE_HOOK | HOOK_UINT16, 0xF889},
	{0x00023E98, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)menu_items_draw},
	{0x00023E9D, CODE_HOOK | HOOK_JMP_DOOM, 0x00023F4E},
	// replace 'options'
	{0x000229B2, CODE_HOOK | HOOK_UINT32, (uint32_t)&options_menu},
	{0x000229B8, CODE_HOOK | HOOK_UINT32, (uint32_t)&options_menu + offsetof(menu_t, last)},
	// import variables
	{0x0002B6D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&menu_item_now},
	{0x0002B67C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&currentMenu},
	{0x00012222, DATA_HOOK | HOOK_IMPORT, (uint32_t)&skull_name},
	{0x0002B6B0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&showMessages},
	{0x0002B6C8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mouseSensitivity},
	// import functions
	{0x00022A60, CODE_HOOK | HOOK_IMPORT, (uint32_t)&options_items[0].func},
	{0x000229D0, CODE_HOOK | HOOK_IMPORT, (uint32_t)&options_items[1].func},
	{0x00022D00, CODE_HOOK | HOOK_IMPORT, (uint32_t)&options_items[2].func},
	{0x000225C0, CODE_HOOK | HOOK_IMPORT, (uint32_t)&options_items[3].func},
	{0x00022C60, CODE_HOOK | HOOK_IMPORT, (uint32_t)&mouse_items[0].func},
};

