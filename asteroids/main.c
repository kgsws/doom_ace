// kgsws' Doom ACE
// Main code example.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "intro.h"

static hook_t hook_list[] =
{
	// replace D_PageDrawer, kinda
	{0x0001d319, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)intro_drawer},
	// replace D_PageTicker
	{0x0002083f, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)intro_ticker},
	// replace M_Responder
	{0x0001d13a, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)intro_input},
	// disable 'wipe'
	{0x0001d4a4, CODE_HOOK | HOOK_UINT16, 0x9090}, // two 'nop's
	// invert 'run key' function (auto run)
	{0x0001fbc5, CODE_HOOK | HOOK_UINT8, 0x01},
	// import some variables from DATA segment
	{0x00074F94, DATA_HOOK | HOOK_READ32, (uint32_t)&lumpcache},
	{0x0002B3BC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gametic},
	{0x00005A84, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finesine},
	{0x00074FC4, DATA_HOOK | HOOK_READ32, (uint32_t)&screens0},
	{0x00012C64, DATA_HOOK | HOOK_IMPORT, (uint32_t)&am_height},
	{0x0002B7E4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&min_scale_mtof},
	{0x0002AE78, DATA_HOOK | HOOK_IMPORT, (uint32_t)&players},
	{0x00039540, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewx},
	{0x00039544, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewy},
	{0x000752C8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&shortnum},
	// terminator
	{0}
};

// usefull variables
void **lumpcache;
uint32_t *gametic;
uint8_t *screens0;
fixed_t *finesine;
fixed_t *finecosine;
uint32_t *am_height;
fixed_t *min_scale_mtof;
void **players;
fixed_t *viewx;
fixed_t *viewy;
void **shortnum;

// this is the exploit entry function
// patch everything you need and leave
void ace_init()
{
	// install hooks
	utils_install_hooks(hook_list);

	// update
	finecosine = finesine + (FINEANGLES / 4);

	// initialize
	intro_init();
}

