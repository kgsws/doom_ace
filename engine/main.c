// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "dehacked.h"
#include "decorate.h"
#include "animate.h"
#include "render.h"
#include "draw.h"
#include "sound.h"
#include "map.h"
#include "menu.h"
#include "stbar.h"
#include "config.h"
#include "rng.h"
#include "demo.h"
#include "ldr_texture.h"
#include "ldr_flat.h"
#include "ldr_sprite.h"

//#define DEBUG_CRASH
//#define DUMP_VRAM
//#define RAM_DEBUG

#define LDR_ENGINE_COUNT	7	// dehacked, sndinfo, decorate, texure-init, flat-init, sprite-init, other-text

typedef struct
{
	// progress bar
	patch_t *gfx_loader_bg;
	patch_t *gfx_loader_bar;
	uint32_t gfx_step;
	uint32_t gfx_ace;
	uint32_t gfx_max;
	uint32_t gfx_current;
	uint16_t gfx_width;
	// counters
	uint32_t counter_value;
	uint32_t count_texture;
	uint32_t count_sprite;
} gfx_loading_t;

// SDK variables

//

uint8_t *ldr_alloc_message;

static uint8_t *ace_wad_name;
static uint32_t ace_wad_type;

uint8_t *screen_buffer;
uint32_t old_game_mode;

uint8_t *error_module;

static gfx_loading_t *loading;

static uint_fast8_t dev_mode;

static uint32_t old_zone_size;

//
static const hook_t restore_loader[];

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void *load_palette()
{
	int32_t idx;
	void *dest = screen_buffer;

	idx = wad_get_lump("PLAYPAL");
	wad_read_lump(dest, idx, 768);

	render_preinit(dest);

	return dest;
}

//
// error message (in game)

void error_message(uint8_t *text)
{
	map_start_title();
	M_StartMessage(text, NULL, 0);
}

//
// loading

void *ldr_malloc(uint32_t size)
{
	void *ret;

	ret = doom_malloc(size);
	if(!ret)
		engine_error("LOADER", "%s memory allocation failed! (%uB)", ldr_alloc_message, size);

	return ret;
}

void *ldr_realloc(void *ptr, uint32_t size)
{
	void *ret;

	ret = doom_realloc(ptr, size);
	if(!ret)
		engine_error("LOADER", "%s memory allocation failed! (%uB)", ldr_alloc_message, size);

	return ret;
}

void ldr_dump_buffer(const uint8_t *path, void *buff, uint32_t size)
{
	int32_t fd;

	fd = doom_open_WR(path);
	if(fd < 0)
		return;

	doom_write(fd, buff, size);
	doom_close(fd);
}

//
// lump counting

static void cb_counter(lumpinfo_t *li)
{
	loading->counter_value++;
}

//
// graphics mode

void gfx_progress(int32_t step)
{
	uint32_t width;

	if(!step)
	{
		V_DrawPatchDirect(0, 0, loading->gfx_loader_bg);
		I_FinishUpdate();
		if(!dev_mode)
			_hack_update();
		// reset
		loading->gfx_current = 0;
		return;
	}

	if(!loading->gfx_loader_bar)
		return;

	if(step < 0)
		step = loading->gfx_ace;

	loading->gfx_current += step;

	if(loading->gfx_current >= loading->gfx_max)
		width = loading->gfx_width;
	else
		width = (loading->gfx_step * loading->gfx_current) >> 16;

	if(loading->gfx_loader_bar->width == width)
		return;

	loading->gfx_loader_bar->width = width;

	V_DrawPatchDirect(0, 0, loading->gfx_loader_bar);

	I_FinishUpdate();
	if(!dev_mode)
		_hack_update();
}

static void init_gfx()
{
	int32_t idx;

	// these locations are specifically picked for their size
	// background is required only once
	loading->gfx_loader_bg = (patch_t*)(screen_buffer + 64000 + 69632);
	loading->gfx_loader_bar = (patch_t*)(screen_buffer + 64000);

	// load background
	idx = wad_check_lump("A_LDING");
	if(idx < 0)
		idx = wad_get_lump("INTERPIC");
	wad_read_lump(loading->gfx_loader_bg, idx, 69632);

	// load progress bar
	idx = wad_check_lump("A_LDBAR");
	if(idx < 0)
		idx = wad_get_lump("TITLEPIC");
	wad_read_lump(loading->gfx_loader_bar, idx, 69632);

	loading->gfx_width = loading->gfx_loader_bar->width;
	if(loading->gfx_width < 32 || loading->gfx_width > 320)
		loading->gfx_loader_bar = NULL;
}

//
// MAIN

uint32_t ace_main()
{
	// DEBUG
#ifdef DUMP_VRAM
	{
		int32_t fd;
		fd = doom_open_WR("vram.bin");
		if(fd > 0)
		{
			doom_write(fd, (void*)0xA0000, 128*1024);
			doom_close(fd);
		}
	}
#endif

	// title
	doom_printf("-= ACE Engine by kgsws =-\nCODE: 0x%08X DATA: 0x%08X ACE: 0x%08X+0x1004\n", doom_code_segment, doom_data_segment, ace_segment);

	// install hooks
	utils_init();

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

	// new video code
	init_draw();

	// load WAD files
	wad_init();

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

	// load graphics
	init_gfx();

	// init graphics mode
	I_InitGraphics();

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

	// disable 'printf'
	if(M_CheckParm("-dev"))
		dev_mode = 1;
	else
		*((uint8_t*)0x0003FE40 + doom_code_segment) = 0xC3;

	// start loading
	gfx_progress(0);

	//
	// early stuff

	// config
	init_config();

	// dehacked
	init_dehacked();
	gfx_progress(-1);

	//
	// count EVERYTHING

	// count textures
	loading->count_texture += count_textures();
	loading->counter_value = 0;
	wad_handle_range(0x5854, cb_counter);
	loading->count_texture += loading->counter_value;

	// count sprites
	loading->counter_value = 0;
	wad_handle_range('S', cb_counter);
	loading->count_sprite = loading->counter_value;

	// calculate progress bar
	{
		uint32_t new_max;

		// GFX stuff count
		loading->gfx_max = loading->count_texture + loading->count_sprite;

		// render tables; it is slooow on old PCs
		if(render_tables_lump < 0)
			loading->gfx_max += RENDER_TABLE_PROGRESS;

		// reserve 15% for engine stuff
		new_max = (loading->gfx_max * 115) / 100;
		loading->gfx_ace = (new_max - loading->gfx_max) / LDR_ENGINE_COUNT;

		// fix rounding error
		loading->gfx_max = loading->gfx_ace * LDR_ENGINE_COUNT + loading->gfx_max;

		// progress bar step size
		loading->gfx_step = ((uint32_t)loading->gfx_width << 16) / loading->gfx_max;
	}

	//
	// LOADING

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

	// random
	init_rng();

	// render
	init_render();

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

	// sound
	init_sound();
	gfx_progress(-1);

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

	// decorate
	init_decorate();
	gfx_progress(-1);

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

	// textures
	init_textures(loading->count_texture);
	gfx_progress(-1);

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

	// flats
	init_flats();
	gfx_progress(-1);

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

	// sprites
	init_sprites(loading->count_sprite);
	gfx_progress(-1);

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

	// animations
	init_animations();

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

	// menu
	init_menu();

	// map
	init_map();

#ifdef RAM_DEBUG
	_hack_show_ram();
	doom_printf("\n");
#endif

	//
	gfx_progress(-1);

	// disable shareware
	old_game_mode = gamemode;
	gamemode = 1;
	gamemode_sw = 0;
	gamemode_reg = 0;

	// continue running Doom
}

//
// bluescreen

static void bs_puts(uint32_t x, uint32_t y, uint8_t *text)
{
	// 'ace_wad_type' is reused as text attributes
	uint16_t *dst;
	uint32_t xx = x;

	dst = (uint16_t*)0xB8000 + x + y * 80;

	while(*text)
	{
		uint8_t in = *text++;
		if(in == '\n')
			goto next_line;
		else
			*dst++ = ace_wad_type | in;
		xx++;
		if(xx >= 74)
		{
next_line:
			dst += (80 - xx) + x;
			xx = x;
		}
	}
}

__attribute((noreturn))
void bluescreen()
{
	uint8_t *text = (void*)d_drawsegs;

	if(!error_module)
		error_module = "idtech1.";

	doom_printf("[%s] %s\n", error_module, text);

	for(uint16_t *dst = (uint16_t*)0xB8000; dst < (uint16_t*)0xB8FA0; dst++)
		*dst = 0x1720;

	ace_wad_type = 0x7100;
	bs_puts(34, 8, " ACE Engine ");
	ace_wad_type = 0x1F00;
	bs_puts(12, 10, "An error has occurred. There is not much you can do now.");
	bs_puts(12, 12, "Module that caused this error is");
	bs_puts(23, 20, "Type 'cls' to clear the screen ...");
	bs_puts(45, 12, error_module);
	bs_puts(6, 15, text);

#if 0
	for(memblock_t *block = mainzone->blocklist.next; ; block = block->next)
	{
		doom_printf("BLOCK 0x%08X; size %uB tag %u used %u\n", block, block->size, block->tag, !!block->user);

		if(block->next == &mainzone->blocklist)
			break;
	}
#endif

	dos_exit(1);
}

//
// DEBUG

#ifdef DEBUG_CRASH
static void debug_func()
{
	uint32_t *ptr;

	ptr = (uint32_t*)&ptr;

	for(uint32_t i = 0; i < 64; i++)
		doom_printf("[%02u] 0x%08X\n", i, ptr[i]);

	dos_exit(1);
}
#endif

//
// zone

static __attribute((regparm(2),no_caller_saved_registers))
void *zone_alloc(uint32_t *psz)
{
	uint32_t size;
	void *ptr = NULL;

	for(size = *psz; size > 0x10000; size -= 0x10000)
	{
		ptr = doom_malloc(size);
		if(ptr)
			break;
	}

	if(ptr)
		*psz = size;
	else
		*psz = 0;

	return ptr;
}

static __attribute((regparm(2),no_caller_saved_registers))
void Z_Init()
{
	// Due to previous zone allocation it does not seem to be
	// possible to get large enough block to cover entire RAM.
	memblock_t *block;
	memblock_t *blother = NULL;
	void *reserved;
	uint32_t total, check;

	// make sure there's a bit of heap left
	reserved = doom_malloc(0x8000);
	if(!reserved)
		engine_error("ZONE", "Allocation failed!");

	// pick allocation method
	if(old_zone_size < 0x00800000)
	{
		// original zone was less than 8MiB
		// it should not be possible to get more memory
		uint32_t size = old_zone_size;

		mainzone = zone_alloc(&size);
		if(!mainzone)
			engine_error("ZONE", "Allocation failed!");

		doom_printf("[ZONE] 0x%08X at 0x%08X\n", size, mainzone);

		mainzone->size = size;

		total = size;
	} else
	{
		// allocate zone in two blocks
		uint32_t sz0, sz1;
		void *p0, *p1;

		sz0 = old_zone_size;
		sz1 = dpmi_get_ram();

		doom_printf("[ZONE] Available 0x%08X bytes.\n", sz1);

		// allocate larger block first
		if(sz1 > sz0)
		{
			uint32_t tmp = sz1;
			sz1 = sz0;
			sz0 = tmp;
		}

		// first
		p0 = zone_alloc(&sz0);
		if(!p0)
			engine_error("ZONE", "Allocation failed!");

		// second
		p1 = zone_alloc(&sz1);

		// info
		doom_printf("[ZONE] 0x%08X at 0x%08X\n", sz0, p0);
		doom_printf("[ZONE] 0x%08X at 0x%08X\n", sz1, p1);

		total = sz0 + sz1;

		// prepare zone
		if(p1 > p0 || !p1)
		{
			mainzone = p0;
			mainzone->size = sz0;
			if(p1)
			{
				blother = p1;
				blother->size = sz1;
			}
		} else
		{
			mainzone = p1;
			mainzone->size = sz1;
			blother = p0;
			blother->size = sz0;
		}
	}

	check = (uint32_t)mod_config.mem_min * 1024 * 1024;
	if(!check)
		check = 512 * 1024 * 1024;
	if(total < check && !M_CheckParm("-lowzone"))
	{
		uint32_t dec;
		uint8_t s0, s1;

		if(total < 1024 * 1024)
		{
			s1 = 'k';
			total <<= 10;
		} else
			s1 = 'M';

		dec = (total & 0xFFFFF) / 10486;
		total >>= 20;

		if(mod_config.mem_min)
		{
			s0 = 'M';
			check = mod_config.mem_min;
		} else
		{
			s0 = 'k';
			check = 512;
		}

		engine_error("ZONE", "Not enough memory!\nMinimum is %u %ciB but only %u.%02u %ciB is available.\nYou can skip this check using parameter '%s'.", check, s0, total, dec, s1, "-lowzone");
	}

	block = (memblock_t*)((void*)mainzone + sizeof(memzone_t));

	mainzone->blocklist.next = block;
	mainzone->blocklist.prev = block;

	mainzone->blocklist.user = (void*)mainzone;
	mainzone->blocklist.tag = PU_STATIC;
	mainzone->rover = block;

	block->prev = &mainzone->blocklist;
	block->next = &mainzone->blocklist;
	block->user = NULL;
	block->size = mainzone->size - sizeof(memzone_t);

	if(blother)
	{
		// connect split zone
		memblock_t *blk;
		uint32_t gap;

		gap = (void*)blother - (void*)block;
		gap -= block->size;

		doom_printf("[ZONE] Skip 0x%08X\n", gap);

		block->size -= sizeof(memblock_t);
		blk = (void*)block + block->size;

		blk->size = gap + sizeof(memblock_t);
		blk->user = (void*)mainzone;
		blk->tag = PU_STATIC;
		blk->id = ZONEID;
		blk->next = blother;
		blk->prev = block;

		blother->user = NULL;
		blother->tag = 0;
		blother->id = 0;
		blother->prev = blk;
		blother->next = block->next;

		block->next = blk;
	}

	// release reserved bit
	doom_free(reserved);
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void ldr_restore()
{
	// restore 'I_Error' modification
	utils_install_hooks(restore_loader, 1);
	// call the original
	I_StartupSound();
}

static __attribute((regparm(2),no_caller_saved_registers))
void late_init()
{
	// call original first
	ST_Init();

	// call other init
	stbar_init();
}

static __attribute((regparm(2),no_caller_saved_registers))
void data_init()
{
	tex_init_data();
	flat_init_data();
	spr_init_data();
}

static const hook_t restore_loader[] =
{
	// 'I_Error' patch
	{0x0001B830, CODE_HOOK | HOOK_UINT8, 0xE8},
};

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// save old zone size - passed by loader in 'drawsegs'
	{0x0002D0A0, DATA_HOOK | HOOK_READ32, (uint32_t)&old_zone_size},
	// data init stuff; 'R_InitData' was overwritten by the exploit
	{0x00035D83, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)data_init},
	// late init stuff
	{0x0001E950, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)late_init},
	// replace call to 'Z_Init' in 'D_DoomMain'
	{0x0001E4BE, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)Z_Init},
	// modify 'I_ZoneBase' to only report available memory
	{0x0001AC7C, CODE_HOOK | HOOK_UINT16, 0xD089},
	{0x0001AC7E, CODE_HOOK | HOOK_JMP_DOOM, 0x0001AD36},
	// restore stuff, hook 'I_StartupSound'
	{0x0001AA7A, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)ldr_restore},
	// add custom loading, skip "commercial" text and PWAD warning
	{0x0001E4DA, CODE_HOOK | HOOK_JMP_DOOM, 0x0001E70C},
	// change '-config' to '-cfg'
	{0x00022B0A, DATA_HOOK | HOOK_UINT32, 0x6766},
	// disable title text update
	{0x0001D8D0, CODE_HOOK | HOOK_UINT8, 0xC3},
	// disable call to 'I_InitGraphics' in 'D_DoomLoop'
	{0x0001D56D, CODE_HOOK | HOOK_SET_NOPS, 5},
	// replace call to 'W_CacheLumpName' in 'I_InitGraphics'
	{0x0001A0F5, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)load_palette},
	// disable call to 'I_InitDiskFlash' in 'I_InitGraphics'
	{0x0001A0FF, CODE_HOOK | HOOK_SET_NOPS, 5},
	// disable disk flash; 'grmode = 1' in 'I_InitGraphics'
	{0x0001A041, CODE_HOOK | HOOK_SET_NOPS, 6},
	// place 'loading' structure into 'vissprites' + 1024
	{0x0005A610, DATA_HOOK | HOOK_IMPORT, (uint32_t)&loading},
	// early 'I_Error' fix
	{0x0001B830, CODE_HOOK | HOOK_UINT8, 0xC3},
	// bluescreen 'I_Error' update
	{0x0001AB32, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)hook_bluescreen},
	// read stuff
	{0x0002B6E0, DATA_HOOK | HOOK_READ32, (uint32_t)&ace_wad_name},
	{0x0002C150, DATA_HOOK | HOOK_READ32, (uint32_t)&ace_wad_type},
	{0x00074FC4, DATA_HOOK | HOOK_READ32, (uint32_t)&screen_buffer},
	// DEBUG
#ifdef RAM_DEBUG
	{0x0001AC8A, CODE_HOOK | HOOK_JMP_DOOM, 0x0001AD36},
#endif
#ifdef DEBUG_CRASH
	{0x00014650, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)debug_func},
//	{0x00014650, CODE_HOOK | HOOK_UINT16, 0x0B0F},
#endif
};

