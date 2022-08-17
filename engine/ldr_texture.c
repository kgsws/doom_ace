// kgsws' ACE Engine
////
// Texture loading replacement.
// Adds support for simple textures between markers 'TX_START' and 'TX_END'.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
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

static uint32_t *numtextures;
static texture_t ***textures;
static uint16_t ***texturecolumnlump;
static uint16_t ***texturecolumnofs;
static uint8_t ***texturecomposite;
static uint32_t **texturecompositesize;
static uint32_t **texturewidthmask;
static fixed_t **textureheight;
uint32_t **texturetranslation;

static uint16_t *patch_lump;
static uint32_t tmp_count;

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
int32_t texture_num_get(uint8_t *name)
{
	uint64_t wame;
	uint32_t idx;
	texture_t **tex = *textures;

	// check for 'no texture'
	if(name[0] == '-')
		return 0;

	// search as 64bit number
	wame = wad_name64(name);

	// do a backward search
	idx = *numtextures;
	do
	{
		idx--;
		if(tex[idx]->wame == wame)
			return idx;
	} while(idx);

	// last texture is 'unknown texture'
	return *numtextures - 1;
}

__attribute((regparm(2),no_caller_saved_registers))
int32_t texture_num_check(uint8_t *name)
{
	// 'texture_num_get' is used more often, but this is sometines required too
	int32_t idx;

	idx = texture_num_get(name);
	if(idx == *numtextures - 1)
		return -1;

	return idx;
}

static __attribute((regparm(2),no_caller_saved_registers))
void load_textures()
{
	uint32_t i = 0;

	for( ; i < tmp_count; i++)
	{
		R_GenerateLookup(i);
		gfx_progress(1);
	}
}

//
// funcs

static uint32_t texture_size_check(uint8_t *name, uint32_t idx)
{
	texturedef_t *td;

	td = wad_cache_optional(name, NULL);
	if(td)
	{
		for(uint32_t i = 0; i < td->count; i++, idx++)
		{
			maptexure_t *mt = (void*)td + td->offset[i];
			uint32_t *dst = *texturecompositesize + idx;
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
			texture_t *tex = (*textures)[idx];

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
					I_Error("[ACE] texture %.8s is missing patch", mt->name);

				tp->originx = mp->originx;
				tp->originy = mp->originy;
				tp->patch = patch_lump[mp->patch];
			}

			// other pointers
			{
				uint32_t size;

				size = sizeof(texture_t) + tex->patchcount * sizeof(texpatch_t);
				(*texturecolumnlump)[i] = (void*)tex + size;

				size += tex->width * sizeof(uint16_t);
				(*texturecolumnofs)[i] = (void*)tex + size;
			}

			// resolution
			(*textureheight)[i] = (uint32_t)tex->height << FRACBITS;

			{
				uint32_t size = 1;
				while(size * 2 <= tex->width)
					size <<= 1;
				(*texturewidthmask)[i] = size - 1;
			}
		}

		doom_free(td);
	}

	return idx;
}

static void load_pnames()
{
	uint8_t *ptr;
	uint32_t size, count;

	// load PNAMES
	ptr = wad_cache_lump(wad_get_lump("PNAMES"), &size);

	// get entry count from size
	size -= sizeof(uint32_t);
	size /= 8;

	// check patch count
	count = *((uint32_t*)ptr);
	if(count > size)
		I_Error("[ACE] invalid PNAMES");

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

	wad_read_lump(&patch, li - *lumpinfo, sizeof(patch_t));

	(*texturewidthmask)[tmp_count] = patch.width;
	(*textureheight)[tmp_count] = patch.height;

	size = sizeof(texture_t);
	size += sizeof(texpatch_t);
	size += (uint32_t)patch.width * sizeof(uint16_t) * 2;

	(*texturecompositesize)[tmp_count] = size;

	tmp_count++;
}

static void cb_tx_load(lumpinfo_t *li)
{
	texture_t *tex;
	uint32_t size;
	uint32_t lump;

	lump = li - *lumpinfo;

	tex = ldr_malloc((*texturecompositesize)[tmp_count]);
	(*textures)[tmp_count] = tex;

	tex->wame = li->wame;
	tex->width = (*texturewidthmask)[tmp_count];
	tex->height = (*textureheight)[tmp_count];
	tex->patchcount = 1;
	tex->patch[0].originx = 0;
	tex->patch[0].originy = 0;
	tex->patch[0].patch = lump;

	(*textureheight)[tmp_count] <<= FRACBITS;

	size = 1;
	while(size * 2 <= tex->width)
		size <<= 1;
	(*texturewidthmask)[tmp_count] = size - 1;

	size = sizeof(texture_t) + tex->patchcount * sizeof(texpatch_t);
	(*texturecolumnlump)[tmp_count] = (void*)tex + size;

	size += tex->width * sizeof(uint16_t);
	(*texturecolumnofs)[tmp_count] = (void*)tex + size;

	tmp_count++;
}

//
// API

void init_textures(uint32_t count)
{
	// To avoid unnecessary memory fragmentation this function does multiple passes.
	uint32_t idx;

	doom_printf("[ACE] preparing %u textures\n", count);
	ldr_alloc_message = "Texture memory allocation failed!";

	count += EXTRA_TEXTURES;

	//
	// PASS 1

	*textures = ldr_malloc(count * 4);
	*texturecolumnlump = ldr_malloc(count * 4);
	*texturecolumnofs = ldr_malloc(count * 4);
	*texturecomposite = ldr_malloc(count * 4);
	*texturecompositesize = ldr_malloc(count * 4);
	*texturewidthmask = ldr_malloc(count * 4);
	*textureheight = ldr_malloc(count * 4);
	*texturetranslation = ldr_malloc(count * 4);

	for(uint32_t i = 0; i < count; i++)
		(*texturetranslation)[i] = i;

	//
	// PASS 2

	// get size of each texture[x]
	idx = texture_size_check("TEXTURE1", 0);
	texture_size_check("TEXTURE2", idx);

	// textures in TX_* block
	tmp_count = idx;
	wad_handle_range(0x5854, cb_tx_count);

	//
	// PASS 3

	// allocate memory for each texture[x]
	for(idx = 0 ; idx < count - EXTRA_TEXTURES; idx++)
		(*textures)[idx] = ldr_malloc((*texturecompositesize)[idx]);

	//
	// PASS 4

	load_pnames();

	//
	// PASS 5

	// load TEXTUREx info
	idx = texture_load("TEXTURE1", 0);
	texture_load("TEXTURE2", idx);

	// load TX_* info
	tmp_count = idx;
	wad_handle_range(0x5854, cb_tx_load);

	//
	// PASS 6

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

		(*texturewidthmask)[tmp_count] = size - 1;
		(*textureheight)[tmp_count] = (uint32_t)tex->height << FRACBITS;
		(*texturecolumnlump)[tmp_count] = (void*)tex->patch;
		(*texturecolumnofs)[tmp_count] = it->columnofs;
		(*texturecomposite)[tmp_count] = it->composite;
		(*textures)[tmp_count] = tex;

		memset((void*)tex->patch, 0xFF, (uint32_t)tex->width * sizeof(uint16_t));
	}

	//
	// DONE

	*numtextures = count;
	tmp_count = count - EXTRA_TEXTURES;

	return;
}

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'R_InitTextures' in 'R_InitData'
	{0x00034610, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)load_textures},
	// replace 'R_TextureNumForName'
	{0x00034750, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)texture_num_get},
	// import variables
	{0x000300E4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numtextures},
	{0x000300E0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&textures},
	{0x000300F4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturecolumnlump},
	{0x000300D0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturecolumnofs},
	{0x000300E8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturecomposite},
	{0x000300D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturecompositesize},
	{0x000300D8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturewidthmask},
	{0x00030124, DATA_HOOK | HOOK_IMPORT, (uint32_t)&textureheight},
	{0x0003010C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturetranslation},
};

