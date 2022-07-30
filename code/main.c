// kgsws' ACE Engine
////
#include "sdk.h"
#include "utils.h"

// variables
uint8_t **destscreen;

//
// EXAMPLE

void hook_I_FinishUpdate()
{
	// get current framebuffer
	uint8_t *dst = *destscreen;

	// add 'crosshair'
	dst[100 * 80 + 40] = 231;

	// run the original function
	I_FinishUpdate();
}

//
// MAIN

uint32_t ace_main()
{
	// hello world
	doom_printf("Text from ACE!\n");
	doom_printf("CODE: 0x%08X\n", doom_code_segment);
	doom_printf("DATA: 0x%08X\n", doom_data_segment);

	// install hooks
	utils_init();

	// continue loading Doom
}

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"))) =
{
	// 
	{0x00022B0A, DATA_HOOK | HOOK_UINT32, 0x6766},
	// invert 'run key' logic (auto run)
	{0x0001FBC5, CODE_HOOK | HOOK_UINT8, 0x01},
	// fix blazing door double close sound
	{0x0002690A, CODE_HOOK | HOOK_UINT16, 0x0BEB},
	// hook 'I_FinishUpdate' in 'D_Display'
	{0x0001D4A6, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_I_FinishUpdate},
	// some variables
	{0x0002914C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&destscreen}
};

