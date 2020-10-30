// kgsws' Doom ACE
// Main entry.
// All the hooks.
// Graphical loader.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "stbar.h"
#include "map.h"
#include "render.h"
#include "hitscan.h"

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
static uint32_t *detailshift;

// all the hooks for ACE engine
static hook_t hook_list[] =
{
/******************
	common
******************/
	{0x0002AE78, DATA_HOOK | HOOK_IMPORT, (uint32_t)&players},
	{0x00012D90, DATA_HOOK | HOOK_IMPORT, (uint32_t)&weaponinfo},
	{0x0001C3EC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mobjinfo},
	// TODO: states here
	//
	{0x00074FA0, DATA_HOOK | HOOK_READ32, (uint32_t)&numlumps},
	{0x00074FA4, DATA_HOOK | HOOK_READ32, (uint32_t)&lumpinfo},
	{0x00074F94, DATA_HOOK | HOOK_READ32, (uint32_t)&lumpcache},
	//
	{0x0002B698, DATA_HOOK | HOOK_IMPORT, (uint32_t)&screenblocks},
	{0x0002914C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&destscreen},
	{0x00074FC4, DATA_HOOK | HOOK_READ32, (uint32_t)&screens0},
	//
	{0x0002B3DC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&consoleplayer},
	{0x0002B3D8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&displayplayer},
	{0x0002B3E8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gamemap},
	{0x0002B3F8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gameepisode},
	{0x0002B3E0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gameskill},
	{0x0002B3FC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&deathmatch},
	{0x0002B400, DATA_HOOK | HOOK_IMPORT, (uint32_t)&netgame},
	{0x0002CF80, DATA_HOOK | HOOK_IMPORT, (uint32_t)&leveltime},
	{0x0002B3BC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gametic},
	{0x0002B3D0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalitems},
	{0x0002B3D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalkills},
	{0x0002B3C8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalsecret},
	//
	{0x00005A84, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finesine},
	{0x00032304, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewheight},
	{0x0003230C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewwidth},
	{0x00038FF8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&detailshift},
	{0x00038fe8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&setsizeneeded},
	{0x000322D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&ds_colormap},
	{0x000322D8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&dc_colormap},
	{0x000322B8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&dc_translation},
	{0x00038FFC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&fixedcolormap},
	{0x0005C550, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spritelights},
	{0x000300A4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&curline},
	{0x00039014, DATA_HOOK | HOOK_IMPORT, (uint32_t)&extralight},
	{0x00030104, DATA_HOOK | HOOK_IMPORT, (uint32_t)&colormaps},
	//
	{0x0002C134, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numlines},
	{0x0002C14C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsectors},
	{0x0002C120, DATA_HOOK | HOOK_IMPORT, (uint32_t)&lines},
	{0x0002C138, DATA_HOOK | HOOK_IMPORT, (uint32_t)&vertexes},
	{0x0002C118, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sides},
	{0x0002C148, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sectors},
	//
	{0x0002CF74, DATA_HOOK | HOOK_IMPORT, (uint32_t)&thinkercap},
	{0x00031490, CODE_HOOK | HOOK_IMPORT, (uint32_t)&ptr_MobjThinker},
	//
	{0x0002B9B0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&la_damage},
	// render function pointers // TODO: remove
	{0x00039010, DATA_HOOK | HOOK_IMPORT, (uint32_t)&colfunc},
	{0x0003900C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&basecolfunc},
	{0x00039008, DATA_HOOK | HOOK_IMPORT, (uint32_t)&fuzzcolfunc},
/*******************
	stuff
*******************/
	{0x00029134, DATA_HOOK | HOOK_IMPORT, (uint32_t)&grmode},
	{0x00075C94, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numChannels},
	{0x00075C98, DATA_HOOK | HOOK_IMPORT, (uint32_t)&channels},
/*******************
	textures
*******************/
	{0x000300e4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numtextures},
	{0x000300e0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&textures},
	{0x000300f4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturecolumnlump},
	{0x000300d0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturecolumnofs},
	{0x000300e8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturecomposite},
	{0x000300d4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturecompositesize},
	{0x000300d8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturewidthmask},
	{0x00030124, DATA_HOOK | HOOK_IMPORT, (uint32_t)&textureheight},
	{0x0003010c, DATA_HOOK | HOOK_IMPORT, (uint32_t)&texturetranslation},
/*******************
	sprites
*******************/
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
/*******************
	flats
*******************/
	{0x000300DC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numflats},
	{0x000300FC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&firstflat},
	{0x00030110, DATA_HOOK | HOOK_IMPORT, (uint32_t)&flattranslation},
	{0x00034690, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)custom_R_FlatNumForName},
/*******************
	stbar.c
*******************/
	{0x000752f0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallnum},
	{0x00075458, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tallpercent},
/*******************
	map.c
*******************/
	// replace call to W_GetNumForName in P_SetupLevel
	{0x0002e8c1, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)map_get_map_lump},
	// change mobj_t size - add extra space for new stuff
	{0x00031552, CODE_HOOK | HOOK_UINT32, sizeof(mobj_t)}, // for Z_Malloc
	{0x00031563, CODE_HOOK | HOOK_UINT32, sizeof(mobj_t)}, // for memset
/*******************
	hitscan.c
*******************/
	// custom mobj hit handling
	{0x0002bb30, CODE_HOOK | HOOK_UINT32, 0x24148b}, // 'mov (%esp),%edx'
	{0x0002BB33, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hitscan_HitMobj},
	{0x0002bb38, CODE_HOOK | HOOK_UINT32, 0x18EB}, // 'jmp'
/*******************
	render.c
*******************/
	// hook 'R_ExecuteSetViewSize' to fix mouselook when screen size changes
	{0x0001d1ff, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)render_SetViewSize},
	// disable 'colormaps' offset in 'scalelight' so any colormap can be used
	{0x00035d34, CODE_HOOK | HOOK_UINT16, 0x9090},
	// setup 'ds_colormap' in 'R_MapPlane'
	{0x00036189, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_planeColormap},
	// setup 'dc_colormap' in 'R_RenderSegLoop'
	{0x00036ac4, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_segColormap},
	// setup 'dc_colormap' in 'R_RenderMaskedSegRange'
	{0x000368e4, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_maskedColormap},
	// modify end of 'R_ProjectSprite' for custom colormaps
	{0x00037f31, CODE_HOOK | HOOK_UINT32, 0xda89f089}, // 'mov %esi,%eax' 'mov %ebx,%edx'
	{0x00037f35, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_spriteColormap},
	{0x00037f3a, CODE_HOOK | HOOK_UINT16, 0x11EB}, // 'jmp'
	// modify end of 'R_DrawPSprite' for custom colormaps
	{0x000381b2, CODE_HOOK | HOOK_UINT16, 0x0E8B}, // 'mov (%esi),%ecx'
	{0x000381b4, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_pspColormap},
	{0x000381b9, CODE_HOOK | HOOK_UINT16, 0x3DEB}, // 'jmp'
	// modify 'R_DrawVisSprite' for custom render functions
	{0x00037bb5, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_spriteColfunc},
	{0x00037bba, CODE_HOOK | HOOK_UINT16, 0x39EB}, // 'jmp'
	// custom light level handling for 'walls'
	{0x000372d8, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_segLight},
	{0x000372dd, CODE_HOOK | HOOK_UINT16, 0x10EB}, // 'jmp'
	// custom light level handling for 'mid walls'
	{0x0003676e, CODE_HOOK | HOOK_UINT8, 0x50}, // 'push %eax'
	{0x0003676f, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_maskedLight},
	{0x00036774, CODE_HOOK | HOOK_UINT32, 0xEB58c289}, // 'mov %eax,%edx' 'pop %eax' 'jmp'
	{0x00036778, CODE_HOOK | HOOK_UINT8, 0x18}, // jmp offset
	// custom light level handling for 'sprites'
	{0x00037fa2, CODE_HOOK | HOOK_UINT8, 0x50}, // 'push %eax'
	{0x00037fa3, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_spriteLight},
	{0x00037fa8, CODE_HOOK | HOOK_UINT32, 0x58c289}, // 'mov %eax,%edx' 'pop %eax'
	{0x00037fab, CODE_HOOK | HOOK_UINT16, 0x9090}, // 'nop's
	// change lightlevel in sector to take only 8bit values
	{0x00038221, CODE_HOOK | HOOK_UINT32, 0x0C52B60F}, // 'movzbl 0xc(%edx),%edx' // player sprites
	// plane light level hook
	{0x00036654, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_planeLight},
	// sky colormap
	{0x00036584, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)render_skyColormap},
	// disable player translation mobj flags // TODO: apply new translation
	{0x00031859, CODE_HOOK | HOOK_UINT8, 0xEB}, // 'jmp'
/*******************
	enhancements
*******************/
	// replace unknown texture error with "no texture"
	{0x0003473d, CODE_HOOK | HOOK_UINT32, 1},
	// disable 'colormaps' in 'R_InitLightTables'
	{0x00035a10, CODE_HOOK | HOOK_UINT32, 0x01EBC031}, // 'xor %eax,%eax' 'jmp'
	// disable "store demo" mode check
	{0x0001d113, CODE_HOOK | HOOK_UINT16, 0x4CEB}, // 'jmp'
	// invert 'run key' function (auto run)
	{0x0001fbc5, CODE_HOOK | HOOK_UINT8, 0x01},
	// custom overlay stuff
	{0x0001d362, CODE_HOOK | HOOK_RELADDR_ACE, (uint32_t)custom_RenderPlayerView},
	// disable sprite table errors
	{0x0003769d, CODE_HOOK | HOOK_UINT8, 0xEB}, // 'jmp'
	{0x000376c5, CODE_HOOK | HOOK_UINT8, 0xEB}, // 'jmp'
	{0x00037727, CODE_HOOK | HOOK_UINT8, 0xEB}, // 'jmp'
	{0x00037767, CODE_HOOK | HOOK_UINT8, 0xEB}, // 'jmp'
	// make invalid sprites invisible
	{0x00037d4c, CODE_HOOK | HOOK_JMP_DOOM, 0x00037f86}, // invalid sprite
	{0x00037d7a, CODE_HOOK | HOOK_JMP_DOOM, 0x00037f86}, // invalid sprite frame
	{0x00038029, CODE_HOOK | HOOK_JMP_DOOM, 0x00038203}, // invalid Psprite
	{0x00038059, CODE_HOOK | HOOK_JMP_DOOM, 0x00038203}, // invalid Psprite frame
	// DEBUG STUFF TO BE REMOVED OR FIXED
	{0x00034780, CODE_HOOK | HOOK_UINT8, 0xC3}, // disable 'R_PrecacheLevel'; TODO: fix
	{0x0002fc8b, CODE_HOOK | HOOK_UINT8, 0xEB}, // disable animations; TODO: rewrite 'P_UpdateSpecials'
	// terminator
	{0}
};

//
// imported variables
player_t *players;
weaponinfo_t *weaponinfo;
mobjinfo_t *mobjinfo;

uint32_t numlumps;
lumpinfo_t *lumpinfo;
void **lumpcache;

uint32_t *screenblocks;
uint8_t **destscreen;
uint8_t *screens0;

uint32_t *consoleplayer;
uint32_t *displayplayer;

uint32_t *gamemap;
uint32_t *gameepisode;
uint32_t *gameskill;
uint32_t *netgame;
uint32_t *deathmatch;
uint32_t *gametic;
uint32_t *leveltime;
uint32_t *totalitems;
uint32_t *totalkills;
uint32_t *totalsecret;

fixed_t *finesine;
fixed_t *finecosine;
uint32_t *viewheight;
uint32_t *viewwidth;

thinker_t *thinkercap;
void *ptr_MobjThinker;

// progress bar info
static uint32_t pbar_step;
static patch_t *pbar_patch;

// markers
static uint64_t markers[] =
{
	0x0054524154535fFF, // ?_START
	0x000000444e455fFF, // ?_END
	0x54524154535fFFFF, // ??_START
	0x0000444e455fFFFF, // ??_END
};

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
static int section_count_lumps(uint8_t s, uint16_t d)
{
	int lump = numlumps - 1;
	int inside = 0;
	int count = 0;

	// edit markers
	((uint8_t*)markers)[0] = s;
	((uint8_t*)markers)[8] = s;
	((uint16_t*)markers)[8] = d;
	((uint16_t*)markers)[12] = d;

	// go backwards trough all the lumps
	while(lump >= 0)
	{
		if(lumpinfo[lump].wame == markers[0] || lumpinfo[lump].wame == markers[2])
		{
			if(!inside)
				// unclosed section
				I_Error("[ACE] x_START without x_END");
			// close this section
			inside = 0;
		} else
		if(lumpinfo[lump].wame == markers[1] || lumpinfo[lump].wame == markers[3])
		{
			if(inside)
				// unclosed section
				I_Error("[ACE] x_END without x_START");
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
		I_Error("[ACE] x_END without x_START");

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
// TX raw patch loader
static void tx_from_patch(uint32_t idx, uint32_t lmp)
{
	patch_t header;
	texture_t *tex;

	tex = Z_Malloc(sizeof(texture_t) + sizeof(texpatch_t), PU_STATIC, NULL);
	(*textures)[idx] = tex;

	// must read patch header here
	// but do not cache it, yet
	lseek(lumpinfo[lmp].handle, lumpinfo[lmp].position, 0);
	read(lumpinfo[lmp].handle, &header, sizeof(header));

	tex->width = header.width;
	tex->height = header.height;
	tex->count = 1;
	tex->wame = lumpinfo[lmp].wame;
	tex->patch[0].x = 0;
	tex->patch[0].y = 0;
	tex->patch[0].p = lmp;

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
	Z_Free(lumpcache[lmp]);

	// animation stuff
	(*texturetranslation)[idx] = idx;
}

//
// TX section loader
static uint32_t tx_process(uint32_t idx, uint8_t s, uint16_t d)
{
	int inside = 0;

	// edit markers
	((uint8_t*)markers)[0] = s;
	((uint8_t*)markers)[8] = s;
	((uint16_t*)markers)[8] = d;
	((uint16_t*)markers)[12] = d;

	for(uint32_t i = 0; i < numlumps; i++)
	{
		if(lumpinfo[i].wame == markers[0] || lumpinfo[i].wame == markers[2])
		{
			inside = 1;
			continue;
		}
		if(lumpinfo[i].wame == markers[1] || lumpinfo[i].wame == markers[3])
		{
			inside = 0;
			continue;
		}
		if(!inside)
			continue;

		tx_from_patch(idx, i);

		idx++;
		// update progress
		pbar_set(idx);
	}

	return idx;
}

//
// sprite section loader
int sprite_process(int idx, uint8_t s, uint16_t d)
{
	int sidx = 0;
	patch_t header;
	int inside = 0;

	// edit markers
	((uint8_t*)markers)[0] = s;
	((uint8_t*)markers)[8] = s;
	((uint16_t*)markers)[8] = d;
	((uint16_t*)markers)[12] = d;

	for(uint32_t i = 0; i < numlumps; i++)
	{
		if(lumpinfo[i].wame == markers[0] || lumpinfo[i].wame == markers[2])
		{
			inside = 1;
			continue;
		}
		if(lumpinfo[i].wame == markers[1] || lumpinfo[i].wame == markers[3])
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
void flat_translation(uint8_t s, uint16_t d)
{
	int idx = 1; // skip "no texture"
	int inside = 0;

	// edit markers
	((uint8_t*)markers)[0] = s;
	((uint8_t*)markers)[8] = s;
	((uint16_t*)markers)[8] = d;
	((uint16_t*)markers)[12] = d;

	(*flattranslation)[0] = W_GetNumForName("\xCCNOFLAT"); // "no texture"

	for(uint32_t i = 0; i < numlumps; i++)
	{
		if(lumpinfo[i].wame == markers[0] || lumpinfo[i].wame == markers[2])
		{
			inside = 1;
			continue;
		}
		if(lumpinfo[i].wame == markers[1] || lumpinfo[i].wame == markers[3])
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
void sprite_table_loop(uint32_t match, uint8_t s, uint16_t d)
{
	int sidx = 0;
	int inside = 0;

	// edit markers
	((uint8_t*)markers)[0] = s;
	((uint8_t*)markers)[8] = s;
	((uint16_t*)markers)[8] = d;
	((uint16_t*)markers)[12] = d;

	for(uint32_t i = 0; i < numlumps; i++)
	{
		if(lumpinfo[i].wame == markers[0] || lumpinfo[i].wame == markers[2])
		{
			inside = 1;
			continue;
		}
		if(lumpinfo[i].wame == markers[1] || lumpinfo[i].wame == markers[3])
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

		sprite_table_loop(match, 0x53, 0x5353);
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
	tex_count = section_count_lumps(0, 0x5854);

	// count TEXTUREx
	tex_count += texture_read("TEXTURE2");
	tmp = texture_read("TEXTURE1");
	tex_count += tmp;
	tex_count += 2; // hardcoded "-" and "no texture"

	// count all the sprites in each S_START-S_END section
	tmpA = section_count_lumps(0x53, 0x5353);

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

	// add "no texture"
	tmpA = W_GetNumForName("\xCCNOTEX");
	tx_from_patch(0, tmpA); // TODO: maybe use less memory
	tx_from_patch(1, tmpA);

	// process TX sections
	idx = tx_process(2, 0, 0x5854);
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
	idx = sprite_process(idx, 0x53, 0x5353);

	// count all the flats in each F_START-F_END section
	tmp = section_count_lumps(0x46, 0x4646);
	*numflats = tmp + 1; // count "no flat" too
	*firstflat = 0;

	// generate translation table
	*flattranslation = Z_Malloc((tmp+1) * 4, PU_STATIC, NULL); // TODO: why +1 ?
	flat_translation(0x46, 0x4646);

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
	int idx = *numflats - 1;
	int inside;
	char *ptr;
	uint64_t wame = 0;

	// edit markers 'F'
	((uint8_t*)markers)[0] = 0x46;
	((uint8_t*)markers)[8] = 0x46;
	((uint16_t*)markers)[8] = 0x4646;
	((uint16_t*)markers)[12] = 0x4646;

	// convert name to u64, uppercase
	ptr = (char*)&wame;
	for(inside = 0; inside < 8 && *name; inside++)
	{
		uint8_t in = *name++;
		if(in >= 'a' && in <= 'z')
			in &= 0b11011111;
		*ptr++ = in;
	}

	inside = 0;
	for(uint32_t i = numlumps-1; i; i--) // go from last to first
	{
		if(lumpinfo[i].wame == markers[0] || lumpinfo[i].wame == markers[2])
		{
			inside = 0;
			continue;
		}
		if(lumpinfo[i].wame == markers[1] || lumpinfo[i].wame == markers[3])
		{
			inside = 1;
			continue;
		}
		if(!inside)
			continue;

		if(lumpinfo[i].wame == wame)
			return idx;

		idx--;
	}

//	I_Error("[ACE] invalid flat name %s", &wame); // replaced by "no texture"
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

	// update
	finecosine = finesine + (FINEANGLES / 4);

	// TODO: disable low detail mode
	*detailshift = 0;

	// load / initialize everything
	do_loader();

	// fullscreen status bar
	stbar_init();

	// new map format
	map_init();

	// rendering changes
	render_init();
}

