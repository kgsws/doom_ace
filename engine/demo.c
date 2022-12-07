// kgsws' ACE Engine
////
// Demo files ... TBD.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "player.h"
#include "map.h"
#include "filebuf.h"
#include "demo.h"

//
// new demo handling
/*
__attribute((regparm(2),no_caller_saved_registers))
static void demo_close_read()
{
}

__attribute((regparm(2),no_caller_saved_registers))
static void demo_read_ticcmd(ticcmd_t *cmd)
{
}
*/
__attribute((regparm(2),no_caller_saved_registers))
static void do_play_demo()
{
	singledemo = 0;
	if(gamestate != GS_DEMOSCREEN)
	{
		gamestate = GS_DEMOSCREEN;
		map_start_title();
	}
	gameaction = ga_nothing;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace 'G_DoPlayDemo'
	{0x00021AD0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)do_play_demo},
	// replace call to 'G_ReadDemoTiccmd' in 'G_Ticker'
//	{0x00020607, CODE_HOOK | HOOK_UINT16, 0xD889},
//	{0x00020609, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)demo_read_ticcmd},
//	{0x0002060E, CODE_HOOK | HOOK_UINT16, 0x43EB},
	// replace call to 'Z_ChangeTag' in 'G_CheckDemoStatus'
//	{0x00021C8A, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)demo_close_read},
//	{0x00021C8F, CODE_HOOK | HOOK_UINT32, 0x28EBC931},
	// disable 'demorecording'
	{0x0002B3F0, DATA_HOOK | HOOK_UINT32, 0},
};

