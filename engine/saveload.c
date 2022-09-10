// kgsws' ACE Engine
////
// New, better, game save & load.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "decorate.h"
#include "inventory.h"
#include "mobj.h"
#include "player.h"
#include "weapon.h"
#include "stbar.h"
#include "saveload.h"

#define SAVE_SLOT_COUNT	6
#define SAVE_SLOT_FILE	"save%u.tga"
#define SAVE_NAME_SIZE	20

#define SAVE_MAGIC	0xA9C7 // just a random number
#define SAVE_TGA_FORMAT	0x2001000000010116
#define SAVE_TGA_PIXOFS	0x428

typedef struct
{ // size must be 24
	uint8_t text[SAVE_NAME_SIZE + 1];
	uint8_t step;
	uint16_t width;
} save_name_t;

typedef struct
{
	uint64_t stuff;
	int16_t originX, originY;
	uint16_t width, height;
	uint8_t bpp;
	uint8_t flags;
	uint8_t savename[SAVE_NAME_SIZE];
	uint16_t save_magic;
} stga_head_t;

typedef struct
{
	uint32_t offs_ext;
	uint32_t offs_dev;
	uint8_t tgasig[18];
} __attribute__((packed)) stga_foot_t;

//

static save_name_t *save_name;
static menuitem_t *load_items;
static menu_t *load_menu;
static uint16_t *menu_now;

static patch_t *preview_patch;
static uint_fast8_t show_save_slot = -1;

static const uint8_t empty_slot[] = "[EMPTY]";
static const uint8_t tgasig[] = "TRUEVISION-XFILE.";

//
// funcs

static inline void prepare_save_slot(int fd, uint32_t idx)
{
	stga_head_t head;
	stga_foot_t foot;
	int32_t tmp;

	// header

	tmp = doom_read(fd, &head, sizeof(stga_head_t));
	if(tmp != sizeof(stga_head_t))
		return;

	if(head.stuff != SAVE_TGA_FORMAT)
		return;

	if(head.save_magic != SAVE_MAGIC)
		return;

	// footer

	doom_lseek(fd, -sizeof(stga_foot_t), SEEK_END);

	tmp = doom_read(fd, &foot, sizeof(stga_foot_t));
	if(tmp != sizeof(stga_foot_t))
		return;

	if(strcmp(foot.tgasig, tgasig))
		return;

	// create entry
	head.save_magic = 0;
	strcpy(save_name[idx].text, head.savename);
	load_items[idx].status = 1;
	save_name[idx].step = 0;

	// preview size

	if(head.width < 160 && head.height < 100)
		return;

	if(head.width > 2048)
		return;

	tmp = 1 + (head.width - 1) / 160;
	if(head.height / tmp < 100)
		return;

	save_name[idx].step = tmp;
	save_name[idx].width = head.width;
}

static inline void generate_preview_line(int fd, uint32_t y, save_name_t *slot)
{
	uint8_t *dst;
	uint8_t *src;

	dst = screen_buffer;
	dst += 320 * 200 + sizeof(patch_t) + 160 * sizeof(uint32_t) + 3 + y;

	src = screen_buffer;
	src += 320 * 200 * 2;

	doom_lseek(fd, SAVE_TGA_PIXOFS + y * slot->width * slot->step, SEEK_SET);
	doom_read(fd, src, 160 * slot->step);

	for(uint32_t i = 0; i < 160; i++)
	{
		*dst = *src;
		src += slot->step;
		dst += 105;
	}
}

static void draw_slot_bg(uint32_t x, uint32_t y, uint32_t width)
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

static void draw_check_preview()
{
	if(show_save_slot != *menu_now)
	{
		// generate preview patch in RAM
		int fd;
		uint8_t *base;
		save_name_t *slot;
		uint8_t name[16];

		show_save_slot = *menu_now;
		slot = save_name + show_save_slot;

		if(slot->step)
		{
			doom_sprintf(name, SAVE_SLOT_FILE, show_save_slot);
			fd = doom_open_RD(name);
			if(fd < 0)
				slot->step = 0;
		}

		preview_patch = (patch_t*)(screen_buffer + 320 * 200);
		preview_patch->width = 160;
		preview_patch->height = 100;
		preview_patch->x = -4;
		preview_patch->y = -50;

		base = (uint8_t*)(preview_patch->offs + 160);
		for(uint32_t i = 0; i < 160; i++)
		{
			preview_patch->offs[i] = base - (uint8_t*)preview_patch;
			base[0] = 0;
			base[1] = 100;
			if(!slot->step)
				memset(base + 3, 0, 100);
			base[104] = 0xFF;
			base += 105;
		}

		if(slot->step)
		{
			for(uint32_t i = 0; i < 100; i++)
				generate_preview_line(fd, i, slot);
			doom_close(fd);
		}
	}

	for(uint32_t i = 0; i < SAVE_SLOT_COUNT; i++)
	{
		uint32_t y = 57 + i * 16;
		draw_slot_bg(171, y, 150);
		M_WriteText(172, y, save_name[i].text);
	}

	for(uint32_t i = 0; i < 7; i++)
		draw_slot_bg(5, 52 + i * 14, 164);
	draw_slot_bg(5, 142, 164);

	V_DrawPatchDirect(0, 0, 0, preview_patch);
}

//
// drawers

static __attribute((regparm(2),no_caller_saved_registers))
void draw_load_menu()
{
	V_DrawPatchDirect(97, 17, 0, W_CacheLumpName((uint8_t*)0x00022458 + doom_data_segment, PU_CACHE));
	draw_check_preview();
}

static __attribute((regparm(2),no_caller_saved_registers))
void draw_save_menu()
{
	V_DrawPatchDirect(97, 17, 0, W_CacheLumpName((uint8_t*)0x000224BC + doom_data_segment, PU_CACHE));
	draw_check_preview();
}

//
// API

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void setup_save_slots()
{
	int fd;
	uint8_t name[16];

	show_save_slot = -1;

	for(uint32_t i = 0; i < SAVE_SLOT_COUNT; i++)
	{
		// prepare empty or broken
		strcpy(save_name[i].text, empty_slot);
		save_name[i].step = 0;
		load_items[i].status = 0;

		// try to read
		doom_sprintf(name, SAVE_SLOT_FILE, i);
		fd = doom_open_RD(name);
		if(fd >= 0)
		{
			prepare_save_slot(fd, i);
			doom_close(fd);
		}
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
void select_load(uint32_t slot)
{
	doom_printf("TODO: load %u\n", slot);
	// G_LoadGame(name);
	M_ClearMenus();
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace 'M_ReadSaveStrings'
	{0x00021D70, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)setup_save_slots},
	// replace 'M_LoadSelect'
	{0x00021F80, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)select_load},
	// change text in 'M_SaveSelect'
	{0x00022196, CODE_HOOK | HOOK_UINT32, (uint32_t)empty_slot},
	// replace 'M_DrawLoad' in menu structure
	{0x00012510 + offsetof(menu_t, draw), DATA_HOOK | HOOK_UINT32, (uint32_t)&draw_load_menu},
	{0x00012510 + offsetof(menu_t, x), DATA_HOOK | HOOK_UINT16, 183},
	// replace 'M_DrawSave' in menu structure
	{0x0001258C + offsetof(menu_t, draw), DATA_HOOK | HOOK_UINT32, (uint32_t)&draw_save_menu},
	{0x0001258C + offsetof(menu_t, x), DATA_HOOK | HOOK_UINT16, 183},
	// import variables
	{0x0002B6D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&menu_now},
	{0x0002B568, DATA_HOOK | HOOK_IMPORT, (uint32_t)&save_name},
	{0x000124A8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&load_items},
	{0x00012510, DATA_HOOK | HOOK_IMPORT, (uint32_t)&load_menu},
};

