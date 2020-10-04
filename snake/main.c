// kgsws' Doom ACE
// Main code example.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "snake.h"

hook_t hook_list[] =
{
	// replace menu drawing
	{0x0001d499, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)snake_drawer},
	// replace menu input
	{0x0001d13a, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)snake_input},
	// disable 'wipe' when playing SNAKE!
	{0x0001d4a4, CODE_HOOK | HOOK_UINT16, 0x9090}, // two 'nop's
	// invert 'run key' function (auto run)
	{0x0001fbc5, CODE_HOOK | HOOK_UINT8, 0x01},
	// import some variables from DATA segment
	{0x0002B3BC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gametic},
	{0x00005A84, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finesine},
	// terminator
	{0}
};

// usefull variables
uint32_t *gametic;
fixed_t *finesine;
fixed_t *finecosine;

// this is the exploit entry function
// patch everything you need and leave
void ace_init()
{
	// install hooks
	utils_install_hooks(hook_list);

	// update
	finecosine = finesine + (FINEANGLES / 4);

	// initialize SNAKE!
	snake_init();
}

