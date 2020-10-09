// kgsws' Doom ACE
// Main code example.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "snake.h"

hook_t hook_list[] =
{
	// enable 'episode' menu
	{0x00022734, CODE_HOOK | HOOK_UINT8, 0xEB}, // 'jmp r8'
	// replace episode 2 function
	{0x000122cd, DATA_HOOK | HOOK_ABSADDR_ACE, (uint32_t)snake_start},
	// only two 'episodes'
	{0x000122e4, DATA_HOOK | HOOK_UINT16, 2},
	// replace 1st episode name
	{0x000122b2, DATA_HOOK | HOOK_CSTR_ACE, (uint32_t)"CWILV00"},
	// invert 'run key' function (auto run)
	{0x0001fbc5, CODE_HOOK | HOOK_UINT8, 2},
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

