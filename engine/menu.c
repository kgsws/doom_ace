// kgsws' ACE Engine
////
// Menu changes and new entries.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "menu.h"

#define SKULLXOFF	-32
#define LINEHEIGHT	16

uint16_t *menu_now;
static uint64_t *skull_name;

static menuitem_t options_items[] =
{
	{1, "M_ENDGAM"},
	{1, "M_MESSG"}
};

static menu_t options_menu =
{
	sizeof(options_items) / sizeof(menuitem_t),
	NULL, // MainDef
	NULL, // OptionsMenu
	NULL, // M_DrawOptions
	60, 37
};

//
// entry drawer

static __attribute((regparm(2),no_caller_saved_registers))
void menu_items_draw(menu_t *menu)
{
	int32_t x, y;
	patch_t *patch;

	if(menu->x < 0)
		return;

	x = menu->x;
	y = menu->y;

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
	V_DrawPatchDirect(x + SKULLXOFF, menu->y - 5 + *menu_now * LINEHEIGHT, 0, patch);
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
//	{0x000229B2, CODE_HOOK | HOOK_UINT32, (uint32_t)&options_menu},
//	{0x000229B8, CODE_HOOK | HOOK_UINT32, (uint32_t)&options_menu + offsetof(menu_t, last)},
	// import variables
	{0x0002B6D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&menu_now},
	{0x00012222, DATA_HOOK | HOOK_IMPORT, (uint32_t)&skull_name},
};

