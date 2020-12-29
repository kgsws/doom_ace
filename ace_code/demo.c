// kgsws' Doom ACE
// Demo stuff.
#include "engine.h"
#include "utils.h"
#include "defs.h"

static hook_t demo_skip_clear[] =
{
	{0x000217e0, CODE_HOOK | HOOK_SET_NOPS, 6},
	// terminator
	{0}
};

static hook_t demo_skip_restore[] =
{
	{0x000217e0, CODE_HOOK | HOOK_UINT32, 0xb3e42d89},
	{0x000217e4, CODE_HOOK | HOOK_UINT16, 0x0002},
	// terminator
	{0}
};

__attribute((regparm(2),no_caller_saved_registers))
void demo_start()
{
	// TODO: custom demo format

	*demoplayback = 1;
	utils_install_hooks(demo_skip_clear);
	G_DoPlayDemo();
	utils_install_hooks(demo_skip_restore);
}

