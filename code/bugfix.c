// kgsws' ACE Engine
////
// This file contains various engine bug fixes and updates.
#include "sdk.h"
#include "engine.h"
#include "utils.h"

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// invert 'run key' logic (auto run)
	{0x0001FBC5, CODE_HOOK | HOOK_UINT8, 0x01},
	// allow invalid switch textures
	{0x0003025B, CODE_HOOK | HOOK_CALL_DOOM, 0x000346F0},
	{0x0003026A, CODE_HOOK | HOOK_CALL_DOOM, 0x000346F0},
	// fix 'OUCH' face; this intentionaly fixes only 'OUCH' caused by enemies
	{0x0003A079, CODE_HOOK | HOOK_UINT8, 0xD7},
	{0x0003A081, CODE_HOOK | HOOK_UINT8, 0xFF},
	{0x0003A085, CODE_HOOK | HOOK_JMP_DOOM, 0x0003A136},
	{0x0003A137, CODE_HOOK | HOOK_UINT8, 8},
	{0x0003A134, CODE_HOOK | HOOK_UINT8, 0xEB},
};

