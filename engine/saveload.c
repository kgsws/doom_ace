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
#include "map.h"
#include "ldr_flat.h"
#include "ldr_texture.h"
#include "filebuf.h"
#include "saveload.h"

#define SAVE_SLOT_COUNT	6
#define SAVE_SLOT_FILE	"save%02u.bmp"
#define SAVE_NAME_SIZE	18
#define SAVE_TITLE_SIZE	28

#define BMP_MAGIC	0x4D42

#define SAVE_MAGIC	0xB1E32A5D	// just a random number
#define SAVE_VERSION	0xE58BAFA1	// increment with 'save_info_t' updates

// save flags
#define CHECK_BIT(f,x)	((f) & (1<<(x)))
enum
{
	SF_SEC_TEXTURE_FLOOR = 16,
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
	SF_SIDE_OFFSX = 16,
	SF_SIDE_OFFSY,
	SF_SIDE_TEXTURE_TOP,
	SF_SIDE_TEXTURE_BOT,
	SF_SIDE_TEXTURE_MID,
};
enum
{
	SF_LINE_FLAGS = 16,
	SF_LINE_SPECIAL,
	SF_LINE_TAG,
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
	uint8_t maplump[8];
	uint64_t mod_csum;
	uint16_t flags;
	uint8_t episode;
	uint8_t map;
	uint32_t leveltime;
	uint32_t kills;
	uint32_t items;
	uint32_t secret;
	uint32_t rng;
} save_info_t;

//

static save_name_t *save_name;
static menuitem_t *load_items;
static menu_t *load_menu;
static uint16_t *menu_now;

static uint8_t *savename;
static uint8_t *savedesc;
static uint32_t *saveslot;

static uint32_t *prndindex;

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
	dst += 320 * 200 + sizeof(patch_t) + 160 * sizeof(uint32_t) + 3 + (99 - y);

	src = screen_buffer;
	src += 320 * 200 * 2;

	doom_lseek(fd, slot->pixoffs + y * slot->width * slot->step, SEEK_SET);
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

static __attribute((noreturn))
void draw_check_preview()
{
	if(show_save_slot != *menu_now)
	{
		// generate preview patch in RAM
		int fd;
		uint8_t *base;
		save_name_t *slot;

		show_save_slot = *menu_now;
		slot = save_name + show_save_slot;

		if(slot->step)
		{
			generate_save_name(show_save_slot);
			fd = doom_open_RD(savename);
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
		uint32_t x = 171;

		if(i == show_save_slot)
			x -= 4;

		draw_slot_bg(x, y, 150);
		M_WriteText(x+1, y, save_name[i].text);
	}

	for(uint32_t i = 0; i < 7; i++)
		draw_slot_bg(5, 52 + i * 14, 164);
	draw_slot_bg(5, 142, 164);

	V_DrawPatchDirect(0, 0, 0, preview_patch);

	menu_skip_draw();
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
		uint64_t tf, tc;
		fixed_t height;

		tf = flat_get_name(sec->floorpic);
		tc = flat_get_name(sec->ceilingpic);

		flags |= (tf != ms[i].wfloor) << SF_SEC_TEXTURE_FLOOR;
		flags |= (tc != ms[i].wceiling) << SF_SEC_TEXTURE_CEILING;

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
			writer_add_u32(flags | i);
			if(CHECK_BIT(flags, SF_SEC_TEXTURE_FLOOR))
				writer_add_wame(&tf);
			if(CHECK_BIT(flags, SF_SEC_TEXTURE_CEILING))
				writer_add_wame(&tc);
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
	writer_add_u32(0);
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
		uint64_t tt, tb, tm;
		fixed_t offs;

		tt = texture_get_name(side->toptexture);
		tb = texture_get_name(side->bottomtexture);
		tm = texture_get_name(side->midtexture);

		offs = (fixed_t)ms[i].textureoffset * FRACUNIT;
		flags |= (side->textureoffset != offs) << SF_SIDE_OFFSX;
		offs = (fixed_t)ms[i].rowoffset * FRACUNIT;
		flags |= (side->rowoffset != offs) << SF_SIDE_OFFSY;

		flags |= (tt != ms[i].wtop) << SF_SIDE_TEXTURE_TOP;
		flags |= (tb != ms[i].wbot) << SF_SIDE_TEXTURE_BOT;
		flags |= (tm != ms[i].wmid) << SF_SIDE_TEXTURE_MID;

		if(flags)
		{
			writer_add_u32(flags | i);
			if(CHECK_BIT(flags, SF_SIDE_OFFSX))
				writer_add_u32(side->textureoffset);
			if(CHECK_BIT(flags, SF_SIDE_OFFSY))
				writer_add_u32(side->rowoffset);
			if(CHECK_BIT(flags, SF_SIDE_TEXTURE_TOP))
				writer_add_wame(&tt);
			if(CHECK_BIT(flags, SF_SIDE_TEXTURE_BOT))
				writer_add_wame(&tb);
			if(CHECK_BIT(flags, SF_SIDE_TEXTURE_MID))
				writer_add_wame(&tm);
		}
	}

	// last entry
	writer_add_u32(0);
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
			writer_add_u32(flags | i);
			if(CHECK_BIT(flags, SF_LINE_FLAGS))
				writer_add_u16(line->flags);
			if(CHECK_BIT(flags, SF_LINE_SPECIAL))
				writer_add_u16(line->special);
			if(CHECK_BIT(flags, SF_LINE_TAG))
				writer_add_u16(line->tag);
		}
	}

	// last entry
	writer_add_u32(0);
}

static __attribute((regparm(2),no_caller_saved_registers))
void do_save()
{
	save_info_t info;
	uint8_t *src;
	uint8_t *dst;

	// prepare save slot
	generate_save_name(*saveslot);
	*gameaction = ga_nothing;

	// generate preview
	R_RenderPlayerView(players + *consoleplayer);
	I_UpdateNoBlit();
	I_ReadScreen(screen_buffer);

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
	strcpy(info.maplump, map_lump_name);
	info.mod_csum = 0; // TODO

	info.flags = *gameskill << 13;
	info.flags |= !!(*fastparm << 0);
	info.flags |= !!(*respawnparm << 1);
	info.flags |= !!(*nomonsters << 2);
	info.flags |= !!(*deathmatch << 3);

	info.episode = *gameepisode;
	info.map = *gamemap;
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

	// DONE
	writer_close();
}

//
// load game

static inline uint32_t ld_get_sectors()
{
	uint32_t flags;

	while(1)
	{
		uint32_t sidx;
		sector_t *sec;
		uint64_t wame;

		reader_get_u32(&flags);
		if(!flags)
			break;

		sidx = flags & 0xFFFF;
		if(sidx >= *numsectors)
			return 1;

		sec = *sectors + sidx;

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
			// TODO: pass inside pointer, relocate later
		}
	}

	// version check
	return reader_get_u32(&flags) || flags != SAVE_VERSION;
}

static inline uint32_t ld_get_sidedefs()
{
	uint32_t flags;

	while(1)
	{
		uint32_t sidx;
		side_t *side;
		uint64_t wame;

		reader_get_u32(&flags);
		if(!flags)
			break;

		sidx = flags & 0xFFFF;
		if(sidx >= *numsides)
			return 1;

		side = *sides + sidx;

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
	return reader_get_u32(&flags) || flags != SAVE_VERSION;
}

static inline uint32_t ld_get_linedefs()
{
	uint32_t flags;

	while(1)
	{
		uint32_t sidx;
		line_t *line;

		reader_get_u32(&flags);
		if(!flags)
			break;

		sidx = flags & 0xFFFF;
		if(sidx >= *numlines)
			return 1;

		line = *lines + sidx;

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
	return reader_get_u32(&flags) || flags != SAVE_VERSION;
}

static __attribute((regparm(2),no_caller_saved_registers))
void do_load()
{
	uint32_t tmp;
	bmp_head_t head;
	save_info_t info;

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
	map_skip_things = 1;
	*gameskill = info.flags >> 13;
	*fastparm = info.flags & 1;
	*respawnparm = !!(info.flags & 2);
	*nomonsters = !!(info.flags & 4);
	*deathmatch = !!(info.flags & 8);
	*gameepisode = info.episode;
	*gamemap = info.map;
	*leveltime = info.leveltime;
	*totalkills = info.kills;
	*totalitems = info.items;
	*totalsecret = info.secret;
	map_load_setup();
	*prndindex = info.rng;

	// sectors
	if(ld_get_sectors())
		goto error_fail;

	// sidedefs
	if(ld_get_sidedefs())
		goto error_fail;

	// linedefs
	if(ld_get_linedefs())
		goto error_fail;

	// DONE
	map_skip_things = 0;
	reader_close();

	return;

	//
error_fail:
	// TODO: don't exit
	I_Error("Load failed!");
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
	{0x00012510 + offsetof(menu_t, x), DATA_HOOK | HOOK_UINT16, 183},
	// replace 'M_DrawSave' in menu structure
	{0x0001258C + offsetof(menu_t, draw), DATA_HOOK | HOOK_UINT32, (uint32_t)&draw_save_menu},
	{0x0001258C + offsetof(menu_t, x), DATA_HOOK | HOOK_UINT16, 183},
	// import variables
	{0x0002B6D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&menu_now},
	{0x0002B568, DATA_HOOK | HOOK_IMPORT, (uint32_t)&save_name},
	{0x000124A8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&load_items},
	{0x00012510, DATA_HOOK | HOOK_IMPORT, (uint32_t)&load_menu},
	{0x0002A780, DATA_HOOK | HOOK_IMPORT, (uint32_t)&savename},
	{0x0002B300, DATA_HOOK | HOOK_IMPORT, (uint32_t)&saveslot},
	{0x0002AC90, DATA_HOOK | HOOK_IMPORT, (uint32_t)&savedesc},
	{0x00012720, DATA_HOOK | HOOK_IMPORT, (uint32_t)&prndindex},
};

