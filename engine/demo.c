// kgsws' ACE Engine
////
// Demo files ... TBD.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "map.h"
#include "demo.h"

uint32_t *demoplayback;
uint32_t *demorecording;

//
// DEMOs ARE DISABLE, for now

__attribute((regparm(2),no_caller_saved_registers))
static uint32_t do_play_demo()
{
	if(*gamestate != GS_DEMOSCREEN)
		map_start_title();
	*gameaction = ga_nothing;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x0002B3E4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&demoplayback},
	{0x0002B3F0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&demorecording},
	// replace 'G_DoPlayDemo'
	{0x00021AD0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)do_play_demo},
	// disable 'demorecording'
	{0x0002B3F0, DATA_HOOK | HOOK_UINT32, 0},
};

