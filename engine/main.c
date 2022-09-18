// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "dehacked.h"
#include "decorate.h"
#include "animate.h"
#include "sound.h"
#include "map.h"
#include "menu.h"
#include "ldr_texture.h"
#include "ldr_flat.h"
#include "ldr_sprite.h"

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

uint32_t *wipegamestate;
uint32_t *gametic;
uint32_t *gamestate;
uint32_t *gameaction;
uint32_t *paused;
uint32_t *menuactive;
uint32_t *gamemode;
static uint32_t *gamemode_sw;
static uint32_t *gamemode_reg;

fixed_t *finesine;
fixed_t *finecosine;
uint32_t *viewheight;
uint32_t *viewwidth;

//

uint8_t *ldr_alloc_message;

static uint8_t *ace_wad_name;
static uint32_t ace_wad_type;

uint8_t *screen_buffer;

static gfx_loading_t *loading;

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

	return dest;
}

static __attribute((regparm(2),no_caller_saved_registers))
void finish_loading()
{
	// restore 'I_FinishUpdate' modification
	utils_install_hooks(restore_loader + 1, 1);
	doom_printf("LOADING FINISHED\n");
}

//
// error message (in game)

void error_message(uint8_t *text)
{
	D_StartTitle();
	M_StartMessage(text, NULL, 0);
}

//
// loading

void *ldr_malloc(uint32_t size)
{
	void *ret;

	ret = doom_malloc(size);
	if(!ret)
		I_Error("%s (%uB)", ldr_alloc_message, size);

	return ret;
}

void *ldr_realloc(void *ptr, uint32_t size)
{
	void *ret;

	ret = doom_realloc(ptr, size);
	if(!ret)
		I_Error("%s (%uB)", ldr_alloc_message, size);

	return ret;
}

static void count_textures()
{
	uint32_t temp;
	int32_t idx;
	uint32_t count = 0;

	idx = wad_check_lump("TEXTURE1");
	if(idx >= 0)
	{
		wad_read_lump(&temp, idx, 4);
		count += temp;
	}

	idx = wad_check_lump("TEXTURE2");
	if(idx >= 0)
	{
		wad_read_lump(&temp, idx, 4);
		count += temp;
	}

	loading->count_texture = count;
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
		V_DrawPatchDirect(0, 0, 0, loading->gfx_loader_bg);
		I_FinishUpdate();
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

	V_DrawPatchDirect(0, 0, 0, loading->gfx_loader_bar);

	I_FinishUpdate();
}

static void gfx_init()
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
	// title
	doom_printf("                          -= ACE Engine by kgsws =-\n[ACE] CODE: 0x%08X DATA: 0x%08X ACE: 0x%08X+0x1004\n", doom_code_segment, doom_data_segment, ace_segment);

	// install hooks
	utils_init();

	// disable shareware
	if(*gamemode_sw)
	{
		*gamemode_sw = 0;
		*gamemode_reg = 1;
	}

	// WAD file
	// - check if ACE is IWAD of PWAD
	if(ace_wad_type == 0x44415749)
	{
		// replace IWAD with ACE WAD
		wadfiles[0] = ace_wad_name;
	} else
	{
		// insert ACE WAD file to the list
		for(uint32_t i = MAXWADFILES-2; i > 0; i--)
			wadfiles[i+1] = wadfiles[i];
		wadfiles[1] = ace_wad_name;
	}

	// load WAD files
	wad_init();

	// load graphics
	gfx_init();

	// init graphics mode
	I_InitGraphics();

	// disable 'printf'
	if(!M_CheckParm("-dev"))
		*((uint8_t*)0x0003FE40 + doom_code_segment) = 0xC3;

	// start loading
	gfx_progress(0);

	//
	// count EVERYTHING

	// count textures
	count_textures();
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

	finecosine = finesine + (FINEANGLES / 4);

	// dehacked
	deh_init();
	gfx_progress(-1);

	// sound
	init_sound();
	gfx_progress(-1);

	// decorate
	init_decorate();
	gfx_progress(-1);

	// textures
	init_textures(loading->count_texture);
	gfx_progress(-1);

	// flats
	init_flats();
	gfx_progress(-1);

	// sprites
	init_sprites(loading->count_sprite);
	gfx_progress(-1);

	// animations
	init_animations();

	// menu
	init_menu();

	//
	gfx_progress(-1);

	// restore 'I_Error' modification
	utils_install_hooks(restore_loader + 0, 1);

	// continue running Doom
}

//
// hooks

static const hook_t restore_loader[] =
{
	// 'I_Error' patch
	{0x0001AB1E, CODE_HOOK | HOOK_CALL_DOOM, 0x0001B830},
	// 'I_FinishUpdate' patch
	{0x00019FE7, CODE_HOOK | HOOK_UINT8, 0x40},
};

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// add custom loading, skip "commercial" text and PWAD warning
	{0x0001E4DA, CODE_HOOK | HOOK_JMP_DOOM, 0x0001E70C},
	// change '-config' to '-cfg'
	{0x00022B0A, DATA_HOOK | HOOK_UINT32, 0x6766},
	// disable title text update
	{0x0001D8D0, CODE_HOOK | HOOK_UINT8, 0xC3},
	// relace call to 'I_InitGraphics' in 'D_DoomLoop'
	{0x0001D56D, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)finish_loading},
	// replace call to 'W_CacheLumpName' in 'I_InitGraphics'
	{0x0001A0F5, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)load_palette},
	// disable call to 'I_InitDiskFlash' in 'I_InitGraphics'
	{0x0001A0FF, CODE_HOOK | HOOK_SET_NOPS, 5},
	// disable disk flash; 'grmode = 1' in 'I_InitGraphics'
	{0x0001A041, CODE_HOOK | HOOK_SET_NOPS, 6},
	// place 'loading' structure into 'vissprites'
	{0x0005A210, DATA_HOOK | HOOK_IMPORT, (uint32_t)&loading},
	// early 'I_Error' fix
	{0x0001AB1E, CODE_HOOK | HOOK_SET_NOPS, 5},
	// force single page in 'I_FinishUpdate'
	{0x00019FE7, CODE_HOOK | HOOK_UINT8, 0x00},
	// import variables
	{0x0002B6E0, DATA_HOOK | HOOK_READ32, (uint32_t)&ace_wad_name},
	{0x00029730, DATA_HOOK | HOOK_IMPORT, (uint32_t)&wadfiles},
	// SDK variables
	{0x00005A84, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finesine},
	{0x00032304, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewheight},
	{0x0003230C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewwidth},
	{0x0002B404, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gameaction},
	{0x0002B408, DATA_HOOK | HOOK_IMPORT, (uint32_t)&paused},
	{0x0002B6CC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&menuactive},
	{0x0002A3BC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gamemode},
	{0x0002A3AC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gamemode_sw},
	{0x0002A3B4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gamemode_reg},
	{0x0002B3F4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gamestate},
	{0x00011B40, DATA_HOOK | HOOK_IMPORT, (uint32_t)&wipegamestate},
	{0x0002B3BC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gametic},
	// read stuff
	{0x0002C150, DATA_HOOK | HOOK_READ32, (uint32_t)&ace_wad_type},
	{0x00074FC4, DATA_HOOK | HOOK_READ32, (uint32_t)&screen_buffer},
};

