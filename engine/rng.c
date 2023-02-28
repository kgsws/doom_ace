// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "rng.h"

uint32_t rng_max = 256;

static const hook_t hook_rng[];

//

void init_rng()
{
	int32_t lump;

	lump = wad_check_lump("ACE_RNG");
	if(lump < 0)
		return;

	if(!lumpinfo[lump].size)
		return;

	rng_max = lumpinfo[lump].size;
	if(rng_max > sizeof(rng_table))
		rng_max = sizeof(rng_table);

	wad_read_lump(rng_table, lump, rng_max);

	utils_install_hooks(hook_rng, 1);
}

static const hook_t hook_rng[] =
{
	{0x00024160, CODE_HOOK | HOOK_COPY(32), (uint32_t)rng_asm_code}, // 32 is max
};

