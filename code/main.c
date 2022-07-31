// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "dehacked.h"

uint8_t **wadfiles;
uint8_t *ace_wad_name;

mobjinfo_t *mobjinfo;
state_t *states;
weaponinfo_t *weaponinfo;

//
// ACE Initialization

static __attribute((regparm(2),no_caller_saved_registers))
void ace_init()
{
	// initialize ACE Engine stuff
	deh_init();
}

//
// MAIN

uint32_t ace_main()
{
	// LOGO - temporary, until graphical loader is added
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

	// insert ACE WAD file to the list
	for(uint32_t i = MAXWADFILES-2; i > 0; i--)
		wadfiles[i+1] = wadfiles[i];
	wadfiles[1] = ace_wad_name;

	// continue loading Doom
}

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// add custom loading, skip "commercial" text and PWAD warning
	{0x0001E4DA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)ace_init},
	{0x0001E4DF, CODE_HOOK | HOOK_JMP_DOOM, 0x0001E70C},
	// change '-config' to '-cfg'
	{0x00022B0A, DATA_HOOK | HOOK_UINT32, 0x6766},
	// import variables
	{0x0002B6E0, DATA_HOOK | HOOK_READ32, (uint32_t)&ace_wad_name},
	{0x00029730, DATA_HOOK | HOOK_IMPORT, (uint32_t)&wadfiles},
	// import info
	{0x00015A28, DATA_HOOK | HOOK_IMPORT, (uint32_t)&states},
	{0x0001C3EC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mobjinfo},
	{0x00012D90, DATA_HOOK | HOOK_IMPORT, (uint32_t)&weaponinfo},

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

