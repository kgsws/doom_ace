// kgsws' Doom ACE
// Main entry.
// All the hooks.
// Graphical loader.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "stbar.h"

static void custom_RenderPlayerView(player_t*) __attribute((regparm(2),no_caller_saved_registers));

// all the hooks for ACE engine
static hook_t hook_list[] =
{
	// common
	{0x00074FA0, DATA_HOOK | HOOK_READ32, (uint32_t)&numlumps},
	{0x00074FA4, DATA_HOOK | HOOK_READ32, (uint32_t)&lumpinfo},
	{0x0002B698, DATA_HOOK | HOOK_IMPORT, (uint32_t)&screenblocks},
	{0x00012D90, DATA_HOOK | HOOK_IMPORT, (uint32_t)&weaponinfo},
	{0x0002914C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&destscreen},
	{0x00074FC4, DATA_HOOK | HOOK_READ32, (uint32_t)&screens0},
	// stbar.c
	{0x000752f0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallnum},
	{0x00075458, DATA_HOOK | HOOK_READ32, (uint32_t)&tallpercent},
	// disable "store demo" mode check
//	{0x0001d113, CODE_HOOK | HOOK_UINT16, 0x4CEB}, // 'jmp'
	// invert 'run key' function (auto run)
	{0x0001fbc5, CODE_HOOK | HOOK_UINT8, 0x01},
	// custom overlay stuff
	{0x0001d362, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)custom_RenderPlayerView},
	//
	// terminator
	{0}
};

//
// imported variables
uint32_t numlumps;
lumpinfo_t *lumpinfo;
uint32_t *screenblocks;
weaponinfo_t *weaponinfo;
uint8_t **destscreen;
uint8_t *screens0;

// this function is called when 3D view should be drawn
// - only in level
// - not in automap
static __attribute((regparm(2),no_caller_saved_registers))
void custom_RenderPlayerView(player_t *pl)
{
	// actually render 3D view
	R_RenderPlayerView(pl);

	// status bar
	stbar_draw(pl);
}

//
// section search
static int section_count_lumps(const char *start, const char *end)
{
	int lump = numlumps - 1;
	int inside = 0;
	int count = 0;

	// go backwards trough all the lumps
	while(lump >= 0)
	{
		if(lumpinfo[lump].wame == *((uint64_t*)start))
		{
			if(!inside)
				// unclosed section
				I_Error("[ACE] %s without %s", start, end);
			// close this section
			inside = 0;
		} else
		if(lumpinfo[lump].wame == *((uint64_t*)end))
		{
			if(inside)
				// unclosed section
				I_Error("[ACE] %s without %s", end, start);
			// open new section
			inside = 1;
			// do not count this one
			count--;
		}
		if(inside)
			count++;
		lump--;
	}

	if(inside)
		// unclosed section
		I_Error("[ACE] %s without %s", end, start);

	return count;
}

//
// graphical loader
static patch_t *init_lgfx()
{
	patch_t *p;
	uint32_t lmp, tmp;

	// fill all video buffers with background
	p = W_CacheLumpName("\xCC""LOADING", PU_STATIC);
	*destscreen = (void*)0xA0000;
	V_DrawPatchDirect(0, 0, 0, p);
	*destscreen = (void*)0xA4000;
	V_DrawPatchDirect(0, 0, 0, p);
	*destscreen = (void*)0xA8000;
	V_DrawPatchDirect(0, 0, 0, p);
	Z_Free(p);

	// redraw
	I_FinishUpdate();

	// load foreground
	// use screens[0] as a temporary storage
	lmp = W_GetNumForName("\xCC""LOADBAR");
	tmp = W_LumpLength(lmp);
	if(tmp > SCREENSIZE*2) // allow fullscreen LOADBAR
		I_Error("[ACE] invalid LOADBAR");
	W_ReadLump(lmp, screens0);
	p = (patch_t*)screens0;
	// this will cause error if patch is invalid
	V_DrawPatch(0, 0, 2, p);

	return p;
}

//
// this is the exploit entry function
void ace_init()
{
	patch_t *p;
	void *ptr;
	int32_t *patch_lump;
	uint32_t tmp;

	// install hooks
	utils_install_hooks(hook_list);

	// fullscreen status bar
	stbar_init();

	//
	// CUSTOM LOADER
	// - graphical
	// - load textures
	// - load sprites

	// setup graphics
	p = init_lgfx();

	// parse PNAMES
	// use screens[2] as a temporary storage
	{
		char *name;

		ptr = W_CacheLumpName("PNAMES", PU_STATIC);
		tmp = *((uint32_t*)ptr);
		if(tmp > SCREENSIZE / sizeof(int32_t)) // that is 16000 names
			I_Error("[ACE] invalid PNAMES");

		patch_lump = (int32_t*)(screens0 + SCREENSIZE*2);
		name = ptr + sizeof(uint32_t);
		for(uint32_t i = 0; i < tmp; i++, name += 8)
			patch_lump[i] = W_CheckNumForName(name);

		Z_Free(ptr);
	}

	// count all the textures in each TX_START-TX_END section
	tmp = section_count_lumps("TX_START", "TX_END\x00");
//	I_Error("total TX textures %d", tmp);

/*
	width = p->width;
	tmp = 0;
	while(1)
	{
		p->width = tmp++;
		V_DrawPatchDirect(0, 0, 0, p);
		I_FinishUpdate();
		if(tmp == width)
			break;
	}
*/
}

