// kgsws' ACE Engine
////
// Flat loading replacement.
// Supports flats in multiple WAD files.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "ldr_flat.h"

//

static uint32_t *numflats;
uint32_t **flattranslation;
uint16_t *flatlump;

static uint32_t tmp_count;

//
// callbacks

static void cb_setup(lumpinfo_t *li)
{
	uint32_t lump;

	if(li->size != 64 * 64)
		return;

	lump = li - *lumpinfo;

	(*flattranslation)[tmp_count] = lump;
	flatlump[tmp_count] = lump;

	tmp_count++;
}

static void cb_count(lumpinfo_t *li)
{
	if(li->size != 64 * 64)
		return;
	tmp_count++;
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void *precache_flat(uint32_t idx)
{
	return W_CacheLumpNum(flatlump[idx], PU_CACHE);
}

static __attribute((regparm(2),no_caller_saved_registers))
int32_t flat_num_get(uint8_t *name)
{
	int32_t ret;

	ret = flat_num_check(name);
	if(ret < 0)
		return 0;

	return ret;
}

//
// API

__attribute((regparm(2),no_caller_saved_registers))
int32_t flat_num_check(uint8_t *name)
{
	uint64_t wame;
	int32_t idx = *numflats;
	lumpinfo_t *li = *lumpinfo + lumpcount;

	// search as 64bit number
	wame = wad_name64(name);

	// do a backward search
	do
	{
		lumpinfo_t *li;

		idx--;
		li = *lumpinfo + flatlump[idx];

		if(li->wame == wame)
			return idx;
	} while(idx);

	return -1;
}

void init_flats()
{
	tmp_count = 0;
	wad_handle_range('F', cb_count);

	*flattranslation = doom_malloc(tmp_count * sizeof(uint32_t));
	flatlump = doom_malloc(tmp_count * sizeof(uint16_t));
	*numflats = tmp_count;

	tmp_count = 0;
	wad_handle_range('F', cb_setup);
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace 'R_FlatNumForName'
	{0x00034690, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)flat_num_get},
	// disable call to 'R_InitFlats' in 'R_InitData'
	{0x00034622, CODE_HOOK | HOOK_SET_NOPS, 5},
	// replace call to 'W_CacheLumpNum' in 'R_PrecacheLevel' (flat caching)
	{0x0003483C, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)precache_flat},
	// import variables
	{0x000300DC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numflats},
//	{0x000300FC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&firstflat}, // this is unused and kept 0
	{0x00030110, DATA_HOOK | HOOK_IMPORT, (uint32_t)&flattranslation},
};

