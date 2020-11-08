// kgsws' Doom ACE
// Sound table.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "textpars.h"

sfx_info_t *sfxinfo;

void sound_init()
{
}

uint32_t sound_get_id(uint8_t *name)
{
	// testing code for doom only sounds, for now

	if(*((uint16_t*)name) != 0x7364)
		return 0;

	for(uint32_t i = 1; i < NUMSFX; i++)
		if(tp_nc_compare(name + 2, sfxinfo[i].name))
			return i;

	return 0;
}

