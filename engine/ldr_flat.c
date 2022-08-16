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
static uint32_t **flattranslation;

static uint32_t tmp_count;

//
// callbacks

static void cb_setup(lumpinfo_t *li)
{
	if(li->size != 64 * 64)
		return;

	(*flattranslation)[tmp_count] = li - *lumpinfo;

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
	// this is not ideal as flattranslation could be already changed
	// but it's nothing critical
	return W_CacheLumpNum((*flattranslation)[idx], PU_CACHE);
}

static __attribute((regparm(2),no_caller_saved_registers))
int32_t flat_num_check(uint8_t *name)
{
	union
	{
		uint64_t w;
		uint8_t b[8];
	} nm;
	uint32_t idx = *numflats;
	lumpinfo_t *li = *lumpinfo + lumpcount;
	uint32_t is_inside = 0;

	// search as 64bit number
	nm.w = 0;
	for(uint32_t i = 0; i < 8; i++)
	{
		register uint8_t in = name[i];
		if(!in)
			break;
		if(in >= 'a' && in <= 'z')
			in &= ~0x20; // uppercase only
		nm.b[i] = in;
	}

	// do a backward search
	do
	{
		li--;

		if(is_inside)
		{
			if(li->wame == 0x0054524154535F46 || li->wame == 0x54524154535F4646)
				is_inside = 0;
			else
			if(li->size == 64 * 64)
			{
				idx--;
				if(li->wame == nm.w)
					return idx;
			}
		} else
		{
			if(li->wame == 0x000000444E455F46 || li->wame == 0x0000444E455F4646)
				is_inside = 1;
		}

	} while(li >= *lumpinfo && idx);

	return -1;
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

void init_flats()
{
	tmp_count = 0;
	wad_handle_range('F', cb_count);

	*flattranslation = doom_malloc(tmp_count * sizeof(uint32_t));
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
	// replace call to 'W_CheckNumForName' in 'P_InitPicAnims' (flat animation check)
	{0x0002EEE4, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)flat_num_check}, // TEMPORARY
	// replace call to 'R_FlatNumForName' in 'P_InitPicAnims' (flat animation basepic)
	{0x0002EEE4, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)flat_num_basepic}, // TEMPORARY
	// diable call to 'R_InitFlats' in 'R_InitData'
	{0x00034622, CODE_HOOK | HOOK_SET_NOPS, 5},
	// replace call to 'W_CacheLumpNum' in 'R_PrecacheLevel' (flat caching)
	{0x0003483C, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)precache_flat},
	// import variables
	{0x000300DC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numflats},
//	{0x000300FC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&firstflat}, // this is unused and kept 0
	{0x00030110, DATA_HOOK | HOOK_IMPORT, (uint32_t)&flattranslation},
};

