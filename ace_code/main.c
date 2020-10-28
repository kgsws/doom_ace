// kgsws' Doom ACE
// Main entry.
// All the hooks.
// Graphical loader.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "stbar.h"

// progres bar task weight
#define PBAR_FLR_SPR	32	// flats / sprite processing
#define PBAR_NEW	32	// new stuff // TODO: split
#define PBAR_P_HU_ST	32	// orignal calls; P_Init, S_Init, HU_Init, ST_Init
#define PBAR_ADD	(PBAR_FLR_SPR+PBAR_NEW+PBAR_P_HU_ST)

// temporary storage in screens[]
#define MAX_PBAR_SIZE	68200
#define MAX_PNAMES	8192
#define MAX_TEX_SIZE	((SCREENSIZE*4) - (MAX_PBAR_SIZE + MAX_PNAMES*sizeof(int32_t)))
#define pbar_ptr	screens0
#define pnames_ptr	(screens0+MAX_PBAR_SIZE)
#define textures_ptr	(screens0+MAX_PBAR_SIZE+MAX_PNAMES*sizeof(int32_t))

typedef struct
{
	int16_t x, y;
	uint16_t patch;
	uint16_t stepdir;
	uint16_t colormap;
} __attribute__((packed)) lpatch_t;

typedef struct
{
	union
	{
		char name[8];
		uint64_t wame;
	};
	uint32_t masked;
	uint16_t width, height;
	uint32_t obsolete;
	uint16_t count;
	lpatch_t patch[];
} __attribute__((packed)) ltex_t;

static void custom_RenderPlayerView(player_t*) __attribute((regparm(2),no_caller_saved_registers));
static int custom_R_FlatNumForName(char*) __attribute((regparm(2),no_caller_saved_registers));
static void *vissprite_cache_lump(vissprite_t*) __attribute((regparm(2),no_caller_saved_registers));

// texture info
static uint32_t *numtextures;
static texture_t ***textures;
static int16_t ***texturecolumnlump;
static uint16_t ***texturecolumnofs;
static uint8_t ***texturecomposite;
static int32_t **texturecompositesize;
static uint32_t **texturewidthmask;
static fixed_t **textureheight;
static uint32_t **texturetranslation;

// sprite info
static uint32_t *firstspritelump;
static fixed_t **spritewidth;
static fixed_t **spriteoffset;
static fixed_t **spritetopoffset;
static uint16_t *sprite_lump;
static int32_t *spr_maxframe;
static spriteframe_t *spr_temp;
static char **spr_names;
static uint32_t *numsprites;
static spritedef_t **sprites;

// flat info
static uint32_t *numflats;
static uint32_t *firstflat;
static uint32_t **flattranslation;

// stuff
static uint32_t *grmode;
static uint32_t *numChannels;
static void **channels;

// TODO: move
static void **colormaps;

// all the hooks for ACE engine
static hook_t hook_list[] =
{
	// common
	{0x00074FA0, DATA_HOOK | HOOK_READ32, (uint32_t)&numlumps},
	{0x00074FA4, DATA_HOOK | HOOK_READ32, (uint32_t)&lumpinfo},
	{0x00074F94, DATA_HOOK | HOOK_READ32, (uint32_t)&lumpcache},
	{0x0002B698, DATA_HOOK | HOOK_IMPORT, (uint32_t)&screenblocks},
	{0x00012D90, DATA_HOOK | HOOK_IMPORT, (uint32_t)&weaponinfo},
	{0x0002914C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&destscreen},
	{0x00074FC4, DATA_HOOK | HOOK_READ32, (uint32_t)&screens0},
	// stuff
	{0x00029134, DATA_HOOK | HOOK_IMPORT, (uint32_t)&grmode},
	{0x00075C94, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numChannels},
	{0x00075C98, DATA_HOOK | HOOK_IMPORT, (uint32_t)&channels},
	// textures
	{0x000300e4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numtextures},
	{0x000300e0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&textures},
	{0x000300f4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturecolumnlump},
	{0x000300d0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturecolumnofs},
	{0x000300e8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturecomposite},
	{0x000300d4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturecompositesize},
	{0x000300d8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturewidthmask},
	{0x00030124, DATA_HOOK | HOOK_IMPORT, (uint32_t)&textureheight},
	{0x0003010c, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturetranslation},
	// sprites
	{0x00030120, DATA_HOOK | HOOK_IMPORT, (uint32_t)&firstspritelump},
	{0x00030100, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spritewidth},
	{0x00030118, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spriteoffset},
	{0x00030114, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spritetopoffset},
	{0x0005C884, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spr_maxframe},
	{0x0005C554, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spr_temp},
	{0x00015800, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spr_names},
	{0x0005C8E0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsprites},
	{0x0005C8E4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sprites},
	{0x00037b96, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)vissprite_cache_lump},
	{0x00037B9B, CODE_HOOK | HOOK_UINT16, 0x0EEB},
	// flats
	{0x000300DC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numflats},
	{0x000300FC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&firstflat},
	{0x00030110, DATA_HOOK | HOOK_IMPORT, (uint32_t)&flattranslation},
	{0x00034690, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)custom_R_FlatNumForName},
	// render
	{0x00030104, DATA_HOOK | HOOK_IMPORT, (uint32_t)&colormaps},
	// stbar.c
	{0x000752f0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallnum},
	{0x00075458, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallpercent},
	// disable "store demo" mode check
//	{0x0001d113, CODE_HOOK | HOOK_UINT16, 0x4CEB}, // 'jmp'
	// invert 'run key' function (auto run)
	{0x0001fbc5, CODE_HOOK | HOOK_UINT8, 0x01},
	// custom overlay stuff
	{0x0001d362, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)custom_RenderPlayerView},
	// make invalid sprites invisible
//	{0x00037d4c, CODE_HOOK | HOOK_JMP_DOOM, 0x00037f86}, // invalid sprite
//	{0x00037d7a, CODE_HOOK | HOOK_JMP_DOOM, 0x00037f86}, // invalid sprite frame
//	{0x00038029, CODE_HOOK | HOOK_JMP_DOOM, 0x00038203}, // invalid Psprite
//	{0x00038059, CODE_HOOK | HOOK_JMP_DOOM, 0x00038203}, // invalid Psprite frame
	// DEBUG STUFF TO BE REMOVED OR FIXED
	{0x00034780, CODE_HOOK | HOOK_UINT8, 0xC3}, // disable 'R_PrecacheLevel'; TODO: fix
	{0x0002fc8b, CODE_HOOK | HOOK_UINT8, 0xEB}, // disable animations; TODO: rewrite 'P_UpdateSpecials'
	// terminator
	{0}
};

//
// imported variables
uint32_t numlumps;
lumpinfo_t *lumpinfo;
void **lumpcache;
uint32_t *screenblocks;
weaponinfo_t *weaponinfo;
uint8_t **destscreen;
uint8_t *screens0;

// progress bar info
static uint32_t pbar_step;
static patch_t *pbar_patch;

// this function is called when 3D view should be drawn
// - only in level
// - not in automap
static __attribute((regparm(2),no_caller_saved_registers))
void custom_RenderPlayerView(player_t *pl)
{
	// actually render 3D view
	R_RenderPlayerView(pl);

	// status bar
	stbar_draw(pl);
}

//
// section search
static int section_count_lumps(const char *start, const char *end)
{
	int lump = numlumps - 1;
	int inside = 0;
	int count = 0;

	// go backwards trough all the lumps
	while(lump >= 0)
	{
		if(lumpinfo[lump].wame == *((uint64_t*)start))
		{
			if(!inside)
				// unclosed section
				I_Error("[ACE] %s without %s", start, end);
			// close this section
			inside = 0;
		} else
		if(lumpinfo[lump].wame == *((uint64_t*)end))
		{
			if(inside)
				// unclosed section
				I_Error("[ACE] %s without %s", end, start);
			// open new section
			inside = 1;
			// do not count this one
			count--;
		}
		if(inside)
			count++;
		lump--;
	}

	if(inside)
		// unclosed section
		I_Error("[ACE] %s without %s", end, start);

	return count;
}

//
// graphical loader
static void init_lgfx()
{
	patch_t *p;
	uint32_t lmp, tmp;

	// fill all video buffers with background
	p = W_CacheLumpName("\xCC""LOADING", PU_STATIC);
	*destscreen = (void*)0xA0000;
	V_DrawPatchDirect(0, 0, 0, p);
	*destscreen = (void*)0xA4000;
	V_DrawPatchDirect(0, 0, 0, p);
	*destscreen = (void*)0xA8000;
	V_DrawPatchDirect(0, 0, 0, p);
	Z_Free(p);

	// redraw
	I_FinishUpdate();

	// load foreground
	// use screens[0] as a temporary storage
	lmp = W_GetNumForName("\xCC""LOADBAR");
	tmp = W_LumpLength(lmp);
	if(tmp > MAX_PBAR_SIZE) // allow fullscreen LOADBAR
		I_Error("[ACE] invalid LOADBAR");
	W_ReadLump(lmp, pbar_ptr);
	pbar_patch = (patch_t*)pbar_ptr;
	// this will cause error if patch is invalid
	V_DrawPatch(0, 0, 2, pbar_patch);
}

static void pbar_set(uint32_t count)
{
	register uint32_t tmp;
	count *= pbar_step;

	tmp = count >> 23;
	if(tmp != pbar_patch->width)
	{
		pbar_patch->width = tmp;
		V_DrawPatchDirect(0, 0, 0, pbar_patch);
		I_FinishUpdate();
	}
}

//
// TEXTUREx loader
static uint32_t texture_read(const char *name)
{
	int lmp;
	uint32_t ret;

	lmp = W_CheckNumForName(name);
	if(lmp < 0)
		return 0;

	ret = W_LumpLength(lmp);
	if(ret > MAX_TEX_SIZE)
		I_Error("[ACE] %s is too big", name);

	W_ReadLump(lmp, textures_ptr);
	ret = *((uint32_t*)textures_ptr);

	return ret;
}

static uint32_t texture_process(uint32_t idx)
{
	uint32_t count = *((uint32_t*)textures_ptr);
	uint32_t *dir = (uint32_t*)textures_ptr + 1;

	do
	{
		texture_t *tex;
		texpatch_t *pat;
		lpatch_t *srp;
		ltex_t *src = (ltex_t*)(textures_ptr + *dir);

		tex = Z_Malloc(sizeof(texture_t) + sizeof(texpatch_t) * src->count, PU_STATIC, NULL);
		(*textures)[idx] = tex;

		tex->width = src->width;
		tex->height = src->height;
		tex->count = src->count;
		tex->wame = src->wame;

		srp = src->patch;
		pat = tex->patch;

		for(uint32_t i = 0; i < tex->count; i++, srp++, pat++)
		{
			pat->x = srp->x;
			pat->y = srp->y;
			pat->p = ((int32_t*)pnames_ptr)[srp->patch];
			if(pat->p < 0)
			{
				tex->width = 0; // string terminator
				I_Error("[ACE] texture %s has missing patch", tex->name);
			}
		}
		(*texturecolumnlump)[idx] = Z_Malloc(tex->width * sizeof(uint16_t), PU_STATIC, NULL);
		(*texturecolumnofs)[idx] = Z_Malloc(tex->width * sizeof(uint16_t), PU_STATIC, NULL);

		{
			uint32_t bits = 1;
			while(bits < tex->width)
				bits <<= 1;
			(*texturewidthmask)[idx] = bits - 1;
			(*textureheight)[idx] = (int32_t)tex->height << FRACBITS;
		}

		// do not update progress here
		// update after calling R_GenerateLookup

		dir++;
		idx++;
	} while(--count);

	return idx;
}

//
// TX section loader
static uint32_t tx_process(uint32_t idx, const char *start, const char *end)
{
	int inside = 0;
	patch_t header;

	for(uint32_t i = 0; i < numlumps; i++)
	{
		if(lumpinfo[i].wame == *((uint64_t*)start))
		{
			inside = 1;
			continue;
		}
		if(lumpinfo[i].wame == *((uint64_t*)end))
		{
			inside = 0;
			continue;
		}
		if(!inside)
			continue;

		texture_t *tex;

		tex = Z_Malloc(sizeof(texture_t) + sizeof(texpatch_t), PU_STATIC, NULL);
		(*textures)[idx] = tex;

		// must read patch header here
		// but do not cache it, yet
		lseek(lumpinfo[i].handle, lumpinfo[i].position, 0);
		read(lumpinfo[i].handle, &header, sizeof(header));

		tex->width = header.width;
		tex->height = header.height;
		tex->count = 1;
		tex->wame = lumpinfo[i].wame;
		tex->patch[0].x = 0;
		tex->patch[0].y = 0;
		tex->patch[0].p = i;

		(*texturecolumnlump)[idx] = Z_Malloc(tex->width * sizeof(uint16_t), PU_STATIC, NULL);
		(*texturecolumnofs)[idx] = Z_Malloc(tex->width * sizeof(uint16_t), PU_STATIC, NULL);

		{
			uint32_t bits = 1;
			while(bits < tex->width)
				bits <<= 1;
			(*texturewidthmask)[idx] = bits - 1;
			(*textureheight)[idx] = (int32_t)tex->height << FRACBITS;
		}

		// call original function
		R_GenerateLookup(idx);

		// free this patch to avoid fragmentation
		Z_Free(lumpcache[i]);

		// animation stuff
		(*texturetranslation)[idx] = idx;

		idx++;
		// update progress
		pbar_set(idx);
	}

	return idx;
}

//
// sprite section loader
int sprite_process(int idx, const char *start, const char *end)
{
	int sidx = 0;
	patch_t header;
	int inside = 0;

	for(uint32_t i = 0; i < numlumps; i++)
	{
		if(lumpinfo[i].wame == *((uint64_t*)start))
		{
			inside = 1;
			continue;
		}
		if(lumpinfo[i].wame == *((uint64_t*)end))
		{
			inside = 0;
			continue;
		}
		if(!inside)
			continue;

		// must read patch header here
		// but do not cache it
		lseek(lumpinfo[i].handle, lumpinfo[i].position, 0);
		read(lumpinfo[i].handle, &header, sizeof(header));

		// fill sprite info
		(*spritewidth)[sidx] = (int32_t)header.width << FRACBITS;
		(*spriteoffset)[sidx] = (int32_t)header.x << FRACBITS;
		(*spritetopoffset)[sidx] = (int32_t)header.y << FRACBITS;
		sprite_lump[sidx] = i;

		sidx++;
		idx++;
		// update progress
		pbar_set(idx);
	}
	return idx;
}

//
// flat section loader
void flat_translation(const char *start, const char *end)
{
	int idx = 0;
	int inside = 0;

	for(uint32_t i = 0; i < numlumps; i++)
	{
		if(lumpinfo[i].wame == *((uint64_t*)start))
		{
			inside = 1;
			continue;
		}
		if(lumpinfo[i].wame == *((uint64_t*)end))
		{
			inside = 0;
			continue;
		}
		if(!inside)
			continue;

		(*flattranslation)[idx] = i;
		idx++;
	}
}

//
// sprite lookup tables
void sprite_table_loop(uint32_t match, const char *start, const char *end)
{
	int sidx = 0;
	int inside = 0;

	for(uint32_t i = 0; i < numlumps; i++)
	{
		if(lumpinfo[i].wame == *((uint64_t*)start))
		{
			inside = 1;
			continue;
		}
		if(lumpinfo[i].wame == *((uint64_t*)end))
		{
			inside = 0;
			continue;
		}
		if(!inside)
			continue;

		// DEBUG: REMOVE
		if(sprite_lump[sidx] != i)
			I_Error("FAILED!");

		if(lumpinfo[i].same == match)
		{
			uint8_t frm = lumpinfo[i].name[4] - 'A';
			uint8_t rot = lumpinfo[i].name[5] - '0';

			R_InstallSpriteLump(sidx, frm, rot, 0);

			if(lumpinfo[i].name[6])
			{
				frm = lumpinfo[i].name[6] - 'A';
				rot = lumpinfo[i].name[7] - '0';
				R_InstallSpriteLump(sidx, frm, rot, 1);
			}
		}
		sidx++;
	}
}

void sprite_table_gen()
{
	int count = 0;
	char **table = spr_names;

	while(*table)
		table++;
	count = table - spr_names;
	*numsprites = count;

	*sprites = Z_Malloc(count * sizeof(spritedef_t), PU_STATIC, NULL);

	table = spr_names;
	for(uint32_t i = 0; i < count; i++, table++)
	{
		uint32_t match = *((uint32_t*)*table);

		memset(spr_temp, 0xFF, 29 * sizeof(spriteframe_t));
		*spr_maxframe = -1;

		sprite_table_loop(match, "S_START", "S_END\x00\x00");
		int found = *spr_maxframe;

		if(found < 0)
		{
			(*sprites)[i].count = 0;
			continue;
		}
		found++;

		// TODO: check rotations & stuff

		(*sprites)[i].count = found;
		(*sprites)[i].frames = Z_Malloc(found * sizeof(spriteframe_t), PU_STATIC, NULL);
		memcpy((*sprites)[i].frames, spr_temp, found * sizeof(spriteframe_t));
	}
}

//
// new loader with progressbar
void do_loader()
{
	void *ptr;
	int32_t *patch_lump;
	uint32_t patch_count;
	uint32_t tmp, tmpA;
	uint32_t tex_count;
	uint32_t pbar_total;
	uint32_t idx;

	// disable disk icon
	*grmode = 0;

	// setup graphics
	init_lgfx();

	// parse PNAMES
	{
		char *name;

		ptr = W_CacheLumpName("PNAMES", PU_STATIC);
		patch_count = *((uint32_t*)ptr);
		if(patch_count > MAX_PNAMES)
			I_Error("[ACE] too many PNAMES");

		patch_lump = (int32_t*)pnames_ptr;
		name = ptr + sizeof(uint32_t);
		for(uint32_t i = 0; i < patch_count; i++, name += 8)
			patch_lump[i] = W_CheckNumForName(name);

		Z_Free(ptr);
	}

	// count all the textures in each TX_START-TX_END section
	tex_count = section_count_lumps("TX_START", "TX_END\x00");

	// count TEXTUREx
	tex_count += texture_read("TEXTURE2");
	tmp = texture_read("TEXTURE1");
	tex_count += tmp;

	// count all the sprites in each S_START-S_END section
	tmpA = section_count_lumps("S_START", "S_END\x00\x00");

	// total for progress bar
	pbar_total = tex_count;
	pbar_total += tmpA;
	pbar_step = (((uint32_t)pbar_patch->width) << 23) / (pbar_total + PBAR_ADD);
	pbar_patch->width = 0;

	// memory for textures
	*numtextures = tex_count;
	*textures = Z_Malloc(tex_count * 4, PU_STATIC, NULL);
	*texturecolumnlump = Z_Malloc(tex_count * 4, PU_STATIC, NULL);
	*texturecolumnofs = Z_Malloc(tex_count * 4, PU_STATIC, NULL);
	*texturecomposite = Z_Malloc(tex_count * 4, PU_STATIC, NULL);
	*texturecompositesize = Z_Malloc(tex_count * 4, PU_STATIC, NULL);
	*texturewidthmask = Z_Malloc(tex_count * 4, PU_STATIC, NULL);
	*textureheight = Z_Malloc(tex_count * 4, PU_STATIC, NULL);
	*texturetranslation = Z_Malloc((tex_count+1) * 4, PU_STATIC, NULL); // TODO: why +1 ?

	// memory for sprites
	*firstspritelump = 0;
	*spritewidth = Z_Malloc(tmpA * sizeof(fixed_t), PU_STATIC, NULL);
	*spriteoffset = Z_Malloc(tmpA * sizeof(fixed_t), PU_STATIC, NULL);
	*spritetopoffset = Z_Malloc(tmpA * sizeof(fixed_t), PU_STATIC, NULL);
	sprite_lump = Z_Malloc(tmpA * sizeof(uint16_t), PU_STATIC, NULL); // new lump lookup table

	// process TX sections
	idx = tx_process(0, "TX_START", "TX_END\x00");
	tmpA = idx;
	// process TEXTURE1
	if(tmp)
		idx = texture_process(idx);
	// process TEXTURE2
	if(texture_read("TEXTURE2"))
		idx = texture_process(idx);

	// generate TEXTUREx
	for(uint32_t i = tmpA; i < idx; i++)
	{
		// call original function
		R_GenerateLookup(i);
		(*texturetranslation)[i] = i;
		// update progress
		pbar_set(i + 1);
	}

	// free all the patches used by original function
	// this is here to avoid early memory fragmentation
	for(uint32_t i = 0; i < patch_count; i++)
	{
		if(lumpcache[i])
			Z_Free(lumpcache[i]);
	}

	// process sprite lumps
	idx = sprite_process(idx, "S_START", "S_END\x00\x00");

	// count all the flats in each F_START-F_END section
	tmp = section_count_lumps("F_START", "F_END\x00\x00");
	*numflats = tmp;
	*firstflat = 0;

	// generate translation table
	*flattranslation = Z_Malloc((tmp+1) * 4, PU_STATIC, NULL); // TODO: why +1 ?
	flat_translation("F_START", "F_END\x00\x00");

	// generate sprite tables
	sprite_table_gen();

	// update progress
	idx += PBAR_FLR_SPR;
	pbar_set(idx);

	// TODO: animations

	// colormap
	{
		uint32_t val;
		tmp = W_GetNumForName("COLORMAP");
		tmpA = W_LumpLength(tmp);
		val = (uint32_t)Z_Malloc(tmpA + 255, PU_STATIC, NULL);
		val += 255;
		val &= 0xFFFFFF00;
		*colormaps = (void*)val;
		W_ReadLump(tmp, (void*)val);
		// TODO: move
		R_InitLightTables();
	}

	// update progress
	idx += PBAR_NEW;
	pbar_set(idx);

	// S_Init - memory allocation only
	*channels = Z_Malloc(*numChannels * 12, PU_STATIC, NULL);

	// init other doom stuff
	P_InitSwitchList(); // TODO: custom
	HU_Init();
	ST_Init();
	R_InitSkyMap();

	// update progress
	idx += PBAR_P_HU_ST;
	pbar_set(idx);

	// eable disk icon
	*grmode = 1;
}

//
// flat texture search replacement
static __attribute((regparm(2),no_caller_saved_registers))
int custom_R_FlatNumForName(char *name)
{
	int idx = 0;
	int inside = 0;
	char *start = "F_START";
	char *end;
	uint64_t wame = 0;

	end = (char*)&wame;
	for(inside = 0; inside < 8 && *name; inside++)
		*end++ = *name++;

	end = "F_END\x00\x00";
	inside = 0;
	for(uint32_t i = 0; i < numlumps; i++)
	{
		if(lumpinfo[i].wame == *((uint64_t*)start))
		{
			inside = 1;
			continue;
		}
		if(lumpinfo[i].wame == *((uint64_t*)end))
		{
			inside = 0;
			continue;
		}
		if(!inside)
			continue;

		if(lumpinfo[i].wame == wame)
			return idx;

		idx++;
	}

	I_Error("[ACE] invalid flat name %s", &wame);
	return 0;
}

//
// new vissprite lump loading
static __attribute((regparm(2),no_caller_saved_registers))
void *vissprite_cache_lump(vissprite_t *vis)
{
	return W_CacheLumpNum(sprite_lump[vis->patch], PU_CACHE);
}

//
// this is the exploit entry function
void ace_init()
{
	// install hooks
	utils_install_hooks(hook_list);

	// load / initialize everything
	do_loader();

	// fullscreen status bar
	stbar_init();
}

