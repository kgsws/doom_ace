// kgsws' ACE Engine
////
// Texture loading replacement.
// Adds support for simple textures between markers 'TX_START' and 'TX_END'.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "config.h"
#include "wadfile.h"
#include "ldr_texture.h"

#define EXTRA_TEXTURES	1	// this is 'missing texture'

typedef struct
{
	union
	{
		uint8_t name[8];
		uint32_t same[2];
		uint64_t wame;
	};
	uint16_t width;
	uint16_t height;
	uint16_t *columnofs;
	uint8_t *composite;
} internal_texture_t;

typedef struct
{
	int32_t count;
	uint32_t offset[];
} texturedef_t;

typedef struct mappatch_s
{
	int16_t originx;
	int16_t originy;
	uint16_t patch;
	uint16_t unused[2];
} __attribute__((packed)) mappatch_t;

typedef struct maptexure_s
{
	union
	{
		uint8_t name[8];
		uint32_t same[2];
		uint64_t wame;
	};
	uint16_t flags;
	uint8_t scalex;
	uint8_t scaley;
	uint16_t width;
	uint16_t height;
	uint32_t unused;
	uint16_t patchcount;
	mappatch_t patch[];
} __attribute__((packed)) maptexure_t;

//

uint_fast8_t tex_was_composite;

uint8_t *textureheightpow;

static uint16_t *patch_lump;
static uint32_t tmp_count;

static uint_fast8_t have_doomtex;

// internal textures

static uint8_t tex_unknown_data[] =
{
	96, 96, 96, 96, 96, 96, 96, 96, 111, 111, 111, 111, 111, 111, 111, 111,
	96, 96, 96, 96, 96, 96, 96, 96, 111, 111, 111, 111, 111, 111, 111, 111,
	96, 96, 96, 96, 96, 96, 96, 96, 111, 111, 111, 111, 111, 111, 111, 111,
	96, 96, 96, 96, 96, 96, 96, 96, 111, 111, 111, 111, 111, 111, 111, 111,
	96, 96, 96, 96, 96, 96, 96, 96, 111, 111, 111, 111, 111, 111, 111, 111,
	96, 96, 96, 96, 96, 96, 96, 96, 111, 111, 111, 111, 111, 111, 111, 111,
	96, 96, 96, 96, 96, 96, 96, 96, 111, 111, 111, 111, 111, 111, 111, 111,
	96, 96, 96, 96, 96, 96, 96, 96, 111, 111, 111, 111, 111, 111, 111, 111,
	96, 96, 96, 96, 96, 96, 96, 96
};

static uint16_t tex_unknown_offs[] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 8, 8
};

static internal_texture_t internal_texture[EXTRA_TEXTURES] =
{
	{
		// this is 'missing texture' and must be last
		.width = 16,
		.height = 128,
		.columnofs = tex_unknown_offs,
		.composite = tex_unknown_data,
	}
};

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
int32_t texture_num_get(const uint8_t *name)
{
	uint64_t wame;
	uint32_t idx;

	// check for 'no texture'
	if(name[0] == '-')
		return 0;

	// search as 64bit number
	wame = wad_name64(name);

	// do a backward search
	idx = numtextures;
	do
	{
		idx--;
		if(textures[idx]->wame == wame)
			return idx;
	} while(idx);

	// last texture is 'unknown texture'
	return numtextures - 1;
}

__attribute((regparm(2),no_caller_saved_registers))
int32_t texture_num_check(const uint8_t *name)
{
	// 'texture_num_get' is used more often, but this is sometines required too
	int32_t idx;

	idx = texture_num_get(name);
	if(idx == numtextures - 1)
		return -1;

	return idx;
}

__attribute((regparm(2),no_caller_saved_registers))
uint64_t texture_get_name(uint32_t idx)
{
	if(!idx)
		return '-';

	if(idx >= numtextures)
		return 0;

	return textures[idx]->wame;
}

//
// funcs

static uint32_t get_height_pow(uint32_t height)
{
	uint32_t i = 0;

	while(1)
	{
		uint32_t res = 1 << i;
		if(res >= 1024)
			break;
		if(res >= height)
			break;
		i++;
	}

	return i;
}

static uint32_t texture_size_check(uint8_t *name, uint32_t idx)
{
	texturedef_t *td;

	td = wad_cache_optional(name, NULL);
	if(td)
	{
		for(uint32_t i = 0; i < td->count; i++, idx++)
		{
			maptexure_t *mt = (void*)td + td->offset[i];
			uint32_t *dst = texturecompositesize + idx;
			uint32_t size;

			// header
			size = sizeof(texture_t);

			// patches
			size += mt->patchcount * sizeof(texpatch_t);

			// column lump & column offset
			size += mt->width * sizeof(uint16_t) * 2;

			// done
			*dst = size;
		}

		doom_free(td);
	}

	return idx;
}

static uint32_t texture_load(uint8_t *name, uint32_t idx)
{
	texturedef_t *td;

	td = wad_cache_optional(name, NULL);
	if(td)
	{
		for(uint32_t i = 0; i < td->count; i++, idx++)
		{
			maptexure_t *mt = (void*)td + td->offset[i];
			texture_t *tex = textures[idx];

			// copy texture info
			tex->wame = mt->wame;
			tex->width = mt->width;
			tex->height = mt->height;
			tex->patchcount = mt->patchcount;

			// copy patches
			for(uint32_t j = 0; j < mt->patchcount; j++)
			{
				mappatch_t *mp = mt->patch + j;
				texpatch_t *tp = tex->patch + j;

				if(mp->patch >= tmp_count || patch_lump[mp->patch] == 0xFFFF)
					engine_error("TEXTURE", "Texture %.8s is missing patch!", mt->name);

				tp->originx = mp->originx;
				tp->originy = mp->originy;
				tp->patch = patch_lump[mp->patch];
			}

			// other pointers
			{
				uint32_t size;

				size = sizeof(texture_t) + tex->patchcount * sizeof(texpatch_t);
				texturecolumnlump[idx] = (void*)tex + size;

				size += tex->width * sizeof(uint16_t);
				texturecolumnofs[idx] = (void*)tex + size;
			}

			// resolution
			textureheight[idx] = (uint32_t)tex->height << FRACBITS;

			{
				uint32_t size = 1;
				while(size * 2 <= tex->width)
					size <<= 1;
				texturewidthmask[idx] = size - 1;
			}

			textureheightpow[idx] = get_height_pow(tex->height);
		}

		doom_free(td);
	}

	return idx;
}

static uint32_t load_pnames()
{
	uint8_t *ptr;
	uint32_t size, count;

	// load PNAMES
	ptr = wad_cache_lump(wad_get_lump(dtxt_pnames), &size);

	// get entry count from size
	size -= sizeof(uint32_t);
	size /= 8;

	// check patch count
	count = *((uint32_t*)ptr);
	if(count > size)
		engine_error("TEXTURE", "Invalid %s!", dtxt_pnames);

	// prepare patch table
	patch_lump = (uint16_t*)ptr;
	ptr += sizeof(uint32_t);

	// check every patch
	tmp_count = count;
	for(uint32_t i = 0; i < count; i++, ptr += 8)
		patch_lump[i] = wad_check_lump(ptr);
}

//
// callbacks

static void cb_tx_count(lumpinfo_t *li)
{
	uint32_t size;
	patch_t patch;

	wad_read_lump(&patch, li - lumpinfo, sizeof(patch_t));

	if(*((uint64_t*)&patch) == 0xA1A0A0D474E5089)
		engine_error("TEXTURE", "Patch '%.8s' is a PNG!", li->name);

	texturewidthmask[tmp_count] = patch.width;
	textureheight[tmp_count] = patch.height;
	textureheightpow[tmp_count] = get_height_pow(patch.height);

	size = sizeof(texture_t);
	size += sizeof(texpatch_t);
	size += (uint32_t)patch.width * sizeof(uint16_t) * 2;

	texturecompositesize[tmp_count] = size;

	tmp_count++;
}

static void cb_tx_load(lumpinfo_t *li)
{
	texture_t *tex;
	uint32_t size;
	uint32_t lump;

	lump = li - lumpinfo;

	tex = ldr_malloc(texturecompositesize[tmp_count]);
	textures[tmp_count] = tex;

	tex->wame = li->wame;
	tex->width = texturewidthmask[tmp_count];
	tex->height = textureheight[tmp_count];
	tex->patchcount = 1;
	tex->patch[0].originx = 0;
	tex->patch[0].originy = 0;
	tex->patch[0].patch = lump;

	textureheight[tmp_count] <<= FRACBITS;

	size = 1;
	while(size * 2 <= tex->width)
		size <<= 1;
	texturewidthmask[tmp_count] = size - 1;

	size = sizeof(texture_t) + tex->patchcount * sizeof(texpatch_t);
	texturecolumnlump[tmp_count] = (void*)tex + size;

	size += tex->width * sizeof(uint16_t);
	texturecolumnofs[tmp_count] = (void*)tex + size;

	tmp_count++;
}

//
// API

uint32_t count_textures()
{
	uint32_t temp;
	int32_t idx;
	uint32_t count = 0;

	//
	// check what exploit broke first

	if(mod_config.enable_dehacked)
	{
		if(!strcmp(dtxt_pnames, "PNAMES"))
			engine_error("TEXTURE", "Rename %s using DEHACKED!", dtxt_pnames);
		if(!strcmp(dtxt_texture1, "TEXTURE1"))
			engine_error("TEXTURE", "Rename %s using DEHACKED!", dtxt_texture1);
		if(!strcmp(dtxt_texture2, "TEXTURE2"))
			engine_error("TEXTURE", "Rename %s using DEHACKED!", dtxt_texture2);
	} else
	{
		// use hardcoded names when DEHACKED is disabled
		strcpy(dtxt_pnames, "PATCHES");
		strcpy(dtxt_texture1, "DOOMTEX1");
		strcpy(dtxt_texture2, "DOOMTEX2");
	}

	//
	// count textures

	if(wad_check_lump(dtxt_pnames) < 0)
		return 0;

	idx = wad_check_lump(dtxt_texture1);
	if(idx >= 0)
	{
		wad_read_lump(&temp, idx, 4);
		count += temp;
		have_doomtex = 1;
	}

	idx = wad_check_lump(dtxt_texture2);
	if(idx >= 0)
	{
		wad_read_lump(&temp, idx, 4);
		count += temp;
		have_doomtex = 1;
	}

	return count;
}

void init_textures(uint32_t count)
{
	// To avoid unnecessary memory fragmentation, this function does multiple passes.
	uint32_t idx;

	doom_printf("[ACE] preparing %u textures\n", count);
	ldr_alloc_message = "Texture";

	count += EXTRA_TEXTURES;

	//
	// PASS 1

	textures = ldr_malloc(count * sizeof(void*));
	texturecolumnlump = ldr_malloc(count * sizeof(void*));
	texturecolumnofs = ldr_malloc(count * sizeof(void*));
	texturecomposite = ldr_malloc(count * sizeof(void*));
	texturecompositesize = ldr_malloc(count * sizeof(uint32_t));
	texturewidthmask = ldr_malloc(count * sizeof(uint32_t));
	textureheight = ldr_malloc(count * sizeof(fixed_t));
	texturetranslation = ldr_malloc(count * sizeof(uint32_t));
	textureheightpow = ldr_malloc(count);

	for(uint32_t i = 0; i < count; i++)
		texturetranslation[i] = i;

	//
	// PASS 2

	if(have_doomtex)
	{
		// get size of each texture[x]
		idx = texture_size_check(dtxt_texture1, 0);
		tmp_count = texture_size_check(dtxt_texture2, idx);
	}

	// textures in TX_* block
	wad_handle_range(0x5854, cb_tx_count);

	//
	// PASS 3

	// allocate memory for each texture[x]
	for(idx = 0 ; idx < count - EXTRA_TEXTURES; idx++)
		textures[idx] = ldr_malloc(texturecompositesize[idx]);

	//
	// PASS 4

	if(have_doomtex)
		load_pnames();

	//
	// PASS 5

	if(have_doomtex)
	{
		// load TEXTUREx info
		idx = texture_load(dtxt_texture1, 0);
		tmp_count = texture_load(dtxt_texture2, idx);
	} else
		tmp_count = 0;

	// load TX_* info
	wad_handle_range(0x5854, cb_tx_load);

	//
	// PASS 6

	if(patch_lump)
		doom_free(patch_lump);

	// add internal textures
	for(uint32_t i = 0; i < EXTRA_TEXTURES; i++, tmp_count++)
	{
		uint32_t size;
		texture_t *tex;
		internal_texture_t *it = internal_texture + i;

		size = sizeof(texture_t);
		size += it->width * sizeof(uint16_t);

		tex = ldr_malloc(size);
		tex->wame = it->wame;
		tex->width = it->width;
		tex->height = it->height;
		tex->patchcount = 0;

		size = 1;
		while(size * 2 <= tex->width)
			size <<= 1;

		texturewidthmask[tmp_count] = size - 1;
		textureheight[tmp_count] = (uint32_t)tex->height << FRACBITS;
		texturecolumnlump[tmp_count] = (void*)tex->patch;
		texturecolumnofs[tmp_count] = it->columnofs;
		texturecomposite[tmp_count] = it->composite;
		textureheightpow[tmp_count] = get_height_pow(tex->height);
		textures[tmp_count] = tex;

		memset((void*)tex->patch, 0xFF, (uint32_t)tex->width * sizeof(uint16_t));
	}

	//
	// DONE

	numtextures = count;
	tmp_count = count - EXTRA_TEXTURES;

	return;
}

__attribute((regparm(2),no_caller_saved_registers))
void tex_init_data()
{
	for(uint32_t i; i < tmp_count; i++)
	{
		R_GenerateLookup(i);
		gfx_progress(1);
	}
}

uint8_t *texture_get_column(uint32_t tex, uint32_t col)
{
	int32_t lump;
	uint32_t offs;

	col &= texturewidthmask[tex];
	lump = texturecolumnlump[tex][col];
	offs = texturecolumnofs[tex][col];

	if(lump != 0xFFFF)
	{
		tex_was_composite = 0;
		return W_CacheLumpNum(lump, PU_CACHE) + offs;
	}

	if(!texturecomposite[tex])
		R_GenerateComposite(tex);

	tex_was_composite = 1;
	return texturecomposite[tex] + offs;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace 'R_TextureNumForName'
	{0x00034750, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)texture_num_get},
};

