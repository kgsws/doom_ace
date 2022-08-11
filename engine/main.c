// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "dehacked.h"

#define LDR_ENGINE_WIDTH	2
#define LDR_ENGINE_COUNT	3	// dehacked + decorate + other text

static uint8_t *ace_wad_name;
static uint32_t ace_wad_type;

mobjinfo_t *mobjinfo;
state_t *states;
weaponinfo_t *weaponinfo;

static uint8_t *screen_buffer;

static patch_t *gfx_loader_bg;
static patch_t *gfx_loader_bar;
static uint32_t gfx_step;
static uint32_t gfx_max = 0xFFFFFFFF;
static uint16_t gfx_width;

static uint32_t counter_value;
static uint32_t count_texture;
static uint32_t count_sprite;
static uint32_t count_flat;

static num64_t range_defs[4] =
{
	{.u64 = 0x0054524154535f00},
	{.u64 = 0x54524154535f0000},
	{.u64 = 0x000000444e455f00},
	{.u64 = 0x0000444e455f0000}
};

//
// ACE Initialization

static __attribute((regparm(2),no_caller_saved_registers))
void ace_init()
{
	// initialize ACE Engine stuff
	deh_init();
}

static __attribute((regparm(2),no_caller_saved_registers))
void *load_palette()
{
	int32_t idx;
	void *dest = (void*)0x0002D0A0 + doom_data_segment; // drawsegs

	idx = wad_get_lump("PLAYPAL");
	wad_read_lump(dest, idx, 768);

	return dest;
}

//
// loading

static void handle_range(uint16_t ident, void (*cb)(lumpinfo_t*))
{
	lumpinfo_t *li = *lumpinfo;
	lumpinfo_t *le = *lumpinfo + lumpcount;
	uint32_t is_inside = 0;

	if(ident > 255)
	{
		range_defs[0].u64 = (0x0054524154535f00 << 8) | ident;
		range_defs[1].u64 = 0xFFFFFFFFFFFFFFFF;
		range_defs[2].u64 = (0x0054524154535f00 << 8) | ident;
		range_defs[3].u64 = 0xFFFFFFFFFFFFFFFF;
	} else
	{
		range_defs[0].u64 = 0x0054524154535f00 | ident;
		range_defs[1].u64 = 0x54524154535f0000 | ident;
		range_defs[2].u64 = 0x0054524154535f00 | ident;
		range_defs[3].u64 = 0x54524154535f0000 | ident;
	}

	for( ; li < le; li++)
	{
		if(is_inside)
		{
			if(li->wame == range_defs[2].u64 || li->wame == range_defs[3].u64)
				is_inside = 0;
			else
				cb(li);
		} else
		{
			if(li->wame == range_defs[0].u64 || li->wame == range_defs[1].u64)
				is_inside = 1;
		}
	}
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

	count_texture = count;
}

//
// lump counting

static void cb_counter(lumpinfo_t *li)
{
	counter_value++;
}

//
// graphics mode

static void gfx_progress(uint32_t step)
{
	uint32_t width;

	if(!gfx_loader_bar)
		return;

	if(step >= gfx_max)
		width = gfx_width;
	else
		width = (gfx_step * step) >> 16;

	if(gfx_loader_bar->width == width)
		return;

	gfx_loader_bar->width = width;

	V_DrawPatchDirect(0, 0, 0, gfx_loader_bg);
	V_DrawPatchDirect(0, 0, 0, gfx_loader_bar);

	I_FinishUpdate();
}

static void gfx_init()
{
	int32_t idx;

	gfx_loader_bg = (patch_t*)(screen_buffer + 64000);
	gfx_loader_bar = (patch_t*)(screen_buffer + 64000 + 69632);

	// load background
	idx = wad_check_lump("A_LDING");
	if(idx < 0)
		idx = wad_get_lump("INTERPIC");
	wad_read_lump(gfx_loader_bg, idx, 69632);

	// load progress bar
	idx = wad_check_lump("A_LDBAR");
	if(idx < 0)
		idx = wad_get_lump("TITLEPIC");
	wad_read_lump(gfx_loader_bar, idx, 69632);

	gfx_width = gfx_loader_bar->width;
	if(gfx_width < LDR_ENGINE_WIDTH * LDR_ENGINE_COUNT + LDR_ENGINE_WIDTH)
	{
		gfx_loader_bar = NULL;
		// well, just an empty loading screen
		V_DrawPatchDirect(0, 0, 0, gfx_loader_bg);
		I_FinishUpdate();
	} else
		gfx_width -= LDR_ENGINE_WIDTH * LDR_ENGINE_COUNT;
}

//
// loading stuff

static __attribute((regparm(2),no_caller_saved_registers))
void generate_texture(uint32_t idx)
{
	R_GenerateLookup(idx);
	gfx_progress(idx + 1);
}

//
// MAIN

uint32_t ace_main()
{
	doom_printf("                          -= ACE Engine by kgsws =-\n[ACE] CODE: 0x%08X DATA: 0x%08X\n", doom_code_segment, doom_data_segment);

	// install hooks
	utils_init();

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

	// count textures
	count_textures();
	counter_value = 0;
	handle_range(0x5854, cb_counter);
	count_texture += counter_value;
	doom_printf("[ACE] texture count %u\n", count_texture);
/*
	// count sprites
	counter_value = 0;
	handle_range('S', cb_counter);
	count_sprite = counter_value;
	doom_printf("[ACE] sprite count %u\n", counter_value);
*/
	// calculate progress bar
	gfx_max = count_texture + count_sprite;
	gfx_step = ((uint32_t)gfx_width << 16) / gfx_max;

	// continue loading Doom
}

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// add custom loading, skip "commercial" text and PWAD warning
	{0x0001E4DA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)ace_init},
	{0x0001E4DF, CODE_HOOK | HOOK_JMP_DOOM, 0x0001E70C},
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
	// replace call to 'R_GenerateLookup' in 'R_InitTextures'
	{0x00034418, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)generate_texture},
	// import variables
	{0x0002B6E0, DATA_HOOK | HOOK_READ32, (uint32_t)&ace_wad_name},
	{0x00029730, DATA_HOOK | HOOK_IMPORT, (uint32_t)&wadfiles},
	// read stuff
	{0x0002C150, DATA_HOOK | HOOK_READ32, (uint32_t)&ace_wad_type},
	{0x00074FC4, DATA_HOOK | HOOK_READ32, (uint32_t)&screen_buffer},
	// import info
	{0x00015A28, DATA_HOOK | HOOK_IMPORT, (uint32_t)&states},
	{0x0001C3EC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mobjinfo},
	{0x00012D90, DATA_HOOK | HOOK_IMPORT, (uint32_t)&weaponinfo},
};

