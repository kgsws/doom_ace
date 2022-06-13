// kgsws' Doom ACE
#include "engine.h"
#include "utils.h"

// variables
int32_t *myargc;
void ***myargv;
uint8_t **wadfiles;

static const hook_t htest[] =
{
	// invert 'run key' function (auto run)
	{0X0001FBC5, CODE_HOOK | HOOK_UINT8, 0x01},
	// skip part of loading
	{0x0001E4DA, CODE_HOOK | HOOK_JMP_DOOM, 0x0001E70C},
	// some variables
	{0x0002B6F0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&myargc},
	{0x0002B6F4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&myargv},
	{0x00029730, DATA_HOOK | HOOK_IMPORT, (uint32_t)&wadfiles}
};


const uint32_t test_ro[] =
{
	(uint32_t)"text pokus",
	(uint32_t)"pokus text",
};

uint32_t test_rw[] =
{
	(uint32_t)"pokus text",
	(uint32_t)"text pokus",
};

//
// MAIN

uint32_t ace_main()
{
	// LOGO
	// generated at 'https://patorjk.com/software/taag/#p=display&f=Slant%20Relief&t=ACE'
	doom_printf(
		"_____/\\\\\\\\\\\\\\\\\\___________/\\\\\\\\\\\\\\\\\\__/\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\_\n"
		" ___/\\\\\\\\\\\\\\\\\\\\\\\\\\______/\\\\\\////////__\\/\\\\\\///////////__\n"
		"  __/\\\\\\/////////\\\\\\___/\\\\\\/___________\\/\\\\\\_____________\n"
		"   _\\/\\\\\\_______\\/\\\\\\__/\\\\\\_____________\\/\\\\\\\\\\\\\\\\\\\\\\_____\n"
		"    _\\/\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\_\\/\\\\\\_____________\\/\\\\\\///////______\n"
		"     _\\/\\\\\\/////////\\\\\\_\\//\\\\\\____________\\/\\\\\\_____________\n"
		"by    _\\/\\\\\\_______\\/\\\\\\__\\///\\\\\\__________\\/\\\\\\_____________\n"
		"kgsws  _\\/\\\\\\_______\\/\\\\\\____\\////\\\\\\\\\\\\\\\\\\_\\/\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\_\n"
		"        _\\///________\\///________\\/////////__\\///////////////__\n"
	);

	// install hooks
	utils_init();

	// fix arguments
	for(uint32_t i = 0; i < *myargc; i++)
	{
		uint32_t *tmp = myargv[0][i];
		if(*tmp == 0x00584148)
		{
			myargv[0] += i;
			*myargc -= i;
			break;
		}
	}

	// remove bogus WAD file
	for(uint32_t i = 0; wadfiles[i]; i++)
	{
		uint32_t *tmp = (uint32_t*)wadfiles[i];
		if(*tmp == 0x28423129)
		{
			wadfiles[i] = NULL;
			break;
		}
	}

	// TEST
	for(uint32_t i = 0; i < 2; i++)
		doom_printf("%u = %p %p\n", i, test_rw[i], test_ro[i]);
	for(uint32_t i = 0; i < 5; i++)
		doom_printf("%u = %p %p\n", i, htest[i].addr, htest[i].value);

	// continue loading Doom
}

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"))) =
{
	// invert 'run key' function (auto run)
	{0X0001FBC5, CODE_HOOK | HOOK_UINT8, 0x01},
	// skip part of loading
	{0x0001E4DA, CODE_HOOK | HOOK_JMP_DOOM, 0x0001E70C},
	// some variables
	{0x0002B6F0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&myargc},
	{0x0002B6F4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&myargv},
	{0x00029730, DATA_HOOK | HOOK_IMPORT, (uint32_t)&wadfiles}
};

