// kgsws' ACE Engine
////
// Demo files ... TBD.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "demo.h"

uint32_t *demoplayback;

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x0002B3E4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&demoplayback},
};

