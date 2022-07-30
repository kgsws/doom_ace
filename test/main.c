// kgsws' Doom ACE
#include <stdint.h>

// local variables
extern void *doom_data_segment;
extern void *doom_code_segment;
// local functions
void dos_exit(uint32_t);

// doom functions
static void (*doom_printf)(uint8_t*, ...);
void (*I_FinishUpdate)();

//
// EXAMPLE

void hook_I_FinishUpdate()
{
	// get current framebuffer
	uint8_t *destscreen = *((uint8_t**)(doom_data_segment + 0x0002914C));

	// add 'crosshair'
	destscreen[100 * 80 + 40] = 231;

	// run the original function
	I_FinishUpdate();
}

//
// MAIN

void ace_main()
{
	// locate 'printf'
	doom_printf = doom_code_segment + 0x0003FE40;

	// hello world
	doom_printf("Text from ACE!\n");
	doom_printf("CODE: 0x%08X\n", doom_code_segment);
	doom_printf("DATA: 0x%08X\n", doom_data_segment);

	// exit
//	dos_exit(1);

	// change '-config' to '-cfg'
	*((uint32_t*)(doom_data_segment + 0x00022B0A)) = 0x6766;

	// invert 'run key' logic (enable auto run)
	*((uint8_t*)(doom_code_segment + 0x0001FBC5)) = 0x01;

	// fix blazing door double close sound
	*((uint16_t*)(doom_code_segment + 0x0002690A)) = 0x0BEB; // jmp +11

	// locate 'I_FinishUpdate'
	I_FinishUpdate = doom_code_segment + 0x00019F60;

	// Redirect call to 'I_FinishUpdate' in 'D_Display' to custom function.
	// This replaces offset of 'call' opcode. The opcode is kept as-is.
	// Call offsets are relative to EIP so offset to 'hook_I_FinishUpdate' has to be calculated.
	*((uint32_t*)(doom_code_segment + 0x0001D4A7)) = (uint32_t)hook_I_FinishUpdate - (uint32_t)(doom_code_segment + 0x0001D4AB);

	// continue loading Doom
}

