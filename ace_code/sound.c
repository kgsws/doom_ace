// kgsws' Doom ACE
// Sound table.
#include "engine.h"
#include "utils.h"
#include "defs.h"

typedef struct
{
	uint8_t *name;
	uint8_t lump_name[8];
} sndinfo_t;

void sound_init()
{
}

uint32_t sound_get_id(uint8_t *name)
{
	doom_printf("get sound id '%s'\n", name);
	return 0;
}

