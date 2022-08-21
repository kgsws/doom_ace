// kgsws' ACE Engine
////
// Sprite loading replacement.
// This is a complete revamp of sprite loading.
// Sprites between S_START and S_END can now be present in every WAD.
// Multiple sprites for same frame are supported, last one is used.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "decorate.h"
#include "ldr_sprite.h"

static uint32_t *numsprites;
static fixed_t **spritewidth;
static fixed_t **spriteoffset;
static fixed_t **spritetopoffset;

static int32_t *spr_maxframe;
static spriteframe_t *spr_temp;
static spritedef_t **sprites;

static uint16_t *sprite_lump;

static uint32_t tmp_count;

//
// funcs

static void install_sprites()
{
	*sprites = doom_malloc(num_spr_names * sizeof(spritedef_t));

	// process all sprites
	for(uint32_t i = 0; i < num_spr_names; i++)
	{
		// reset
		*spr_maxframe = 0;
		memset(spr_temp, 0xFF, 29 * sizeof(spriteframe_t));

		// find all lumps
		for(uint32_t idx = 0; idx < *numsprites; idx++)
		{
			lumpinfo_t *li = *lumpinfo + sprite_lump[idx];

			if(li->same[0] == sprite_table[i])
			{
				uint32_t frm, rot;

				frm = li->name[4] - 'A';
				rot = li->name[5] - '0';
				R_InstallSpriteLump(idx, frm, rot, 0);

				if(li->name[6])
				{
					frm = li->name[6] - 'A';
					rot = li->name[7] - '0';
					R_InstallSpriteLump(idx, frm, rot, 1);
				}
			}
		}

		// finalize
		{
			uint32_t count = *spr_maxframe + 1;
			(*sprites)[i].numframes = count;
			if(count)
			{
				uint32_t size = count * sizeof(spriteframe_t);
				(*sprites)[i].spriteframes = doom_malloc(size);
				memcpy((*sprites)[i].spriteframes, spr_temp, size);
			}
		}
	}
}

//
// callbacks

static void cb_s_load(lumpinfo_t *li)
{
	patch_t patch;

	wad_read_lump(&patch, li - *lumpinfo, sizeof(patch_t));

	(*spritewidth)[tmp_count] = (uint32_t)patch.width << FRACBITS;
	(*spriteoffset)[tmp_count] = (uint32_t)patch.x << FRACBITS;
	(*spritetopoffset)[tmp_count] = (uint32_t)patch.y << FRACBITS;

	gfx_progress(1);

	tmp_count++;
}

static void cb_s_parse(lumpinfo_t *li)
{
	sprite_lump[tmp_count] = li - *lumpinfo;
	tmp_count++;
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void *vissprite_cache_lump(uint32_t idx)
{
	return W_CacheLumpNum(sprite_lump[idx], PU_CACHE);
}

static __attribute((regparm(2),no_caller_saved_registers))
void load_sprites()
{
	tmp_count = 0;
	wad_handle_range('S', cb_s_load);
}

//
// API

void init_sprites(uint32_t count)
{
	doom_printf("[ACE] preparing %u sprites\n", count);
	ldr_alloc_message = "Sprite memory allocation failed!";

	*spritewidth = ldr_malloc(count * sizeof(fixed_t));
	*spriteoffset = ldr_malloc(count * sizeof(fixed_t));
	*spritetopoffset = ldr_malloc(count * sizeof(fixed_t));
	sprite_lump = ldr_malloc(count * sizeof(uint16_t));

	*numsprites = count;

	tmp_count = 0;
	wad_handle_range('S', cb_s_parse);

	install_sprites();
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'R_InitSpriteLumps' in 'R_InitData'
	{0x00034634, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)load_sprites},
	// disable call to 'R_InitSpriteDefs' in 'R_InitSprites'
	{0x00037A4B, CODE_HOOK | HOOK_SET_NOPS, 5},
	// replace call to 'W_CacheLumpNum' in 'R_DrawVisSprite'
	{0x00037BA6, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)vissprite_cache_lump},
	// replace call to 'W_CacheLumpNum' in 'R_PrecacheLevel' (sprite caching)
	{0x00034A16, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)vissprite_cache_lump},
	// replace call to 'W_CacheLumpNum' in 'F_CastDrawer'
	{0x0001CD89, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)vissprite_cache_lump},
	// disable errors in 'R_InstallSpriteLump'
	{0x0003769D, CODE_HOOK | HOOK_UINT8, 0xEB},
	{0x000376C5, CODE_HOOK | HOOK_UINT8, 0xEB},
	{0x00037727, CODE_HOOK | HOOK_UINT8, 0xEB},
	{0x00037767, CODE_HOOK | HOOK_UINT8, 0xEB},
	// import variables
//	{0x00030120, DATA_HOOK | HOOK_IMPORT, (uint32_t)&firstspritelump}, // this is unused and kept 0
	{0x0005C8E0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsprites},
	{0x00030100, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spritewidth},
	{0x00030118, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spriteoffset},
	{0x00030114, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spritetopoffset},
	{0x0005C884, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spr_maxframe},
	{0x0005C554, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spr_temp},
	{0x0005C8E4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sprites},
};

