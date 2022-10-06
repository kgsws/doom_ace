// kgsws' ACE Engine
////
// Subset of SNDINFO support.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "textpars.h"
#include "map.h"
#include "sound.h"

#define NUMSFX	109
#define NUMSFX_RNG	4
#define NUM_SFX_HOOKS	8
#define SFX_PRIORITY	666
#define RNG_RECURSION	8

#define S_CLIPPING_DIST	(1200 * FRACUNIT)
#define S_STEREO_SWING	(96 * FRACUNIT)
#define	S_CLOSE_DIST	(200 * FRACUNIT)
#define S_ATTENUATOR	((S_CLIPPING_DIST - S_CLOSE_DIST) >> FRACBITS)

typedef struct old_sfxinfo_s
{
	uint8_t *name;
	uint32_t single;
	uint32_t priority;
	struct old_sfxinfo_s *link;
	int32_t pitch;
	int32_t volume;
	void *data;
	int32_t usefulness;
	int32_t lumpnum;
} old_sfxinfo_t;

//

static old_sfxinfo_t *S_sfx;
musicinfo_t *S_music;

static int32_t *volume_val;

static musicinfo_t **music_now;

static uint32_t numsfx = NUMSFX + NUMSFX_RNG;
static sfxinfo_t *sfxinfo;

static hook_t sfx_hooks[NUM_SFX_HOOKS];

static const sfxinfo_t sfx_rng[NUMSFX_RNG] =
{
	{ // posit
		.priority = 98,
		.usefulness = -1,
		.rng_count = 3,
		.rng_id = {36, 37, 38},
		.lumpnum = -1
	},
	{ // bgsit
		.priority = 98,
		.usefulness = -1,
		.rng_count = 2,
		.rng_id = {39, 40},
		.lumpnum = -1
	},
	{ // podth
		.priority = 70,
		.usefulness = -1,
		.rng_count = 3,
		.rng_id = {59, 60, 61},
		.lumpnum = -1
	},
	{ // bgdth
		.priority = 70,
		.usefulness = -1,
		.rng_count = 2,
		.rng_id = {62, 63},
		.lumpnum = -1
	},
};

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t sound_start_check(void *mo, uint32_t idx)
{
	sfxinfo_t *sfx;
	uint32_t count = 0;

	// only 16 bits are valid
	idx &= 0xFFFF;

	if(!idx || idx >= numsfx)
		return 0;

	sfx = sfxinfo + idx;

	while(sfx->rng_count)
	{
		if(count > RNG_RECURSION)
			I_Error("[SNDINFO] Potentially recursive sound!");

		if(sfx->rng_count > 1)
			idx = P_Random() % sfx->rng_count;
		else
			idx = 0;

		idx = sfx->rng_id[idx];
		sfx = sfxinfo + idx;

		count++;
	}

	if(sfx->lumpnum < 0)
		return 0;

	return idx;
}

//
// funcs

static sfxinfo_t *sfx_find(uint64_t alias)
{
	for(uint32_t i = 0; i < numsfx; i++)
	{
		if(sfxinfo[i].alias == alias)
			return sfxinfo + i;
	}

	return NULL;
}

static sfxinfo_t *sfx_create(uint64_t alias)
{
	uint32_t last = numsfx;

	numsfx++;
	if(numsfx >= 0x10000)
		I_Error("[SNDINFO] So. Many. Sounds.");

	sfxinfo = ldr_realloc(sfxinfo, numsfx * sizeof(sfxinfo_t));

	sfxinfo[last].alias = alias;
	sfxinfo[last].priority = SFX_PRIORITY;
	sfxinfo[last].reserved = 0;
	sfxinfo[last].rng_count = 0;
	sfxinfo[last].data = NULL;
	sfxinfo[last].usefulness = -1;
	sfxinfo[last].lumpnum = -1;

	return sfxinfo + last;
}

static sfxinfo_t *sfx_get(uint8_t *name)
{
	sfxinfo_t *ret;
	uint64_t alias = tp_hash64(name);

	ret = sfx_find(alias);
	if(ret)
		return ret;

	return sfx_create(alias);
}

static void sfx_by_lump(uint8_t *name, int32_t lump)
{
	sfxinfo_t *sfx;
	uint64_t alias;

	alias = tp_hash64(name);

	// check for existing
	sfx = sfx_find(alias);
	if(!sfx)
	{
		// try to use empty default sounds
		if(lump >= 0)
		{
			for(uint32_t i = 0; i < NUMSFX; i++)
			{
				if(sfxinfo[i].alias)
					continue;

				if(sfxinfo[i].lumpnum == lump)
				{
					// found one; use it
					sfxinfo[i].alias = alias;
					sfxinfo[i].priority = SFX_PRIORITY;
					return;
				}
			}
		}

		// create a new one
		sfx = sfx_create(alias);
	}

	// sanity check
	if(sfx < sfxinfo + NUMSFX && sfx->lumpnum != lump)
		I_Error("[SNDINFO] Illegal replacement of '%.8s' by '%.8s' in '%s' detected!", (*lumpinfo)[sfx->lumpnum].name, (*lumpinfo)[lump].name, name);

	// fill info
	sfx->lumpnum = lump;
	sfx->rng_count = 0;
}

//
// callbacks

static void cb_sndinfo(lumpinfo_t *li)
{
	uint8_t *kw, *name;
	sfxinfo_t *sfx;
	int32_t lump;

	tp_load_lump(li);

	while(1)
	{
		kw = tp_get_keyword();
		if(!kw)
			return;

		if(kw[0] == '$')
		{
			// special directive
			if(!strcmp(kw + 1, "random"))
			{
				uint32_t count = 0;
				uint16_t id[5];

				// alias
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;

				name = kw;

				// block start
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;
				if(kw[0] != '{')
					I_Error("[SNDINFO] Expected '{', found '%s'!", kw);

				// get random sounds
				while(1)
				{
					// get sound alias
					kw = tp_get_keyword();
					if(!kw)
						goto error_end;
					if(kw[0] == '}')
					{
						// done
						break;
					}

					if(count >= 5)
						I_Error("[SNDINFO] Too many random sounds!");

					sfx = sfx_get(kw);
					id[count] = sfx - sfxinfo;
					count++;
				}

				// update existing or create new entry
				sfx = sfx_get(name);

				// sanity check
				if(sfx < sfxinfo + NUMSFX)
					I_Error("[SNDINFO] Illegal replacement of '%.8s' by random in '%s' detected!", (*lumpinfo)[sfx->lumpnum].name, name);

				// modify
				sfx->lumpnum = -1;
				sfx->rng_count = count;
				for(uint32_t i = 0; i < count; i++)
					sfx->rng_id[i] = id[i];

				continue;
			} else
			if(!strcmp(kw + 1, "limit"))
			{
				// ZDoom compatibility; just ignore this one
				// This MIGHT be supported in the future.
				tp_get_keyword(); // skip alias
				kw = tp_get_keyword(); // skip number
				if(!kw)
					goto error_end;
				continue;
			} else
			if(!strcmp(kw + 1, "playersound"))
			{
				uint8_t *pcl, *slt;
				uint8_t text[PLAYER_SOUND_CLASS_LEN + PLAYER_SOUND_SLOT_LEN];

				// sound class
				pcl = tp_get_keyword_lc();
				if(!pcl)
					goto error_end;
				if(strlen(pcl) >= PLAYER_SOUND_CLASS_LEN)
					I_Error("[SNDINFO] Too long string '%s'!", pcl);

				// gender
				kw = tp_get_keyword_lc();
				if(!kw)
					goto error_end;
				if(strcmp(kw, "other"))
					I_Error("[SNDINFO] Unsupported gender '%s'!", kw);

				// sound slot
				slt = tp_get_keyword_lc();
				if(!slt)
					goto error_end;
				if(strlen(slt) >= PLAYER_SOUND_SLOT_LEN)
					I_Error("[SNDINFO] Too long string '%s'!", slt);

				// sound lump
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;

				// get lump
				lump = wad_check_lump(kw);

				// slot name
				doom_sprintf(text, "%s#%s", pcl, slt);

				// update existing or create new entry
				sfx_by_lump(text, lump);

				continue;
			} else
				I_Error("[SNDINFO] Unsupported directive '%s'!", kw);
		}

		name = kw;

		// sound lump
		kw = tp_get_keyword();
		if(!kw)
			goto error_end;

		// get lump
		lump = wad_check_lump(kw);

		// update existing or create new entry
		sfx_by_lump(name, lump);
	}

error_end:
	I_Error("[SNDINFO] Incomplete definition!");
}

//
// API

void start_music(int32_t lump, uint32_t loop)
{
	// abuse original system to play any music lump
	// alternate betwee two different slots
	uint32_t idx;

	if(lump < 0)
	{
		S_StopMusic();
		return;
	}

	if(*music_now == S_music + 1)
		idx = 2;
	else
		idx = 1;

	S_music[idx].lumpnum = lump;
	S_ChangeMusic(idx, loop);
}

uint16_t sfx_by_alias(uint64_t alias)
{
	for(uint32_t i = 0; i < numsfx; i++)
	{
		if(sfxinfo[i].alias == alias)
			return i;
	}
	return 0;
}

uint16_t sfx_by_name(uint8_t *name)
{
	return sfx_by_alias(tp_hash64(name));
}

void sfx_rng_fix(uint16_t *idx, uint32_t pmatch)
{
	for(uint32_t i = 0; i < NUMSFX_RNG; i++)
	{
		const sfxinfo_t *sr = sfx_rng + i;

		if(sr->priority != pmatch)
			continue;

		for(uint32_t j = 0; j < sr->rng_count; j++)
		{
			if(sr->rng_id[j] == *idx)
			{
				*idx = NUMSFX + i;
				return;
			}
		}
	}
}

void init_sound()
{
	uint8_t temp[16];

	*((uint16_t*)temp) = 0x5344;

	doom_printf("[ACE] init sounds\n");
	ldr_alloc_message = "Sound info";

	// allocate memory for internal sounds
	sfxinfo = ldr_malloc((NUMSFX + NUMSFX_RNG) * sizeof(sfxinfo_t));

	// process internal sounds
	for(uint32_t i = 1; i < NUMSFX; i++)
	{
		uint8_t *name;

		if(S_sfx[i].link)
		{
			sfxinfo[i].alias = 0;
			sfxinfo[i].priority = S_sfx[i].priority;
			sfxinfo[i].reserved = 0;
			sfxinfo[i].rng_count = 1;
			sfxinfo[i].rng_id[0] = S_sfx[i].link - S_sfx;
			sfxinfo[i].data = NULL;
			sfxinfo[i].usefulness = -1;
			sfxinfo[i].lumpnum = -1;
			continue;
		}

		strncpy(temp + 2, S_sfx[i].name, 6);

		sfxinfo[i].alias = 0;
		sfxinfo[i].priority = S_sfx[i].priority;
		sfxinfo[i].reserved = 0;
		sfxinfo[i].rng_count = 0;
		sfxinfo[i].data = NULL;
		sfxinfo[i].usefulness = -1;
		sfxinfo[i].lumpnum = wad_check_lump(temp);
	}

	// add extra RNG sounds
	memcpy(sfxinfo + NUMSFX, sfx_rng, sizeof(sfx_rng));

	// process SNDINFO
	wad_handle_lump("SNDINFO", cb_sndinfo);

	// link sounds - make sure each lump is referenced only ONCE
	// orignal Doom sound cache would break otherwise
	for(uint32_t i = NUMSFX + NUMSFX_RNG; i < numsfx; i++)
	{
		sfxinfo_t *sfx = sfxinfo + i;

		if(sfx->lumpnum < 0)
			continue;

		// find this lump in previous sounds
		for(uint32_t j = i-1; j; j--)
		{
			if(sfx->lumpnum == sfxinfo[j].lumpnum)
			{
				// 'random' with count of one works as a redirect
				sfx->lumpnum = -1;
				sfx->rng_count = 1;
				sfx->rng_id[0] = j;
				break;
			}
		}
	}

	// patch CODE
	for(uint32_t i = 0; i < NUM_SFX_HOOKS; i++)
		sfx_hooks[i].value += (uint32_t)sfxinfo;
	utils_install_hooks(sfx_hooks, NUM_SFX_HOOKS);
}

//
// sound update

uint32_t sound_adjust(mobj_t *listener, mobj_t *source, int32_t *vol, int32_t *sep)
{
	fixed_t approx_dist;
	fixed_t adx;
	fixed_t ady;
	angle_t angle;
	void *end = *sectors + *numsectors;

	// check for sector sound
	// place sector sounds closest to the camera
	if((void*)source >= (void*)*sectors && (void*)source < end)
	{
		sector_t *sec;
		fixed_t tmp;

		// update sector sound
		sec = (void*)source - offsetof(sector_t, soundorg);

		// set source to player location
		source->x = listener->x;
		source->y = listener->y;

		// limit source to sector bounding box
		if(source->x < sec->extra->bbox[BOXLEFT])
			source->x = sec->extra->bbox[BOXLEFT];
		else
		if(source->x > sec->extra->bbox[BOXRIGHT])
			source->x = sec->extra->bbox[BOXRIGHT];

		if(source->y < sec->extra->bbox[BOXBOTTOM])
			source->y = sec->extra->bbox[BOXBOTTOM];
		else
		if(source->y > sec->extra->bbox[BOXTOP])
			source->y = sec->extra->bbox[BOXTOP];
	}

	// adjust volume
	adx = abs(listener->x - source->x);
	ady = abs(listener->y - source->y);

	approx_dist = adx + ady - ((adx < ady ? adx : ady) >> 1);

	if(!approx_dist)
	{
		*vol = *volume_val;
		*sep = 128;
		return 1;
	}

	if(approx_dist > S_CLIPPING_DIST)
		return 0;

	angle = R_PointToAngle2(listener->x, listener->y, source->x, source->y);
	if(angle > listener->angle)
		angle = angle - listener->angle;
	else
		angle = angle + (0xffffffff - listener->angle);
	angle >>= ANGLETOFINESHIFT;

	*sep = 128 - (FixedMul(S_STEREO_SWING,finesine[angle]) >> FRACBITS);

	if(approx_dist < S_CLOSE_DIST)
		*vol = *volume_val;
	else
		*vol = (*volume_val * ((S_CLIPPING_DIST - approx_dist) >> FRACBITS)) / S_ATTENUATOR;

	return (*vol > 0);
}

//
// hooks

static hook_t sfx_hooks[NUM_SFX_HOOKS] =
{
	// patch access to 'S_sfx'
	{0x0003F160, CODE_HOOK | HOOK_UINT32, 0},
	{0x0003F3C2, CODE_HOOK | HOOK_UINT32, 28},
	{0x0003F3D4, CODE_HOOK | HOOK_UINT32, 28},
	{0x0003F3DE, CODE_HOOK | HOOK_UINT32, 24},
	{0x0003F401, CODE_HOOK | HOOK_UINT32, 24},
	{0x0003F40C, CODE_HOOK | HOOK_UINT32, 32},
	{0x0003F41D, CODE_HOOK | HOOK_UINT32, 24},
	{0x0003F42A, CODE_HOOK | HOOK_UINT32, 24},
};

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x0001488C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&S_sfx},
	{0x0001444C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&S_music},
	{0x00014448, DATA_HOOK | HOOK_IMPORT, (uint32_t)&music_now},
	{0x00075CA0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&volume_val},
	// disable hardcoded sound randomization
	{0x00027716, CODE_HOOK | HOOK_UINT16, 0x41EB}, // A_Look
	{0x0002882B, CODE_HOOK | HOOK_UINT16, 0x47EB}, // A_Scream
	// replace calls to 'S_AdjustSoundParams'
	{0x0003F1F8, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_sound_adjust},
	{0x0003F4D9, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_sound_adjust},
	// custom sound ID check and translation
	// invalid sounds are skipped instead of causing error
	{0x0003F13E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)sound_start_check},
	{0x0003F143, CODE_HOOK | HOOK_UINT16, 0xC085},
	{0x0003F146, CODE_HOOK | HOOK_JMP_DOOM, 0x0003F365},
	{0x0003F145, CODE_HOOK | HOOK_UINT16, 0x840F},
	{0x0003F14B, CODE_HOOK | HOOK_UINT16, 0xC789},
	{0x0003F14D, CODE_HOOK | HOOK_SET_NOPS, 9},
	// disable sfx->link
	{0x0003F171, CODE_HOOK | HOOK_UINT8, 0xEB},
	{0x0003F493, CODE_HOOK | HOOK_UINT8, 0xEB},
};

