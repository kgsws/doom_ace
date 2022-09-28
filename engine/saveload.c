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
#include "animate.h"
#include "stbar.h"
#include "map.h"
#include "ldr_flat.h"
#include "ldr_texture.h"
#include "filebuf.h"
#include "stbar.h"
#include "think.h"
#include "menu.h"
#include "demo.h"
#include "saveload.h"

#define SAVE_SLOT_COUNT	6
#define SAVE_SLOT_FILE	"save%02u.bmp"
#define SAVE_NAME_SIZE	18
#define SAVE_TITLE_SIZE	28

#define BMP_MAGIC	0x4D42

#define SAVE_MAGIC	0xB1E32A5D	// just a random number
#define SAVE_VERSION	0xE58BAFA4	// increment with updates

// doom special thinkers
#define T_MoveCeiling	0x000263D0
#define T_VerticalDoor	0x00026810
#define T_MoveFloor	0x00029020
#define T_PlatRaise	0x0002CB30
#define T_LightFlash	0x0002A7F0
#define T_StrobeFlash	0x0002A8A0
#define T_Glow	0x0002AAC0
#define T_FireFlicker	0x0002A750

// save flags
#define CHECK_BIT(f,x)	((f) & (1<<(x)))
enum
{
	SF_SEC_TEXTURE_FLOOR,
	SF_SEC_TEXTURE_CEILING,
	SF_SEC_HEIGHT_FLOOR,
	SF_SEC_HEIGHT_CEILING,
	SF_SEC_LIGHT_LEVEL,
	SF_SEC_SPECIAL,
	SF_SEC_TAG,
	SF_SEC_SOUNDTARGET,
};
enum
{
	SF_SIDE_OFFSX,
	SF_SIDE_OFFSY,
	SF_SIDE_TEXTURE_TOP,
	SF_SIDE_TEXTURE_BOT,
	SF_SIDE_TEXTURE_MID,
};
enum
{
	SF_LINE_FLAGS,
	SF_LINE_SPECIAL,
	SF_LINE_TAG,
};

enum
{
	// active buttons
	STH__BUTTON = 0xB6FF,
	STH_BUTTON_FRONT_TOP,
	STH_BUTTON_FRONT_BOT,
	STH_BUTTON_FRONT_MID,
	STH_BUTTON_BACK_TOP,
	STH_BUTTON_BACK_BOT,
	STH_BUTTON_BACK_MID,
	// doom special effects
	STH_DOOM__BASE = 0xD4FF,
	STH_DOOM_CEILING,
	STH_DOOM_DOOR,
	STH_DOOM_FLOOR,
	STH_DOOM_PLAT,
	STH_DOOM_FLASH,
	STH_DOOM_STROBE,
	STH_DOOM_GLOW,
	STH_DOOM_FLICKER,
	// ACE special effects
	STH_ACE__BASE = 0xABFF,
	STH_ACE_LINE_SCROLL,
	STH_ACE_CEILING,
	STH_ACE_FLOOR,
};

//

typedef struct
{ // size must be 24
	uint8_t text[SAVE_NAME_SIZE + 1];
	uint8_t step;
	uint16_t width;
	uint16_t pixoffs;
} save_name_t;

typedef struct
{
	// BMP
	uint16_t magic;
	uint32_t filesize;
	uint32_t unused;
	uint32_t pixoffs;
	// DIB
	uint32_t dibsize;
	uint32_t width;
	uint32_t height;
	uint16_t planes;
	uint16_t bpp;
	uint32_t compression;
	uint32_t datasize;
	uint32_t resh;
	uint32_t resv;
	uint32_t colorcount;
	uint32_t icolor;
} __attribute__((packed)) bmp_head_t;

typedef struct
{
	uint32_t magic;
	uint8_t text[SAVE_TITLE_SIZE];
} save_title_t;

typedef struct
{
	save_title_t title;
	uint32_t version;
	uint64_t map_wame;
	uint64_t mod_csum;
	uint16_t flags;
	uint32_t leveltime;
	uint32_t kills;
	uint32_t items;
	uint32_t secret;
	uint32_t rng;
} save_info_t;

//

typedef struct
{
	// uint64_t type; // type is loaded first
	uint32_t netid;
	//
	fixed_t x, y, z;
	angle_t angle;
	fixed_t pitch;
	fixed_t floorz;
	fixed_t ceilingz;
	fixed_t radius;
	fixed_t height;
	fixed_t mx, my, mz;
	//
	int32_t health;
	int32_t movedir;
	int32_t movecount;
	int32_t reactiontime;
	int32_t threshold;
	int32_t lastlook;
	//
	uint32_t state;
	int32_t tics;
	//
	struct
	{
		int16_t x;
		int16_t y;
		int16_t angle;
		int16_t options;
	} spawn;
	//
	uint32_t flags;
	uint32_t flags1;
	//
	uint32_t target;
	uint32_t tracer;
	uint32_t master;
	//
	uint8_t animation;
	uint8_t __unused;
	uint16_t player;
} save_thing_t;

typedef struct
{
	uint32_t state;
	int32_t tics;
} save_pspr_t;

typedef struct
{
	uint32_t mobj;
	fixed_t viewz;
	fixed_t viewheight;
	fixed_t deltaviewheight;
	//
	int32_t powers[NUMPOWERS];
	int32_t health;
	int32_t armor_points;
	uint64_t armor_type;
	uint64_t weapon_ready;
	uint64_t weapon_pending;
	//
	uint32_t cheats;
	uint32_t refire;
	//
	int32_t killcount;
	int32_t itemcount;
	int32_t secretcount;
	//
	int32_t damagecount;
	int32_t bonuscount;
	//
	fixed_t	pspx;
	fixed_t	pspy;
	save_pspr_t pspr[NUMPSPRITES];
	//
	uint8_t extralight;
	uint8_t usedown;
	uint8_t attackdown;
	uint8_t backpack;
	uint8_t state;
	uint8_t didsecret;
	//
	uint16_t inv_sel;
} save_player_t;

typedef struct
{ // STH_BUTTON_*
	uint64_t texture;
	uint32_t base;
	uint16_t animate;
	uint16_t delay;
	uint16_t line;
} save_switch_t;

typedef struct
{ // STH_DOOM_CEILING
	uint8_t type;
	uint8_t crush;
	uint16_t sector;
	fixed_t bottomheight;
	fixed_t topheight;
	fixed_t speed;
	int8_t direction;
	int8_t olddirection;
	uint16_t tag;
} save_ceiling_t;

typedef struct
{ // STH_DOOM_DOOR
	uint8_t type;
	int8_t direction;
	uint16_t sector;
	fixed_t topheight;
	fixed_t speed;
	uint16_t topwait;
	uint16_t topcountdown;
} save_door_t;

typedef struct
{ // STH_DOOM_FLOOR
	uint8_t type;
	uint8_t crush;
	uint8_t direction;
	uint16_t sector;
	uint16_t newspecial;
	uint64_t texture;
	fixed_t floordestheight;
	fixed_t speed;
} save_floor_t;

typedef struct
{ // STH_DOOM_PLAT
	uint8_t type;
	uint8_t crush;
	uint8_t status;
	uint8_t oldstatus;
	uint16_t wait;
	uint16_t count;
	uint16_t sector;
	uint16_t tag;
	fixed_t speed;
	fixed_t low;
	fixed_t high;
} save_plat_t;

typedef struct
{ // STH_DOOM_FLASH
	uint16_t sector;
	int16_t maxlight;
	int16_t minlight;
	uint8_t count;
	uint8_t maxtime;
	uint8_t mintime;
} save_flash_t;

typedef struct
{ // STH_DOOM_STROBE
	uint16_t sector;
	int16_t minlight;
	int16_t maxlight;
	uint8_t count;
	uint8_t darktime;
	uint8_t brighttime;
} save_strobe_t;

typedef struct
{ // STH_DOOM_GLOW
	uint16_t sector;
	int16_t minlight;
	int16_t maxlight;
	int8_t direction;
} save_glow_t;

typedef struct
{ // STH_DOOM_FLICKER
	uint16_t sector;
	int16_t maxlight;
	int16_t minlight;
	uint8_t count;
} save_flicker_t;

typedef struct
{ // STH_ACE_LINE_SCROLL
	uint16_t line;
	int8_t x, y;
} save_line_scroll_t;

//

static uint32_t *r_setblocks; // TODO: move to 'render'
static uint8_t **r_rdptr;
static uint8_t **r_fbptr;

static uint32_t *brain_sound_id;

static save_name_t *save_name;
static menuitem_t *load_items;
static menu_t *load_menu;

static uint8_t *savename;
static uint8_t *savedesc;
static uint32_t *saveslot;

static patch_t *preview_patch;
static uint_fast8_t show_save_slot = -1;

static const uint8_t empty_slot[] = "[EMPTY]";

// bitmap header
static bmp_head_t bmp_header =
{
	.magic = BMP_MAGIC,
	.filesize = 0xFE36,
	.pixoffs = 0x0436,
	.dibsize = 40,
	.width = 320,
	.height = 200,
	.planes = 1,
	.bpp = 8,
	.datasize = 320 * 200,
	.resh = 0x0B13,
	.resv = 0x0B13,
	.colorcount = 256,
	.icolor = 256,
};

//
// funcs

static void generate_save_name(uint32_t slot)
{
	doom_sprintf(savename, SAVE_SLOT_FILE, slot);
}

static inline void prepare_save_slot(int fd, uint32_t idx)
{
	bmp_head_t head;
	save_info_t info;
	int32_t tmp;

	// header

	tmp = doom_read(fd, &head, sizeof(bmp_head_t));
	if(tmp != sizeof(bmp_head_t))
		return;

	if(head.magic != BMP_MAGIC)
		return;

	if(head.dibsize < 40)
		return;

	// info

	doom_lseek(fd, head.filesize, SEEK_SET);
	tmp = doom_read(fd, &info, sizeof(save_info_t));
	if(tmp != sizeof(save_info_t))
		return;

	if(info.title.magic != 0xB1E32A5D)
		return;

	// create entry
	info.title.text[SAVE_NAME_SIZE] = 0;
	strcpy(save_name[idx].text, info.title.text);
	load_items[idx].status = 1;

	// preview

	if(head.pixoffs > 0xFFFF)
		return;
	if(head.compression)
		return;
	if(head.bpp != 8)
		return;
	if(head.width < 160)
		return;
	if(head.width > 4096)
		return;
	if(head.height < 100)
		return;
	if(head.height > 4096)
		return;

	tmp = 1 + (head.width - 1) / 160;
	if(head.height / tmp < 100)
		return;

	save_name[idx].step = tmp;
	save_name[idx].width = (head.width + 1) & ~3;
	save_name[idx].pixoffs = head.pixoffs;
}

static inline void generate_preview_line(int fd, uint32_t y, save_name_t *slot)
{
	uint8_t *dst;
	uint8_t *src;

	dst = screen_buffer;
	dst += 320 * 200 * 2 + sizeof(patch_t) + 160 * sizeof(uint32_t) + 3 + (99 - y);

	src = screen_buffer;
	src += 320 * 200 * 3;

	doom_lseek(fd, slot->pixoffs + y * slot->width * slot->step, SEEK_SET);
	doom_read(fd, src, 160 * slot->step);

	for(uint32_t i = 0; i < 160; i++)
	{
		*dst = *src;
		src += slot->step;
		dst += 105;
	}
}

static void draw_check_preview()
{
	if(show_save_slot != *menu_item_now)
	{
		// generate preview patch in RAM
		int fd;
		uint8_t *base;
		save_name_t *slot;

		show_save_slot = *menu_item_now;
		slot = save_name + show_save_slot;

		if(slot->step)
		{
			generate_save_name(show_save_slot);
			fd = doom_open_RD(savename);
			if(fd < 0)
				slot->step = 0;
		}

		preview_patch = (patch_t*)(screen_buffer + 320 * 200 * 2);
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
		uint32_t x = 171;

		if(i == show_save_slot)
			x -= 4;

		menu_draw_slot_bg(x, y, 150);
		M_WriteText(x+1, y, save_name[i].text);
	}

	for(uint32_t i = 0; i < 7; i++)
		menu_draw_slot_bg(5, 52 + i * 14, 164);
	menu_draw_slot_bg(5, 142, 164);

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
// save game

static uint32_t sv_convert_state(state_t *st, mobjinfo_t *info)
{
	uint32_t state;

	if(!st || !info)
		return 0x80000000;

	state = st - states;

	if(state < NUMSTATES)
		return state | 0x80000000;
	else
	if(state >= info->state_idx_first && state < info->state_idx_limit)
		return state - info->state_idx_first;
	else
		// this should never happen
		return 0x80000000;
}

static inline void sv_put_sectors(int32_t lump)
{
	map_sector_t *ms;
	uint32_t count;

	ms = W_CacheLumpNum(lump, PU_LEVEL);
	count = W_LumpLength(lump) / sizeof(map_sector_t);

	for(uint32_t i = 0; i < count; i++)
	{
		sector_t *sec = *sectors + i;
		uint32_t flags = 0;
		uint64_t tname;
		fixed_t height;

		flags |= (sec->floorpic != flat_num_get(ms[i].floorpic)) << SF_SEC_TEXTURE_FLOOR;
		flags |= (sec->ceilingpic != flat_num_get(ms[i].ceilingpic)) << SF_SEC_TEXTURE_CEILING;

		height = (fixed_t)ms[i].floorheight * FRACUNIT;
		flags |= (sec->floorheight != height) << SF_SEC_HEIGHT_FLOOR;

		height = (fixed_t)ms[i].ceilingheight * FRACUNIT;
		flags |= (sec->ceilingheight != height) << SF_SEC_HEIGHT_CEILING;

		flags |= (sec->lightlevel != ms[i].lightlevel) << SF_SEC_LIGHT_LEVEL;
		flags |= (sec->special != ms[i].special) << SF_SEC_SPECIAL;
		flags |= (sec->tag != ms[i].tag) << SF_SEC_TAG;

		flags |= (!!sec->soundtarget) << SF_SEC_SOUNDTARGET;

		if(flags)
		{
			writer_add_u32(flags | (i << 16));
			if(CHECK_BIT(flags, SF_SEC_TEXTURE_FLOOR))
			{
				tname = flat_get_name(sec->floorpic);
				writer_add_wame(&tname);
			}
			if(CHECK_BIT(flags, SF_SEC_TEXTURE_CEILING))
			{
				tname = flat_get_name(sec->ceilingpic);
				writer_add_wame(&tname);
			}
			if(CHECK_BIT(flags, SF_SEC_HEIGHT_FLOOR))
				writer_add_u32(sec->floorheight);
			if(CHECK_BIT(flags, SF_SEC_HEIGHT_CEILING))
				writer_add_u32(sec->ceilingheight);
			if(CHECK_BIT(flags, SF_SEC_LIGHT_LEVEL))
				writer_add_u16(sec->lightlevel);
			if(CHECK_BIT(flags, SF_SEC_SPECIAL))
				writer_add_u16(sec->special);
			if(CHECK_BIT(flags, SF_SEC_TAG))
				writer_add_u16(sec->tag);
			if(CHECK_BIT(flags, SF_SEC_SOUNDTARGET))
				writer_add_u32(sec->soundtarget->netid);
		}
	}

	// last entry
	writer_add_u16(0);
}

static inline void sv_put_sidedefs(int32_t lump)
{
	map_sidedef_t *ms;
	uint32_t count;

	ms = W_CacheLumpNum(lump, PU_LEVEL);
	count = W_LumpLength(lump) / sizeof(map_sidedef_t);

	for(uint32_t i = 0; i < count; i++)
	{
		side_t *side = *sides + i;
		uint32_t flags = 0;
		uint64_t tname;
		fixed_t offs;

		offs = (fixed_t)ms[i].textureoffset * FRACUNIT;
		flags |= (side->textureoffset != offs) << SF_SIDE_OFFSX;
		offs = (fixed_t)ms[i].rowoffset * FRACUNIT;
		flags |= (side->rowoffset != offs) << SF_SIDE_OFFSY;

		flags |= (side->toptexture != texture_num_get(ms[i].toptexture)) << SF_SIDE_TEXTURE_TOP;
		flags |= (side->bottomtexture != texture_num_get(ms[i].bottomtexture)) << SF_SIDE_TEXTURE_BOT;
		flags |= (side->midtexture != texture_num_get(ms[i].midtexture)) << SF_SIDE_TEXTURE_MID;

		if(flags)
		{
			writer_add_u32(flags | (i << 16));
			if(CHECK_BIT(flags, SF_SIDE_OFFSX))
				writer_add_u32(side->textureoffset);
			if(CHECK_BIT(flags, SF_SIDE_OFFSY))
				writer_add_u32(side->rowoffset);
			if(CHECK_BIT(flags, SF_SIDE_TEXTURE_TOP))
			{
				tname = texture_get_name(side->toptexture);
				writer_add_wame(&tname);
			}
			if(CHECK_BIT(flags, SF_SIDE_TEXTURE_BOT))
			{
				tname = texture_get_name(side->bottomtexture);
				writer_add_wame(&tname);
			}
			if(CHECK_BIT(flags, SF_SIDE_TEXTURE_MID))
			{
				tname = texture_get_name(side->midtexture);
				writer_add_wame(&tname);
			}
		}
	}

	// last entry
	writer_add_u16(0);
}

static inline void sv_put_linedefs(int32_t lump)
{
	map_linedef_t *ml;
	uint32_t count;

	ml = W_CacheLumpNum(lump, PU_LEVEL);
	count = W_LumpLength(lump) / sizeof(map_linedef_t);

	for(uint32_t i = 0; i < count; i++)
	{
		line_t *line = *lines + i;
		uint32_t flags = 0;

		flags |= (line->flags != ml[i].flags) << SF_LINE_FLAGS;
		flags |= (line->special != ml[i].special) << SF_LINE_SPECIAL;
		flags |= (line->tag != ml[i].tag) << SF_LINE_TAG;

		if(flags)
		{
			writer_add_u32(flags | (i << 16));
			if(CHECK_BIT(flags, SF_LINE_FLAGS))
				writer_add_u16(line->flags);
			if(CHECK_BIT(flags, SF_LINE_SPECIAL))
				writer_add_u16(line->special);
			if(CHECK_BIT(flags, SF_LINE_TAG))
				writer_add_u16(line->tag);
		}
	}

	// last entry
	writer_add_u16(0);
}

static inline void sv_put_buttons()
{
	save_switch_t sw;

	for(uint32_t i = 0; i < MAX_BUTTONS; i++)
	{
		switch_t *slot = active_switch + i;
		uint16_t type;

		type = anim_switch_type(slot);
		if(!type)
			continue;

		writer_add_u16(STH__BUTTON + type);

		sw.texture = anim_switch_texture(slot);
		sw.base = slot->base;
		sw.animate = slot->animate;
		sw.delay = slot->delay;
		sw.line = slot->line - *lines;

		writer_add(&sw, sizeof(sw));
	}

	// no last entry!
}

static inline void sv_put_thinkers()
{
	for(thinker_t *th = thcap.next; th != &thcap; th = th->next)
	{
		if(th->function == think_line_scroll)
		{
			save_line_scroll_t sav;
			line_scroll_t *now = (line_scroll_t*)th;

			writer_add_u16(STH_ACE_LINE_SCROLL);

			sav.line = now->line - *lines;
			sav.x = now->x;
			sav.y = now->y;

			writer_add(&sav, sizeof(sav));
		}
	}
}

static inline void sv_put_specials()
{
	// only old Doom level specials
	for(thinker_t *th = thinkercap->next; th != thinkercap; th = th->next)
	{
		uint32_t ret;
		uint32_t type;

		if(th->function)
			type = (uint32_t)th->function - doom_code_segment;
		else
			type = 0;

		if(!type) // [suspended]
		{
			uint32_t i;

			// find suspended T_MoveCeiling
			for(i = 0; i < MAXCEILINGS; i++)
				if(activeceilings[i] == (ceiling_t*)th)
					break;
			if(i < MAXCEILINGS)
				goto add_ceiling;
			// find suspended T_PlatRaise
			for(i = 0; i < MAXPLATS; i++)
				if(activeplats[i] == (plat_t*)th)
					break;
			if(i < MAXPLATS)
				goto add_plat;
		} else
		if(type == T_MoveCeiling)
		{
add_ceiling:
			{
				save_ceiling_t sav;
				ceiling_t *now = (ceiling_t*)th;

				writer_add_u16(STH_DOOM_CEILING);

				sav.type = now->type;
				sav.crush = now->crush;
				sav.sector = now->sector - *sectors;
				sav.bottomheight = now->bottomheight;
				sav.topheight = now->topheight;
				sav.speed = now->speed;
				sav.direction = now->direction;
				sav.olddirection = now->olddirection;
				sav.tag = now->tag;

				if(!type) // sus!
					sav.type |= 0x80;

				writer_add(&sav, sizeof(sav));
			}
		} else
		if(type == T_VerticalDoor)
		{
			save_door_t sav;
			vldoor_t *now = (vldoor_t*)th;

			writer_add_u16(STH_DOOM_DOOR);

			sav.type = now->type;
			sav.direction = now->direction;
			sav.sector = now->sector - *sectors;
			sav.topheight = now->topheight;
			sav.speed = now->speed;
			sav.topwait = now->topwait;
			sav.topcountdown = now->topcountdown;

			writer_add(&sav, sizeof(sav));
		} else
		if(type == T_MoveFloor)
		{
			save_floor_t sav;
			floormove_t *now = (floormove_t*)th;

			writer_add_u16(STH_DOOM_FLOOR);

			sav.type = now->type;
			sav.crush = now->crush;
			sav.direction = now->direction;
			sav.sector = now->sector - *sectors;
			sav.newspecial = now->newspecial;
			sav.texture = texture_get_name(now->texture);
			sav.floordestheight = now->floordestheight;
			sav.speed = now->speed;

			writer_add(&sav, sizeof(sav));
		} else
		if(type == T_PlatRaise)
		{
add_plat:
			{
				save_plat_t sav;
				plat_t *now = (plat_t*)th;

				writer_add_u16(STH_DOOM_PLAT);

				sav.type = now->type;
				sav.crush = now->crush;
				sav.status = now->status;
				sav.oldstatus = now->oldstatus;
				sav.wait = now->wait;
				sav.count = now->count;
				sav.sector = now->sector - *sectors;
				sav.tag = now->tag;
				sav.speed = now->speed;
				sav.low = now->low;
				sav.high = now->high;

				if(!type) // sus!
					sav.type |= 0x80;

				writer_add(&sav, sizeof(sav));
			}
		} else
		if(type == T_LightFlash)
		{
			save_flash_t sav;
			lightflash_t *now = (lightflash_t*)th;

			writer_add_u16(STH_DOOM_FLASH);

			sav.sector = now->sector - *sectors;
			sav.maxlight = now->maxlight;
			sav.minlight = now->minlight;
			sav.count = now->count;
			sav.maxtime = now->maxtime;
			sav.mintime = now->mintime;

			writer_add(&sav, sizeof(sav));
		} else
		if(type == T_StrobeFlash)
		{
			save_strobe_t sav;
			strobe_t *now = (strobe_t*)th;

			writer_add_u16(STH_DOOM_STROBE);

			sav.sector = now->sector - *sectors;
			sav.minlight = now->minlight;
			sav.maxlight = now->maxlight;
			sav.count = now->count;
			sav.darktime = now->darktime;
			sav.brighttime = now->brighttime;

			writer_add(&sav, sizeof(sav));
		} else
		if(type == T_Glow)
		{
			save_glow_t sav;
			glow_t *now = (glow_t*)th;

			writer_add_u16(STH_DOOM_GLOW);

			sav.sector = now->sector - *sectors;
			sav.minlight = now->minlight;
			sav.maxlight = now->maxlight;
			sav.direction = now->direction;

			writer_add(&sav, sizeof(sav));
		} else
		if(type == T_FireFlicker)
		{
			save_flicker_t sav;
			fireflicker_t *now = (fireflicker_t*)th;

			writer_add_u16(STH_DOOM_FLICKER);

			sav.sector = now->sector - *sectors;
			sav.maxlight = now->maxlight;
			sav.minlight = now->minlight;
			sav.count = now->count;

			writer_add(&sav, sizeof(sav));
		}
	}

	// last entry
	writer_add_u16(0);
}

static uint32_t svcb_thing(mobj_t *mo)
{
	save_thing_t thing;
	uint32_t state;
	inventory_t *item;

	writer_add_wame(&mo->info->alias);

	thing.netid = mo->netid;

	thing.x = mo->x;
	thing.y = mo->y;
	thing.z = mo->z;
	thing.angle = mo->angle;
	thing.pitch = mo->pitch;
	thing.floorz = mo->floorz;
	thing.ceilingz = mo->ceilingz;
	thing.radius = mo->radius;
	thing.height = mo->height;
	thing.mx = mo->momx;
	thing.my = mo->momy;
	thing.mz = mo->momz;

	thing.health = mo->health;
	thing.movedir = mo->movedir;
	thing.movecount = mo->movecount;
	thing.reactiontime = mo->reactiontime;
	thing.threshold = mo->threshold;
	thing.lastlook = mo->lastlook;

	thing.state = sv_convert_state(mo->state, mo->info);
	thing.tics = mo->tics;

	thing.spawn.x = mo->spawnpoint.x;
	thing.spawn.y = mo->spawnpoint.y;
	thing.spawn.angle = mo->spawnpoint.angle;
	thing.spawn.options = mo->spawnpoint.options;

	thing.flags = mo->flags;
	thing.flags1 = mo->flags1;

	thing.target = mo->target ? mo->target->netid : 0;
	thing.tracer = mo->tracer ? mo->tracer->netid : 0;
	thing.master = mo->master ? mo->master->netid : 0;

	thing.animation = mo->animation;
	thing.__unused = 0;
	if(mo->player)
		thing.player = 1 + (mo->player - players);
	else
		thing.player = 0;

	writer_add(&thing, sizeof(thing));

	// inventory

	if(mo->inventory)
	{
		// rewind
		item = mo->inventory;
		while(item->prev)
			item = item->prev;

		// save
		while(item)
		{
			writer_add_wame(&mobjinfo[item->type].alias);
			writer_add_u16(item->count);
			item = item->next;
		}
	}

	// last item
	uint64_t alias = 0;
	writer_add_wame(&alias);

	return 0;
}

static inline void sv_put_things()
{
	mobj_for_each(svcb_thing);
	// last entry
	uint64_t alias = 0;
	writer_add_wame(&alias);
}

static inline void sv_put_players()
{
	save_player_t plr;

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		player_t *pl;

		if(!playeringame[i])
			continue;

		pl = players + i;

		writer_add_u16(i + 1);

		plr.mobj = pl->mo->netid;
		plr.viewz = pl->viewz;
		plr.viewheight = pl->viewheight;
		plr.deltaviewheight = pl->deltaviewheight;

		for(uint32_t j = 0; j < NUMPOWERS; j++)
			plr.powers[j] = pl->powers[j];

		plr.health = pl->health;
		plr.armor_points = pl->armorpoints;
		plr.armor_type = pl->armortype ? mobjinfo[pl->armortype].alias : 0;
		plr.weapon_ready = pl->readyweapon ? pl->readyweapon->alias : 0;
		plr.weapon_pending = pl->pendingweapon ? pl->pendingweapon->alias : 0;

		plr.cheats = pl->cheats;
		plr.refire = pl->refire;

		plr.killcount = pl->killcount;
		plr.itemcount = pl->itemcount;
		plr.secretcount = pl->secretcount;

		plr.damagecount = pl->damagecount;
		plr.bonuscount = pl->bonuscount;

		plr.pspx = pl->psprites[0].sx;
		plr.pspy = pl->psprites[0].sy;

		for(uint32_t j = 0; j < NUMPSPRITES; j++)
		{
			plr.pspr[j].state = sv_convert_state(pl->psprites[j].state, pl->readyweapon);
			plr.pspr[j].tics = pl->psprites[j].tics;
		}

		plr.extralight = pl->extralight;
		plr.usedown = pl->usedown;
		plr.attackdown = pl->attackdown;
		plr.backpack = pl->backpack;
		plr.state = pl->playerstate;
		plr.didsecret = pl->didsecret;

		plr.inv_sel = pl->inv_sel ? pl->inv_sel->type : 0;

		writer_add(&plr, sizeof(plr));
	}

	// last entry
	writer_add_u16(0);
}

static __attribute((regparm(2),no_caller_saved_registers))
void do_save()
{
	save_info_t info;
	uint8_t *src;
	uint8_t *dst;
	uint32_t old_size;
	uint32_t old_cmap;

	// prepare save slot
	generate_save_name(*saveslot);
	*gameaction = ga_nothing;

	// generate preview - so much stuff to make it look cool
	old_cmap = players[*consoleplayer].fixedcolormap;
	players[*consoleplayer].fixedcolormap = 0;
	old_size = *r_setblocks;
	*r_setblocks = 20; // fullscreen with no status bar
	R_ExecuteSetViewSize();
	R_RenderPlayerView(players + *consoleplayer);
	*r_rdptr = *r_fbptr; // fullscreen hack
	I_ReadScreen(screen_buffer);
	*r_setblocks = old_size;
	R_ExecuteSetViewSize();
	players[*consoleplayer].fixedcolormap = old_cmap;
	*stbar_refresh_force = 1;

	// open file
	writer_open(savename);

	// bitmap header
	writer_add(&bmp_header, sizeof(bmp_header));

	// palette
	src = W_CacheLumpName((uint8_t*)0x0001FD14 + doom_data_segment, PU_CACHE);
	dst = writer_reserve(256 * sizeof(uint32_t));
	for(uint32_t i = 0; i < 256; i++)
	{
		*dst++ = src[2];
		*dst++ = src[1];
		*dst++ = src[0];
		*dst++ = 0;
		src += 3;
	}

	// pixels
	src = screen_buffer + 320 * 200;
	for(uint32_t i = 0; i < 200; i++)
	{
		src -= 320;
		writer_add(src, 320);
	}

	// title
	savedesc[SAVE_TITLE_SIZE] = 0;
	info.title.magic = SAVE_MAGIC;
	strcpy(info.title.text, savedesc);

	// game info
	info.version = SAVE_VERSION;
	info.map_wame = map_lump.wame;
	info.mod_csum = 0; // TODO

	info.flags = *gameskill << 13;
	info.flags |= (!!*fastparm) << 0;
	info.flags |= (!!*respawnparm) << 1;
	info.flags |= (!!*nomonsters) << 2;
	info.flags |= (!!*deathmatch) << 3;

	info.leveltime = *leveltime;

	info.kills = *totalkills;
	info.items = *totalitems;
	info.secret = *totalsecret;
	info.rng = *prndindex;

	writer_add(&info, sizeof(info));
	writer_add_u32(SAVE_VERSION);

	// sectors
	sv_put_sectors(map_lump_idx + ML_SECTORS);
	writer_add_u32(SAVE_VERSION);

	// sidedefs
	sv_put_sidedefs(map_lump_idx + ML_SIDEDEFS);
	writer_add_u32(SAVE_VERSION);

	// linedefs
	sv_put_linedefs(map_lump_idx + ML_LINEDEFS);
	writer_add_u32(SAVE_VERSION);

	// specials
	sv_put_buttons();
	sv_put_thinkers();
	sv_put_specials();
	writer_add_u32(SAVE_VERSION);

	// things
	sv_put_things();
	writer_add_u32(SAVE_VERSION);

	// players
	sv_put_players();
	writer_add_u32(SAVE_VERSION);

	// DONE
	writer_close();
}

//
// load game

static state_t *ld_convert_state(uint32_t state, mobjinfo_t *info, uint32_t allow_null)
{
	if(!info)
		state = 0x80000000;

	if(state & 0x80000000)
	{
		state &= 0x7FFFFFFF;
		if(state >= NUMSTATES)
			state = 0;
	} else
	{
		state += info->state_idx_first;
		if(state >= info->state_idx_limit)
			state = 0;
	}

	if(!state && allow_null)
		return NULL;

	return states + state;
}

static uint32_t ld_get_armor(uint64_t alias)
{
	int32_t type;
	mobjinfo_t *info;

	type = mobj_check_type(alias);
	if(type < 0)
		return 0;

	info = mobjinfo + type;
	if(info->extra_type != ETYPE_ARMOR && info->extra_type != ETYPE_ARMOR_BONUS)
		return 0;

	return type;
}

static mobjinfo_t *ld_get_weapon(uint64_t alias)
{
	int32_t type;
	mobjinfo_t *info;

	type = mobj_check_type(alias);
	if(type < 0)
		return NULL;

	info = mobjinfo + type;
	if(info->extra_type != ETYPE_WEAPON)
		return NULL;

	return info;
}

static inline uint32_t ld_get_sectors()
{
	while(1)
	{
		uint16_t flags;
		uint16_t idx;
		sector_t *sec;
		uint64_t wame;

		if(reader_get_u16(&flags))
			return 1;
		if(!flags)
			break;

		if(reader_get_u16(&idx))
			return 1;

		if(idx >= *numsectors)
			return 1;

		sec = *sectors + idx;

		if(CHECK_BIT(flags, SF_SEC_TEXTURE_FLOOR))
		{
			if(reader_get_wame(&wame))
				return 1;
			sec->floorpic = flat_num_get((uint8_t*)&wame);
		}
		if(CHECK_BIT(flags, SF_SEC_TEXTURE_CEILING))
		{
			if(reader_get_wame(&wame))
				return 1;
			sec->ceilingpic = flat_num_get((uint8_t*)&wame);
		}
		if(CHECK_BIT(flags, SF_SEC_HEIGHT_FLOOR))
		{
			if(reader_get_u32(&sec->floorheight))
				return 1;
		}
		if(CHECK_BIT(flags, SF_SEC_HEIGHT_CEILING))
		{
			if(reader_get_u32(&sec->ceilingheight))
				return 1;
		}
		if(CHECK_BIT(flags, SF_SEC_LIGHT_LEVEL))
		{
			if(reader_get_u16(&sec->lightlevel))
				return 1;
		}
		if(CHECK_BIT(flags, SF_SEC_SPECIAL))
		{
			if(reader_get_u16(&sec->special))
				return 1;
		}
		if(CHECK_BIT(flags, SF_SEC_TAG))
		{
			if(reader_get_u16(&sec->tag))
				return 1;
		}
		if(CHECK_BIT(flags, SF_SEC_SOUNDTARGET))
		{
			uint32_t nid;
			if(reader_get_u32(&nid))
				return 1;
			sec->soundtarget = (mobj_t*)nid;
		}
	}

	// version check
	uint32_t version;
	return reader_get_u32(&version) || version != SAVE_VERSION;
}

static inline uint32_t ld_get_sidedefs()
{
	while(1)
	{
		uint16_t flags;
		uint16_t idx;
		side_t *side;
		uint64_t wame;

		if(reader_get_u16(&flags))
			return 1;
		if(!flags)
			break;

		if(reader_get_u16(&idx))
			return 1;

		if(idx >= *numsides)
			return 1;

		side = *sides + idx;

		if(CHECK_BIT(flags, SF_SIDE_OFFSX))
		{
			if(reader_get_u32(&side->textureoffset))
				return 1;
		}
		if(CHECK_BIT(flags, SF_SIDE_OFFSY))
		{
			if(reader_get_u32(&side->rowoffset))
				return 1;
		}
		if(CHECK_BIT(flags, SF_SIDE_TEXTURE_TOP))
		{
			if(reader_get_wame(&wame))
				return 1;
			side->toptexture = texture_num_get((uint8_t*)&wame);
		}
		if(CHECK_BIT(flags, SF_SIDE_TEXTURE_BOT))
		{
			if(reader_get_wame(&wame))
				return 1;
			side->bottomtexture = texture_num_get((uint8_t*)&wame);
		}
		if(CHECK_BIT(flags, SF_SIDE_TEXTURE_MID))
		{
			if(reader_get_wame(&wame))
				return 1;
			side->midtexture = texture_num_get((uint8_t*)&wame);
		}
	}

	// version check
	uint32_t version;
	return reader_get_u32(&version) || version != SAVE_VERSION;
}

static inline uint32_t ld_get_linedefs()
{
	while(1)
	{
		uint16_t flags;
		uint16_t idx;
		line_t *line;

		if(reader_get_u16(&flags))
			return 1;
		if(!flags)
			break;

		if(reader_get_u16(&idx))
			return 1;

		if(idx >= *numlines)
			return 1;

		line = *lines + idx;

		if(CHECK_BIT(flags, SF_LINE_FLAGS))
		{
			if(reader_get_u16(&line->flags))
				return 1;
		}
		if(CHECK_BIT(flags, SF_LINE_SPECIAL))
		{
			if(reader_get_u16(&line->special))
				return 1;
		}
		if(CHECK_BIT(flags, SF_LINE_TAG))
		{
			if(reader_get_u16(&line->tag))
				return 1;
		}
	}

	// version check
	uint32_t version;
	return reader_get_u32(&version) || version != SAVE_VERSION;
}

static inline uint32_t ld_get_specials()
{
	while(1)
	{
		uint16_t type;

		if(reader_get_u16(&type))
			return 1;

		if(!type)
			break;

		switch(type)
		{
			case STH_BUTTON_FRONT_TOP:
			case STH_BUTTON_FRONT_BOT:
			case STH_BUTTON_FRONT_MID:
			case STH_BUTTON_BACK_TOP:
			case STH_BUTTON_BACK_BOT:
			case STH_BUTTON_BACK_MID:
			{
				save_switch_t sw;
				switch_t *slot;

				if(reader_get(&sw, sizeof(sw)))
					return 1;

				if(sw.line >= *numlines)
					return 1;

				type -= STH__BUTTON;
				slot = anim_switch_make(type, *lines + sw.line, sw.texture);
				if(!slot)
					break;

				slot->base = sw.base;
				slot->animate = sw.animate;
				slot->delay = sw.delay;
			}
			break;
			// new thinkers
			case STH_ACE_LINE_SCROLL:
			{
				save_line_scroll_t sav;
				line_scroll_t *now;

				if(reader_get(&sav, sizeof(sav)))
					return 1;

				if(sav.line >= *numlines)
					return 1;

				now = Z_Malloc(sizeof(line_scroll_t), PU_LEVEL, NULL);

				now->line = *lines + sav.line;
				now->x = sav.x;
				now->y = sav.y;

				now->thinker.function = think_line_scroll;

				think_add(&now->thinker);
			}
			break;
			// old thinkers
			case STH_DOOM_CEILING:
			{
				save_ceiling_t sav;
				ceiling_t *now;

				if(reader_get(&sav, sizeof(sav)))
					return 1;

				if(sav.sector >= *numsectors)
					return 1;

				now = Z_Malloc(sizeof(ceiling_t), PU_LEVEL, NULL);

				now->type = sav.type & 0x7F;
				now->crush = sav.crush;
				now->sector = *sectors + sav.sector;
				now->bottomheight = sav.bottomheight;
				now->topheight = sav.topheight;
				now->speed = sav.speed;
				now->direction = sav.direction;
				now->olddirection = sav.olddirection;
				now->tag = sav.tag;

				now->sector->specialdata = now;
				now->thinker.function = (void*)T_MoveCeiling + doom_code_segment;

				P_AddThinker(&now->thinker);
				P_AddActiveCeiling(now);

				if(sav.type & 0x80) // sus!
					now->thinker.function = NULL;
			}
			break;
			case STH_DOOM_DOOR:
			{
				save_door_t sav;
				vldoor_t *now;

				if(reader_get(&sav, sizeof(sav)))
					return 1;

				if(sav.sector >= *numsectors)
					return 1;

				now = Z_Malloc(sizeof(vldoor_t), PU_LEVEL, NULL);

				now->type = sav.type;
				now->direction = sav.direction;
				now->sector = *sectors + sav.sector;
				now->topheight = sav.topheight;
				now->speed = sav.speed;
				now->topwait = sav.topwait;
				now->topcountdown = sav.topcountdown;

				now->sector->specialdata = now;
				now->thinker.function = (void*)T_VerticalDoor + doom_code_segment;

				P_AddThinker(&now->thinker);
			}
			break;
			case STH_DOOM_FLOOR:
			{
				save_floor_t sav;
				floormove_t *now;

				if(reader_get(&sav, sizeof(sav)))
					return 1;

				if(sav.sector >= *numsectors)
					return 1;

				now = Z_Malloc(sizeof(floormove_t), PU_LEVEL, NULL);

				now->type = sav.type;
				now->crush = sav.crush;
				now->direction = sav.direction;
				now->sector = *sectors + sav.sector;
				now->newspecial = sav.newspecial;
				now->texture = texture_num_get((uint8_t*)&sav.texture);
				now->floordestheight = sav.floordestheight;
				now->speed = sav.speed;

				now->sector->specialdata = now;
				now->thinker.function = (void*)T_MoveFloor + doom_code_segment;

				P_AddThinker(&now->thinker);
			}
			break;
			case STH_DOOM_PLAT:
			{
				save_plat_t sav;
				plat_t *now;

				if(reader_get(&sav, sizeof(sav)))
					return 1;

				if(sav.sector >= *numsectors)
					return 1;

				now = Z_Malloc(sizeof(plat_t), PU_LEVEL, NULL);

				now->type = sav.type & 0x7F;
				now->crush = sav.crush;
				now->status = sav.status;
				now->oldstatus = sav.oldstatus;
				now->wait = sav.wait;
				now->count = sav.count;
				now->sector = *sectors + sav.sector;
				now->tag = sav.tag;
				now->speed = sav.speed;
				now->low = sav.low;
				now->high = sav.high;

				now->sector->specialdata = now;
				now->thinker.function = (void*)T_PlatRaise + doom_code_segment;

				P_AddThinker(&now->thinker);
				P_AddActivePlat(now);

				if(sav.type & 0x80) // sus!
					now->thinker.function = NULL;
			}
			break;
			case STH_DOOM_FLASH:
			{
				save_flash_t sav;
				lightflash_t *now;

				if(reader_get(&sav, sizeof(sav)))
					return 1;

				if(sav.sector >= *numsectors)
					return 1;

				now = Z_Malloc(sizeof(lightflash_t), PU_LEVEL, NULL);

				now->sector = *sectors + sav.sector;
				now->maxlight = sav.maxlight;
				now->minlight = sav.minlight;
				now->count = sav.count;
				now->maxtime = sav.maxtime;
				now->mintime = sav.mintime;

				now->thinker.function = (void*)T_LightFlash + doom_code_segment;

				P_AddThinker(&now->thinker);
			}
			break;
			case STH_DOOM_STROBE:
			{
				save_strobe_t sav;
				strobe_t *now;

				if(reader_get(&sav, sizeof(sav)))
					return 1;

				if(sav.sector >= *numsectors)
					return 1;

				now = Z_Malloc(sizeof(strobe_t), PU_LEVEL, NULL);

				now->sector = *sectors + sav.sector;
				now->minlight = sav.minlight;
				now->maxlight = sav.maxlight;
				now->count = sav.count;
				now->darktime = sav.darktime;
				now->brighttime = sav.brighttime;

				now->thinker.function = (void*)T_StrobeFlash + doom_code_segment;

				P_AddThinker(&now->thinker);
			}
			break;
			case STH_DOOM_GLOW:
			{
				save_glow_t sav;
				glow_t *now;

				if(reader_get(&sav, sizeof(sav)))
					return 1;

				if(sav.sector >= *numsectors)
					return 1;

				now = Z_Malloc(sizeof(glow_t), PU_LEVEL, NULL);

				now->sector = *sectors + sav.sector;
				now->minlight = sav.minlight;
				now->maxlight = sav.maxlight;
				now->direction = sav.direction;

				now->thinker.function = (void*)T_Glow + doom_code_segment;

				P_AddThinker(&now->thinker);
			}
			break;
			case STH_DOOM_FLICKER:
			{
				save_flicker_t sav;
				fireflicker_t *now;

				if(reader_get(&sav, sizeof(sav)))
					return 1;

				if(sav.sector >= *numsectors)
					return 1;

				now = Z_Malloc(sizeof(fireflicker_t), PU_LEVEL, NULL);

				now->sector = *sectors + sav.sector;
				now->maxlight = sav.maxlight;
				now->minlight = sav.minlight;
				now->count = sav.count;

				now->thinker.function = (void*)T_FireFlicker + doom_code_segment;

				P_AddThinker(&now->thinker);
			}
			break;
			default:
				// unknown!
				return 1;
		}
	}

	// version check
	uint32_t version;
	return reader_get_u32(&version) || version != SAVE_VERSION;
}

static uint32_t ldcb_thing(mobj_t *mo)
{
	mo->target = mobj_by_netid((uint32_t)mo->target);
	mo->tracer = mobj_by_netid((uint32_t)mo->tracer);
	mo->master = mobj_by_netid((uint32_t)mo->master);
}

static inline uint32_t ld_get_things()
{
	save_thing_t thing;
	uint64_t alias;
	int32_t type;
	uint32_t maxnetid = 0;
	mobj_t *mo;
	inventory_t *item, *ilst;

	while(1)
	{
		if(reader_get_wame(&alias))
			return 1;
		if(!alias)
			break;

		type = mobj_check_type(alias);
		if(type < 0)
			return 1;

		reader_get(&thing, sizeof(thing));

		if(thing.netid > maxnetid)
			maxnetid = thing.netid;

		mo = P_SpawnMobj(thing.x, thing.y, thing.z, type);

		mo->netid = thing.netid;

		mo->angle = thing.angle;
		mo->pitch = thing.pitch;
		mo->floorz = thing.floorz;
		mo->ceilingz = thing.ceilingz;
		mo->radius = thing.radius;
		mo->height = thing.height;
		mo->momx = thing.mx;
		mo->momy = thing.my;
		mo->momz = thing.mz;

		mo->health = thing.health;
		mo->movedir = thing.movedir;
		mo->movecount = thing.movecount;
		mo->reactiontime = thing.reactiontime;
		mo->threshold = thing.threshold;
		mo->lastlook = thing.lastlook;

		mo->state = ld_convert_state(thing.state, mo->info, 0);
		mo->tics = thing.tics;
		mo->sprite = mo->state->sprite;
		mo->frame = mo->state->frame;

		mo->spawnpoint.x = thing.spawn.x;
		mo->spawnpoint.y = thing.spawn.y;
		mo->spawnpoint.angle = thing.spawn.angle;
		mo->spawnpoint.type = type;
		mo->spawnpoint.options = thing.spawn.options;

		mo->flags = thing.flags;
		mo->flags1 = thing.flags1;

		mo->target = (mobj_t*)thing.target;
		mo->tracer = (mobj_t*)thing.tracer;
		mo->master = (mobj_t*)thing.master;

		mo->animation = thing.animation;
		if(thing.player)
		{
			thing.player--;
			if(thing.player < MAXPLAYERS)
				mo->player = players + thing.player;
			else
				mo->player = NULL;
		} else
			mo->player = NULL;

		// inventory
		ilst = NULL;
		while(1)
		{
			uint16_t count;

			if(reader_get_wame(&alias))
				return 1;
			if(!alias)
				break;

			type = mobj_check_type(alias);
			if(type < 0)
				return 1;

			if(reader_get_u16(&count))
				return 1;

			if(!inventory_is_valid(mobjinfo + type))
				continue;

			item = doom_malloc(sizeof(inventory_t));
			item->prev = ilst;
			item->next = NULL;
			item->type = type;
			item->count = count;
			if(ilst)
				ilst->next = item;
			mo->inventory = item;
			ilst = item;
		}
	}

	// relocate mobj pointers
	mobj_for_each(ldcb_thing);

	// relocate soundtargets
	for(uint32_t i = 0; i < *numsectors; i++)
	{
		sector_t *sec = *sectors + i;
		sec->soundtarget = mobj_by_netid((uint32_t)sec->soundtarget);
	}

	mobj_netid = maxnetid + 1;

	// version check
	return reader_get_u32(&type) || type != SAVE_VERSION;
}

static inline uint32_t ld_get_players()
{
	save_player_t plr;

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
		playeringame[i] = 0;

	while(1)
	{
		uint16_t idx;
		player_t *pl;

		if(reader_get_u16(&idx))
			return 1;
		if(!idx)
			break;

		idx--;
		if(idx >= MAXPLAYERS)
			return 1;
		pl = players + idx;

		if(reader_get(&plr, sizeof(plr)))
			return 1;

		if(plr.state >= PST_REBORN)
			return 1;

		pl->mo = mobj_by_netid(plr.mobj);
		if(!pl->mo)
			return 1;

		playeringame[idx] = 1;

		pl->viewz = plr.viewz;
		pl->viewheight = plr.viewheight;
		pl->deltaviewheight = plr.deltaviewheight;

		for(uint32_t i = 0; i < NUMPOWERS; i++)
			pl->powers[i] = plr.powers[i];

		pl->health = plr.health;
		pl->armorpoints = plr.armor_points;
		pl->armortype = ld_get_armor(plr.armor_type);
		pl->readyweapon = ld_get_weapon(plr.weapon_ready);
		pl->pendingweapon = ld_get_weapon(plr.weapon_pending);

		pl->cheats = plr.cheats;
		pl->refire = plr.refire;

		pl->killcount = plr.killcount;
		pl->itemcount = plr.itemcount;
		pl->secretcount = plr.secretcount;

		pl->damagecount = plr.damagecount;
		pl->bonuscount = plr.bonuscount;

		pl->psprites[0].sx = plr.pspx;
		pl->psprites[0].sy = plr.pspy;

		for(uint32_t i = 0; i < NUMPSPRITES; i++)
		{
			pl->psprites[i].state = ld_convert_state(plr.pspr[i].state, pl->readyweapon, 1);
			pl->psprites[i].tics = plr.pspr[i].tics;
		}

		pl->extralight = plr.extralight;
		pl->usedown = plr.usedown;
		pl->attackdown = plr.attackdown;
		pl->backpack = plr.backpack;
		pl->playerstate = plr.state;
		pl->didsecret = plr.didsecret;

		pl->inv_sel = inventory_find(pl->mo, plr.inv_sel);
	}

	// version check
	uint32_t version;
	return reader_get_u32(&version) || version != SAVE_VERSION;
}

static __attribute((regparm(2),no_caller_saved_registers))
void do_load()
{
	uint32_t tmp;
	bmp_head_t head;
	save_info_t info;
	player_t *pl;

	// prepare save slot
	generate_save_name(*saveslot);
	*gameaction = ga_nothing;

	// open file
	reader_open(savename);

	// skip bitmap stuff
	if(reader_get(&head, sizeof(head)))
		goto error_fail;
	if(reader_seek(head.filesize))
		goto error_fail;

	// game info
	if(reader_get(&info, sizeof(info)))
		goto error_fail;
	if(reader_get_u32(&tmp) || tmp != SAVE_VERSION)
		goto error_fail;

	// invalidate player links
	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		if(playeringame[i] && players[i].mo)
		{
			players[i].mo->player = NULL;
			players[i].mo = NULL;
		}
	}

	// load map
	map_skip_stuff = 1;
	*gameskill = info.flags >> 13;
	*fastparm = info.flags & 1;
	*respawnparm = !!(info.flags & 2);
	*nomonsters = !!(info.flags & 4);
	*deathmatch = !!(info.flags & 8);

	map_lump.wame = info.map_wame;

	if(*gameskill > sk_nightmare)
		goto error_fail;

	*netgame = 0; // TODO: net saves?
	*netdemo = 0;

	map_load_setup();

	*leveltime = info.leveltime;
	*totalkills = info.kills;
	*totalitems = info.items;
	*totalsecret = info.secret;

	// sectors
	if(ld_get_sectors())
		goto error_fail;

	// sidedefs
	if(ld_get_sidedefs())
		goto error_fail;

	// linedefs
	if(ld_get_linedefs())
		goto error_fail;

	// specials
	if(ld_get_specials())
		goto error_fail;

	// things
	if(ld_get_things())
		goto error_fail;

	// players
	if(ld_get_players())
		goto error_fail;
	if(!playeringame[*consoleplayer])
		goto error_fail;
	pl = players + *consoleplayer;
	if(pl->mo->info->extra_type != ETYPE_PLAYERPAWN)
		goto error_fail;

	// count brain targets; hack + silence
	*brain_sound_id = 0;
	doom_A_BrainAwake(NULL);
	*brain_sound_id = 0x60;

	stbar_start(pl);
	HU_Start();
	player_viewheight(pl->mo->info->player.view_height);

	// DONE
	*prndindex = info.rng;
	map_skip_stuff = 0;
	reader_close();

	return;

	//
error_fail:
	reader_close();
	error_message("Unable to load this slot!");
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void setup_save_slots()
{
	int fd;

	show_save_slot = -1;

	for(uint32_t i = 0; i < SAVE_SLOT_COUNT; i++)
	{
		// prepare empty or broken
		strcpy(save_name[i].text, empty_slot);
		save_name[i].step = 0;
		load_items[i].status = 0;

		// try to read
		generate_save_name(i);
		fd = doom_open_RD(savename);
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
	*saveslot = slot;
	*gameaction = ga_loadgame;
	M_ClearMenus();
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'G_DoLoadGame' in 'G_Ticker'
	{0x00020515, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)do_load},
	// replace call to 'G_DoSaveGame' in 'G_Ticker'
	{0x0002051F, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)do_save},
	// replace 'M_ReadSaveStrings'
	{0x00021D70, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)setup_save_slots},
	// replace 'M_LoadSelect'
	{0x00021F80, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)select_load},
	// change text in 'M_SaveSelect'
	{0x00022196, CODE_HOOK | HOOK_UINT32, (uint32_t)empty_slot},
	// replace 'M_DrawLoad' in menu structure
	{0x00012510 + offsetof(menu_t, draw), DATA_HOOK | HOOK_UINT32, (uint32_t)&draw_load_menu},
	{0x00012510 + offsetof(menu_t, x), DATA_HOOK | HOOK_UINT16, -1},
	// replace 'M_DrawSave' in menu structure
	{0x0001258C + offsetof(menu_t, draw), DATA_HOOK | HOOK_UINT32, (uint32_t)&draw_save_menu},
	{0x0001258C + offsetof(menu_t, x), DATA_HOOK | HOOK_UINT16, -1},
	// brain targets hack
	{0x00028AFC, CODE_HOOK | HOOK_IMPORT, (uint32_t)&brain_sound_id},
	// import variables
	{0x0002B568, DATA_HOOK | HOOK_IMPORT, (uint32_t)&save_name},
	{0x000124A8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&load_items},
	{0x00012510, DATA_HOOK | HOOK_IMPORT, (uint32_t)&load_menu},
	{0x0002A780, DATA_HOOK | HOOK_IMPORT, (uint32_t)&savename},
	{0x0002B300, DATA_HOOK | HOOK_IMPORT, (uint32_t)&saveslot},
	{0x0002AC90, DATA_HOOK | HOOK_IMPORT, (uint32_t)&savedesc},
	// extra, temporary
	{0x00038FE0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&r_setblocks},
	{0x000290F8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&r_rdptr},
	{0x0002914C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&r_fbptr},
};

