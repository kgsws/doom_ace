// kgsws' ACE Engine
////
// This file contains various engine bug fixes and updates.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "ldr_texture.h"
#include "render.h"

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// disable 'store demo' feature
	{0x0001D113, CODE_HOOK | HOOK_UINT16, 0x4CEB},
	// fix blaze door double closing sound
	{0x0002690A, CODE_HOOK | HOOK_UINT16, 0x0BEB},
	// fix 'OUCH' face; this intentionaly fixes only 'OUCH' caused by enemies
	{0x0003A079, CODE_HOOK | HOOK_UINT8, 0xD7},
	{0x0003A081, CODE_HOOK | HOOK_UINT8, 0xFF},
	{0x0003A085, CODE_HOOK | HOOK_JMP_DOOM, 0x0003A136},
	{0x0003A137, CODE_HOOK | HOOK_UINT8, 8},
	{0x0003A134, CODE_HOOK | HOOK_UINT8, 0xEB},
};

