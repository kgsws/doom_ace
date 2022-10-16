// kgsws' ACE Engine
////
// Flat loading replacement.
// Supports flats in multiple WAD files.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "ldr_texture.h"
#include "ldr_flat.h"

//

uint16_t *flatlump;

static uint32_t tmp_count;

uint8_t **flattexture_data;
uint16_t *flattexture_idx;
uint8_t *flattexture_mask; // transparent color
static uint32_t num_texture_flats;

//
// callbacks

static void cb_setup(lumpinfo_t *li)
{
	uint32_t lump;

	if(li->size != 64 * 64)
		return;

	lump = li - lumpinfo;

	flattranslation[tmp_count] = tmp_count;
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
	if(idx >= numflats)
		return NULL; // 'R_PrecacheLevel' does not use this anyway
	return W_CacheLumpNum(flatlump[idx], PU_CACHE);
}

__attribute((regparm(2),no_caller_saved_registers))
int32_t flat_num_get(const uint8_t *name)
{
	int32_t ret;

	ret = flat_num_check(name);
	if(ret < 0)
	{
		// now try textures
		uint64_t wame = wad_name64(name);
		for(uint32_t i = 0; i < num_texture_flats; i++)
		{
			texture_t *tex = textures[flattexture_idx[i]];
			if(tex->wame == wame)
				return numflats + i + 1;
		}
		return numflats;
	}

	return ret;
}

static __attribute((regparm(2),no_caller_saved_registers))
void R_InitFlats()
{
	// find transparent color in texture based flats; for transparent extra floors
	for(uint32_t i = 0; i < num_texture_flats; i++)
	{
		uint8_t colors[256];
		texture_t *tex = textures[flattexture_idx[i]];

		memset(colors, 0, 256);

		for(uint32_t j = 0; j < tex->patchcount; j++)
		{
			texpatch_t *tp = tex->patch + j;
			patch_t *patch;

			if(tp->patch < 0)
				continue;

			patch = W_CacheLumpNum(tp->patch, PU_CACHE);

			for(uint32_t x = 0; x < patch->width; x++)
			{
				column_t *column = (column_t*)((uint8_t*)patch + patch->offs[x]);
				while(column->topdelta != 0xFF)
				{
					uint32_t count = column->length;
					uint8_t *source = (uint8_t*)column + 3;
					while(count--)
					{
						colors[*source] = 1;
						source++;
					}
					column = (column_t*)((uint8_t*)column + column->length + 4);
				}
			}
		}

		for(uint32_t j = 0; j < 256; j++)
		{
			if(!colors[j])
			{
				flattexture_mask[i] = j;
				break;
			}
		}
	}
}

//
// API

__attribute((regparm(2),no_caller_saved_registers))
int32_t flat_num_check(const uint8_t *name)
{
	uint64_t wame;
	int32_t idx = numflats;

	// search as 64bit number
	wame = wad_name64(name);

	// do a backward search
	do
	{
		lumpinfo_t *li;

		idx--;
		li = lumpinfo + flatlump[idx];

		if(li->wame == wame)
			return idx;
	} while(idx);

	return -1;
}

__attribute((regparm(2),no_caller_saved_registers))
uint64_t flat_get_name(uint32_t idx)
{
	lumpinfo_t *li;

	if(idx == numflats)
		return 0xFFFFFFFFFFFFFFFF;

	if(idx > numflats)
	{
		texture_t *tex = textures[flattexture_idx[idx - numflats - 1]];
		return tex->wame;
	}

	li = lumpinfo + flatlump[idx];

	return li->wame;
}

void init_flats()
{
	ldr_alloc_message = "Flat";

	tmp_count = 0;
	wad_handle_range('F', cb_count);

	flattranslation = ldr_malloc(tmp_count * sizeof(uint16_t));
	flatlump = ldr_malloc(tmp_count * sizeof(uint16_t));
	numflats = tmp_count;

	tmp_count = 0;
	wad_handle_range('F', cb_setup);

	// scan for 64x64 textures; for transparent extra floors
	for(uint32_t i = 0; i < numtextures; i++)
	{
		texture_t *tex = textures[i];

		if(tex->width != 64)
			continue;

		if(tex->height != 64)
			continue;

		num_texture_flats++;
	}

	// prepare those textures
	flattexture_data = ldr_malloc(num_texture_flats * sizeof(uint8_t*));
	flattexture_idx = ldr_malloc(num_texture_flats * sizeof(uint16_t));
	flattexture_mask = ldr_malloc(num_texture_flats * sizeof(uint8_t));

	num_texture_flats = 0;
	for(uint32_t i = 0; i < numtextures; i++)
	{
		texture_t *tex = textures[i];

		if(tex->width != 64)
			continue;

		if(tex->height != 64)
			continue;

		flattexture_data[num_texture_flats] = NULL;
		flattexture_idx[num_texture_flats] = i;
		flattexture_mask[num_texture_flats] = 0;

		num_texture_flats++;
	}

	if(num_texture_flats)
		doom_printf("[ACE] %u flat textures\n", num_texture_flats);
}

void *flat_generate_composite(uint32_t idx)
{
	if(!flattexture_data[idx])
	{
		uint8_t *data;
		texture_t *tex = textures[flattexture_idx[idx]];

		data = Z_Malloc(64 * 64, PU_STATIC, &flattexture_data[idx]);
		memset(data, flattexture_mask[idx], 64 * 64);

		for(uint32_t i = 0; i < tex->patchcount; i++)
		{
			texpatch_t *tp = tex->patch + i;
			patch_t *patch;
			int32_t xx, x0, x1;

			if(tp->patch < 0)
				continue;

			patch = W_CacheLumpNum(tp->patch, PU_CACHE);
			x0 = tp->originx;
			x1 = x0 + patch->width;

			if(x0 < 0)
				xx = 0;
			else
				xx = x0;

			if(x1 > 64)
				x1 = 64;

			for( ; xx < x1; xx++)
			{
				column_t *column = (column_t*)((uint8_t*)patch + patch->offs[xx - x0]);
				while(column->topdelta != 0xFF)
				{
					int32_t count, topdelta;
					uint8_t *source;
					uint8_t *dest;

					topdelta = column->topdelta + tp->originy;

					source = (uint8_t*)column + 3;
					count = column->length;

					if(topdelta < 0)
					{
						source -= topdelta;
						count += topdelta;
					}

					dest = data + xx + topdelta * 64;

					if(count + topdelta > 64)
						count = 64 - topdelta;

					while(count > 0)
					{
						*dest = *source++;
						dest += 64;
						count--;
					}

					column = (column_t*)((uint8_t*)column + column->length + 4);
				}
			}
		}

		Z_ChangeTag2(data, PU_CACHE);
	}

	return flattexture_data[idx];
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace 'R_FlatNumForName'
	{0x00034690, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)flat_num_get},
	// replace call to 'R_InitFlats' in 'R_InitData'
	{0x00034622, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)R_InitFlats},
	// replace call to 'W_CacheLumpNum' in 'R_PrecacheLevel' (flat caching)
	{0x0003483C, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)precache_flat},
};

