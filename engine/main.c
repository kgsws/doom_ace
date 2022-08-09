// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "dehacked.h"

static uint8_t *ace_wad_name;
static uint32_t ace_wad_type;

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
	doom_printf("                          -= ACE Engine by kgsws =-\n[ACE] CODE: 0x%08X DATA: 0x%08X\n", doom_code_segment, doom_data_segment);

	// install hooks
	utils_init();

	// WAD file
	// - check if ACE is IWAD of PWAD
	if(ace_wad_type == 0x44415749)
	{
		// replace IWAD with ACE WAD
		wadfiles[0] = ace_wad_name;
	} else
	{
		// insert ACE WAD file to the list
		for(uint32_t i = MAXWADFILES-2; i > 0; i--)
			wadfiles[i+1] = wadfiles[i];
		wadfiles[1] = ace_wad_name;
	}

	// load WAD files
	wad_init();

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
	// read stuff
	{0x0002C150, DATA_HOOK | HOOK_READ32, (uint32_t)&ace_wad_type},
	// import info
	{0x00015A28, DATA_HOOK | HOOK_IMPORT, (uint32_t)&states},
	{0x0001C3EC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mobjinfo},
	{0x00012D90, DATA_HOOK | HOOK_IMPORT, (uint32_t)&weaponinfo},
};

