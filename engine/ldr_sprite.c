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
#include "mobj.h"
#include "ldr_sprite.h"

uint16_t *sprite_lump;
uint32_t sprite_tnt1;

static uint32_t tmp_count;

//
// funcs

static void install_sprites(uint32_t sprite_count)
{
	sprites = ldr_malloc(numsprites * sizeof(spritedef_t));

	// process all sprites
	for(uint32_t i = 0; i < numsprites; i++)
	{
		if(i == sprite_tnt1)
		{
			// ignore 'TNT1'
			sprites[i].numframes = 0;
			continue;
		}

		// reset
		spr_maxframe = -1;
		memset(sprtemp, 0xFF, 29 * sizeof(spriteframe_t));

		// find all lumps
		for(uint32_t idx = 0; idx < sprite_count; idx++)
		{
			lumpinfo_t *li = lumpinfo + sprite_lump[idx];

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
			uint32_t count = spr_maxframe + 1;
			sprites[i].numframes = count;
			if(count)
			{
				uint32_t size = count * sizeof(spriteframe_t);
				sprites[i].spriteframes = ldr_malloc(size);
				memcpy(sprites[i].spriteframes, sprtemp, size);
			}
		}
	}
}

//
// callbacks

static void cb_s_load(lumpinfo_t *li)
{
	patch_t patch;

	ldr_get_patch_header(li - lumpinfo, &patch);

	spritewidth[tmp_count] = (fixed_t)patch.width << FRACBITS;
	spriteoffset[tmp_count] = (fixed_t)patch.x << FRACBITS;
	spritetopoffset[tmp_count] = (fixed_t)patch.y << FRACBITS;

	gfx_progress(1);

	tmp_count++;
}

static void cb_s_parse(lumpinfo_t *li)
{
	sprite_lump[tmp_count] = li - lumpinfo;
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
void *precache_setup_sprites(uint8_t *buff)
{
	// optionally, it is possible to precache every sprite used in any state
	// but for now, let's do like Doom did

	memset(buff, 0, numsprites);

	for(thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		mobj_t *mo;

		if(th->function != (void*)P_MobjThinker)
			continue;

		mo = (mobj_t*)th;

		if(mo->sprite >= numsprites)
			continue;

		buff[mo->sprite] = 1;
	}
}

//
// API

void init_sprites(uint32_t count)
{
	doom_printf("[ACE] preparing %u sprites\n", count);
	ldr_alloc_message = "Sprite";

	spritewidth = ldr_malloc(count * sizeof(fixed_t));
	spriteoffset = ldr_malloc(count * sizeof(fixed_t));
	spritetopoffset = ldr_malloc(count * sizeof(fixed_t));
	sprite_lump = ldr_malloc(count * sizeof(uint16_t));

	tmp_count = 0;
	wad_handle_range('S', cb_s_parse);

	install_sprites(count);
}

__attribute((regparm(2),no_caller_saved_registers))
void spr_init_data()
{
	tmp_count = 0;
	wad_handle_range('S', cb_s_load);
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// disable call to 'R_InitSpriteDefs' in 'R_InitSprites'
	{0x00037A4B, CODE_HOOK | HOOK_SET_NOPS, 5},
	// replace call to 'W_CacheLumpNum' in 'R_DrawVisSprite'
	{0x00037BA6, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)vissprite_cache_lump},
	// replace call to 'W_CacheLumpNum' in 'R_PrecacheLevel' (sprite caching)
	{0x00034A16, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)vissprite_cache_lump},
	// replace call to 'W_CacheLumpNum' in 'F_CastDrawer'
	{0x0001CD89, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)vissprite_cache_lump},
	// replace call to 'memset' in 'R_PrecacheLevel'
	{0x00034976, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)precache_setup_sprites},
	{0x0003497B, CODE_HOOK | HOOK_UINT16, 0x27EB},
	// disable errors in 'R_InstallSpriteLump'
	{0x0003769D, CODE_HOOK | HOOK_UINT8, 0xEB},
	{0x000376C5, CODE_HOOK | HOOK_UINT8, 0xEB},
	{0x00037727, CODE_HOOK | HOOK_UINT8, 0xEB},
	{0x00037767, CODE_HOOK | HOOK_UINT8, 0xEB},
};

