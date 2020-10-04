// kgsws' Doom ACE
// Main code example.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "stbar.h"

static void custom_RenderPlayerView(player_t*) __attribute((regparm(1)));

static hook_t hook_list[] =
{
	// import useful variables from DATA segment
	{0x0002B3BC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gametic},
	{0x0002CF80, DATA_HOOK | HOOK_IMPORT, (uint32_t)&leveltime},
	{0x00005A84, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finesine},
	// more imports
	{0x00012D90, DATA_HOOK | HOOK_IMPORT, (uint32_t)&weaponinfo},
	// invert 'run key' function (auto run)
	{0x0001fbc5, CODE_HOOK | HOOK_UINT8, 0x01},
	// custom overlay stuff
	{0x0001d362, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)custom_RenderPlayerView},
	// terminator
	{0}
};

// usefull variables
uint32_t *screenblocks;
uint32_t *gametic;
uint32_t *leveltime;
fixed_t *finesine;
fixed_t *finecosine;

// sometimes usefull variables
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
}

// this function is called when 3D view should be drawn
// - only in level
// - not in automap
static __attribute((regparm(1))) void custom_RenderPlayerView(player_t *pl)
{
	// actually render 3D view
	R_RenderPlayerView(pl);

	// status bar
	stbar_draw(pl);
}

