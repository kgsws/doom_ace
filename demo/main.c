// kgsws' Doom ACE
// Main code example.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "stbar.h"
#include "mlook.h"

static void custom_RenderPlayerView(player_t*) __attribute((regparm(1),no_caller_saved_registers));

static hook_t hook_list[] =
{
	// import useful variables from DATA segment
	{0x0002CF80, DATA_HOOK | HOOK_IMPORT, (uint32_t)&leveltime},
	{0x00005A84, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finesine},
	{0x0002B3BC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gametic},
	{0x0002B3DC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&consoleplayer},
	{0x0002B3D8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&displayplayer},
	//
	{0x00032304, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewheight},
	{0x0003230C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewwidth},
	{0x00038FF8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&detailshift},
	//
	{0x0002AE78, DATA_HOOK | HOOK_IMPORT, (uint32_t)&players},
	{0x00012D90, DATA_HOOK | HOOK_IMPORT, (uint32_t)&weaponinfo},
	// invert 'run key' function (auto run)
	{0x0001fbc5, CODE_HOOK | HOOK_UINT8, 0x01},
	// custom overlay stuff
	{0x0001d362, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)custom_RenderPlayerView},
	// terminator
	{0}
};

// usefull variables
uint32_t *gametic;
uint32_t *leveltime;
fixed_t *finesine;
fixed_t *finecosine;

uint32_t *consoleplayer;
uint32_t *displayplayer;

uint32_t *viewheight;
uint32_t *viewwidth;
uint32_t *detailshift;

// sometimes usefull variables
player_t *players;
weaponinfo_t *weaponinfo;

// this is the exploit entry function
// patch everything you need and leave
void ace_init()
{
	// install hooks
	utils_install_hooks(hook_list);

	// update
	finecosine = finesine + (FINEANGLES / 4);

	// init fullscreen status bar
	stbar_init();

	// init mouse look
	mlook_init();
}

// this function is called when 3D view should be drawn
// - only in level
// - not in automap
static __attribute((regparm(1),no_caller_saved_registers)) void custom_RenderPlayerView(player_t *pl)
{
	// actually render 3D view
	R_RenderPlayerView(pl);

	// status bar
	stbar_draw(pl);
}

