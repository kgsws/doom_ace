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

#define NUMSFX_RNG	4
#define NUM_SFX_HOOKS	8
#define SFX_PRIORITY	666
#define RNG_RECURSION	8

#define S_CLIPPING_DIST	(1200 * FRACUNIT)
#define S_STEREO_SWING	(96 * FRACUNIT)
#define	S_CLOSE_DIST	(200 * FRACUNIT)
#define S_ATTENUATOR	((S_CLIPPING_DIST - S_CLOSE_DIST) >> FRACBITS)

typedef struct
{
	uint64_t alias;
	const uint8_t *lump;
} new_sfx_t;

//

static uint32_t numsfx = NEW_NUMSFX + NUMSFX_RNG;
static sfxinfo_t *sfxinfo;

static sound_seq_t *sndseq;
static uint32_t numsndseq = NUMSNDSEQ;

static hook_t sfx_hooks[NUM_SFX_HOOKS];

static const new_sfx_t new_sfx[NEW_NUMSFX - NUMSFX] =
{
	[SFX_QUAKE - NUMSFX] =
	{
		.alias = 0x5AE1D71BE4B32BF5,
		.lump = "dsquake",
	},
	[SFX_SECRET - NUMSFX] =
	{
		.alias = 0x49728E5CEF8F3A6E,
		.lump = "dssecret",
	},
	[SFX_FREEZE - NUMSFX] =
	{
		.alias = 0x5EA59729AF8F3A6F,
		.lump = "icedth1",
	},
	[SFX_ICEBREAK - NUMSFX] =
	{
		.alias = 0x5CA2963A6F8F11EB,
		.lump = "icebrk1a",
	},
};

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

static const sound_seq_t def_sndseq[NUMSNDSEQ] =
{
	[SNDSEQ_DOOR] =
	{
		.norm_open.start = 20,
		.norm_open.repeat = SSQ_NO_STOP,
		.norm_close.start = 21,
		.norm_close.repeat = SSQ_NO_STOP,
		.fast_open.start = 88,
		.fast_open.repeat = SSQ_NO_STOP,
		.fast_close.start = 89,
		.fast_close.repeat = SSQ_NO_STOP,
	},
	[SNDSEQ_PLAT] =
	{
		.norm_open.start = 18,
		.norm_open.stop = 19,
		.norm_close.start = 18,
		.norm_close.stop = 19,
		.fast_open.start = 18,
		.fast_open.stop = 19,
		.fast_close.start = 18,
		.fast_close.stop = 19,
	},
	[SNDSEQ_CEILING] =
	{
		.norm_open.move = 22,
		.norm_open.repeat = 8,
		.norm_close.move = 22,
		.norm_close.repeat = 8,
		.fast_open.move = 22,
		.fast_open.repeat = 8,
		.fast_close.move = 22,
		.fast_close.repeat = 8,
	},
	[SNDSEQ_FLOOR] =
	{
		.norm_open.move = 22,
		.norm_open.stop = 19,
		.norm_open.repeat = 8,
		.norm_close.move = 22,
		.norm_close.stop = 19,
		.norm_close.repeat = 8,
		.fast_open.move = 22,
		.fast_open.stop = 19,
		.fast_open.repeat = 8,
		.fast_close.move = 22,
		.fast_close.stop = 19,
		.fast_close.repeat = 8,
	},
};

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t sound_start_check(void **mo, uint32_t idx)
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
			engine_error("SNDINFO", "Potentially recursive sound!");

		if(sfx->rng_count > 1)
			idx = P_Random() % sfx->rng_count;
		else
			idx = 0;

		idx = sfx->rng_id[idx];
		sfx = sfxinfo + idx;

		count++;
	}

	if(snd_sfxdevice != 3)
		// sound is disabled; but RNG has to be processed
		return 0;

	if(sfx->lumpnum < 0)
		return 0;

	if(*mo)
	{
		// check for consoleplayer sound
		count = (uint32_t)*mo;
		if(count > 0 && count <= MAXPLAYERS)
		{
			if(consoleplayer != count - 1)
				return 0;
			// play this sound only for local player
			// this has to be filtered after possible 'P_Random' call
			*mo = sfxinfo + idx;
		}
	} else
		// each full volume sound has its own slot
		*mo = sfxinfo + idx;

	return idx;
}

static __attribute((regparm(2),no_caller_saved_registers))
void start_weapon_sound(mobj_t *mo, uint32_t idx)
{
	S_StartSound(SOUND_CHAN_WEAPON(mo), idx);
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
	if(numsfx >= 0x8000) // what is a reasonable limit?
		engine_error("SNDINFO", "So. Many. Sounds.");

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
		engine_error("SNDINFO", "Illegal replacement of '%.8s' by '%.8s' in '%s' detected!", lumpinfo[sfx->lumpnum].name, lumpinfo[lump].name, name);

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
					engine_error("SNDINFO", "Expected '%c' found '%s'!", '{', kw);

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
						engine_error("SNDINFO", "Too many random sounds!");

					sfx = sfx_get(kw);
					id[count] = sfx - sfxinfo;
					count++;
				}

				// update existing or create new entry
				sfx = sfx_get(name);

				// sanity check
				if(sfx < sfxinfo + NUMSFX)
					engine_error("SNDINFO", "Illegal replacement of '%.8s' by random in '%s' detected!", lumpinfo[sfx->lumpnum].name, name);

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
					engine_error("SNDINFO", "Too long string '%s'!", pcl);

				// gender
				kw = tp_get_keyword_lc();
				if(!kw)
					goto error_end;
				if(strcmp(kw, "other"))
					engine_error("SNDINFO", "Unsupported gender '%s'!", kw);

				// sound slot
				slt = tp_get_keyword_lc();
				if(!slt)
					goto error_end;
				if(strlen(slt) >= PLAYER_SOUND_SLOT_LEN)
					engine_error("SNDINFO", "Too long string '%s'!", slt);

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
				engine_error("SNDINFO", "Unsupported directive '%s'!", kw);
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
	engine_error("SNDINFO", "Incomplete definition!");
}

static uint32_t get_sfx_length(int32_t idx)
{
	sfxinfo_t *sfx;
	struct
	{
		uint16_t format;
		uint16_t samplerate;
		uint32_t samplecount;
	} head;

	if(idx <= 0)
		return 0;

	sfx = sfxinfo + idx;

	if(sfx->lumpnum < 0)
		return 0;

	wad_read_lump(&head, sfx->lumpnum, sizeof(head));

	if(head.format != 3)
		return 0;

	if(head.samplecount < 32)
		return 0;

	head.samplecount -= 32;
	head.samplecount *= 35;
	head.samplecount /= head.samplerate;

	if(head.samplecount)
		head.samplecount--;

	return head.samplecount;
}

static void cb_sndseq(lumpinfo_t *li)
{
	uint8_t *kw, *wk, *name;
	sound_seq_t *seq;
	uint32_t value;
	uint16_t do_stop;

	tp_load_lump(li);

	while(1)
	{
		kw = tp_get_keyword();
		if(!kw)
			return;

		if(kw[0] == '[')
		{
			name = tp_get_keyword();
			if(!name)
				goto error_end;

			numsndseq++;
			sndseq = ldr_realloc(sndseq, numsndseq * sizeof(sound_seq_t));
			seq = sndseq + numsndseq - 1;
			memset(seq, 0, sizeof(sound_seq_t));
			seq->alias = tp_hash64(name);
			seq->number = 256;

			while(1)
			{
				wk = tp_get_keyword();
				if(!wk)
					goto error_end;

				if(wk[0] == ']')
					break;

				kw = tp_get_keyword();
				if(!kw)
					goto error_end;

				if(!strcmp(wk, "door"))
				{
					if(doom_sscanf(kw, "%u", &value) != 1 || value >= 255)
						goto error_value;
					seq->number = value | SEQ_IS_DOOR;
				} else
				if(!strcmp(wk, "platform"))
				{
					if(doom_sscanf(kw, "%u", &value) != 1 || value >= 255)
						goto error_value;
					seq->number = value;
				} else
				{
					sound_seq_t *sss;
					seq_sounds_t *dst;

					if(doom_sscanf(wk, "%u", &value) != 1 || value > 3)
					{
						kw = wk;
						goto error_value;
					}

					switch(value)
					{
						case 0:
							dst = &seq->norm_open;
						break;
						case 1:
							dst = &seq->norm_close;
						break;
						case 2:
							dst = &seq->fast_open;
						break;
						default:
							dst = &seq->fast_close;
						break;
					}

					sss = snd_seq_by_name(kw);
					if(!sss)
						goto error_value;

					*dst = sss->norm_open;
				}
			}
			continue;
		}

		if(kw[0] != ':')
			engine_error("SNDSEQ", "Unexpected '%s'!\n", kw);

		name = tp_get_keyword();
		if(!name)
			goto error_end;

		numsndseq++;
		sndseq = ldr_realloc(sndseq, numsndseq * sizeof(sound_seq_t));
		seq = sndseq + numsndseq - 1;
		memset(seq, 0, sizeof(sound_seq_t));
		seq->alias = tp_hash64(name);
		seq->number = 256;
		do_stop = 0;

		while(1)
		{
			wk = tp_get_keyword_lc();
			if(!wk)
				goto error_end;

			if(!strcmp(wk, "end"))
			{
				seq->norm_open.repeat |= do_stop;
				seq->norm_close = seq->norm_open;
				seq->fast_open = seq->norm_open;
				seq->fast_close = seq->norm_open;
				break;
			}

			kw = tp_get_keyword();
			if(!kw)
				goto error_end;

			if(!strcmp(wk, "door"))
			{
				if(doom_sscanf(kw, "%u", &value) != 1 || value >= 255)
					goto error_value;
				seq->number = value | SEQ_IS_DOOR;
			} else
			if(!strcmp(wk, "platform"))
			{
				if(doom_sscanf(kw, "%u", &value) != 1 || value >= 255)
					goto error_value;
				seq->number = value;
			} else
			if(!strcmp(wk, "playuntildone"))
			{
				seq->norm_open.start = sfx_by_alias(tp_hash64(kw));
				seq->norm_open.delay = get_sfx_length(seq->norm_open.start);
			} else
			if(!strcmp(wk, "playtime"))
			{
				seq->norm_open.start = sfx_by_alias(tp_hash64(kw));
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;
				if(doom_sscanf(kw, "%u", &value) != 1 || !value || value > 0xFFFF)
					goto error_value;
				seq->norm_open.delay = value;
			} else
			if(!strcmp(wk, "playloop"))
			{
				seq->norm_open.move = sfx_by_alias(tp_hash64(kw));
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;
				if(doom_sscanf(kw, "%u", &value) != 1 || !value || value > 0x0FFF)
					goto error_value;
				seq->norm_open.repeat = value;
			} else
			if(!strcmp(wk, "playrepeat"))
			{
				seq->norm_open.move = sfx_by_alias(tp_hash64(kw));
				seq->norm_open.repeat = get_sfx_length(seq->norm_open.move);
			} else
			if(!strcmp(wk, "stopsound"))
			{
				seq->norm_open.stop = sfx_by_alias(tp_hash64(kw));
			} else
			if(!strcmp(wk, "nostopcutoff"))
			{
				do_stop = SSQ_NO_STOP;
				tp_push_keyword(kw);
			} else
				engine_error("SNDSEQ", "Unknown keyword '%s' in '%s'!", wk, name);
		}
	}

error_value:
	engine_error("SNDSEQ", "Invalid value '%s' in '%s'!", kw, name);
error_end:
	engine_error("SNDSEQ", "Incomplete definition!");
}

//
// API

void S_StopSound(mobj_t *mo)
{
	// CHAN_VOICE
	doom_S_StopSound(mo);
	// CHAN_BODY
	doom_S_StopSound(SOUND_CHAN_BODY(mo));
	// CHAN_WEAPON
	doom_S_StopSound(SOUND_CHAN_WEAPON(mo));
}

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

	if(music_now == S_music + 1)
		idx = 2;
	else
		idx = 1;

	S_music[idx].lumpnum = lump;
	S_ChangeMusic(idx, loop);
}

uint16_t sfx_by_alias(uint64_t alias)
{
	if(!alias)
		return 0;
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
				*idx = NEW_NUMSFX + i;
				return;
			}
		}
	}
}

sound_seq_t *snd_seq_by_sector(sector_t *sec, uint32_t def_type)
{
	uint16_t magic;

	if(sec->sndseq == 0xFF)
		return sndseq + def_type;

	magic = sec->sndseq;
	if(def_type == SNDSEQ_DOOR)
		magic |= SEQ_IS_DOOR;

	for(uint32_t i = numsndseq - 1; i >= NUMSNDSEQ; i--)
	{
		if(sndseq[i].number == magic)
			return sndseq + i;
	}

	return NULL;
}

sound_seq_t *snd_seq_by_id(uint32_t id)
{
	for(uint32_t i = numsndseq - 1; i >= NUMSNDSEQ; i--)
	{
		if(sndseq[i].number == id)
			return sndseq + i;
	}

	return NULL;
}

sound_seq_t *snd_seq_by_name(const uint8_t *name)
{
	uint64_t alias;

	alias = tp_hash64(name);

	for(uint32_t i = numsndseq - 1; i >= NUMSNDSEQ; i--)
	{
		if(sndseq[i].alias == alias)
			return sndseq + i;
	}

	return NULL;
}

void init_sound()
{
	uint8_t temp[16];

	*((uint16_t*)temp) = 0x5344;

	doom_printf("[ACE] init sounds\n");
	ldr_alloc_message = "Sound info";

	// allocate memory for internal sounds
	sfxinfo = ldr_malloc((NEW_NUMSFX + NUMSFX_RNG) * sizeof(sfxinfo_t));

	// process original sounds
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

	// add new sounds
	for(uint32_t i = NUMSFX; i < NEW_NUMSFX; i++)
	{
		sfxinfo[i].alias = new_sfx[i - NUMSFX].alias;
		sfxinfo[i].priority = SFX_PRIORITY;
		sfxinfo[i].lumpnum = wad_check_lump(new_sfx[i - NUMSFX].lump);
		sfxinfo[i].usefulness = -1;
		sfxinfo[i].rng_count = 0;
		sfxinfo[i].data = NULL;
	}

	// add extra RNG sounds
	memcpy(sfxinfo + NEW_NUMSFX, sfx_rng, sizeof(sfx_rng));

	// process SNDINFO
	wad_handle_lump("SNDINFO", cb_sndinfo);

	// link sounds - make sure each lump is referenced only ONCE
	// orignal Doom sound cache would break otherwise
	for(uint32_t i = NEW_NUMSFX + NUMSFX_RNG; i < numsfx; i++)
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

	// allocate memory for internal sounds
	sndseq = ldr_malloc(NUMSNDSEQ * sizeof(sound_seq_t));
	memcpy(sndseq, def_sndseq, sizeof(def_sndseq));

	// process SNDSEQ
	wad_handle_lump("SNDSEQ", cb_sndseq);

	// patch CODE
	for(uint32_t i = 0; i < NUM_SFX_HOOKS; i++)
		sfx_hooks[i].value += (uint32_t)sfxinfo;
	utils_install_hooks(sfx_hooks, NUM_SFX_HOOKS);

	 *((uint32_t*)((void*)0x0003F433 + doom_code_segment)) = numsfx * sizeof(sfxinfo_t);
}

//
// sound update

uint32_t sound_adjust(mobj_t *listener, mobj_t *source, int32_t *vol, int32_t *sep)
{
	fixed_t approx_dist;
	fixed_t adx;
	fixed_t ady;
	angle_t angle;

	if(!listener)
		goto full_volume;

	// check for full volume sound
	if((void*)source >= (void*)sfxinfo && (void*)source < (void*)(sfxinfo + numsfx))
		goto full_volume;

	// check for sector sound
	// place sector sounds closest to the camera
	if((void*)source >= (void*)sectors && (void*)source < (void*)(sectors + numsectors))
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
		goto full_volume;

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
		*vol = volume_val;
	else
		*vol = (volume_val * ((S_CLIPPING_DIST - approx_dist) >> FRACBITS)) / S_ATTENUATOR;

	return (*vol > 0);

full_volume:
	*vol = volume_val;
	*sep = 128;
	return 1;
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
	// disable hardcoded sound randomization
	{0x00027716, CODE_HOOK | HOOK_UINT16, 0x41EB}, // A_Look
	{0x0002882B, CODE_HOOK | HOOK_UINT16, 0x47EB}, // A_Scream
	// replace calls to 'S_AdjustSoundParams'
	{0x0003F1F8, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_sound_adjust},
	{0x0003F4D9, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_sound_adjust},
	// sound listener is player->camera
	{0x0001D5FA, CODE_HOOK | HOOK_UINT32, (uint32_t)&players->camera},
	{0x0003F1F2, CODE_HOOK | HOOK_UINT32, (uint32_t)&players->camera},
	// custom sound ID check and translation
	// invalid sounds are skipped instead of causing error
	{0x0003F13C, CODE_HOOK | HOOK_UINT32, 0xE08950},
	{0x0003F13F, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)sound_start_check},
	{0x0003F144, CODE_HOOK | HOOK_UINT32, 0xC0855D},
	{0x0003F148, CODE_HOOK | HOOK_JMP_DOOM, 0x0003F365},
	{0x0003F147, CODE_HOOK | HOOK_UINT16, 0x840F},
	{0x0003F14D, CODE_HOOK | HOOK_UINT32, 0x00EBC789},
	{0x0003F151, CODE_HOOK | HOOK_SET_NOPS, 5},
	{0x0003F205, CODE_HOOK | HOOK_UINT16, 0x2FEB},
	// disable sfx->link
	{0x0003F171, CODE_HOOK | HOOK_UINT8, 0xEB},
	{0x0003F493, CODE_HOOK | HOOK_UINT8, 0xEB},
	// disable some M_Random calls
	{0x0003F248, CODE_HOOK | HOOK_UINT16, 0x78EB},
	// A_Saw, CHAN_WEAPON
	{0x0002D666, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)start_weapon_sound},
	{0x0002D677, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)start_weapon_sound},
	// A_Punch, CHAN_WEAPON
	{0x0002D5C9, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)start_weapon_sound},
};

