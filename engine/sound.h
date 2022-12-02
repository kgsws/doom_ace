// kgsws' ACE Engine
////

#define PLAYER_SOUND_CLASS_LEN	64
#define PLAYER_SOUND_SLOT_LEN	12

// SOUND_CHAN_VOICE is the default
#define SOUND_CHAN_BODY(mo)	((void*)(mo) + offsetof(mobj_t,sound_body) - sizeof(thinker_t))
#define SOUND_CHAN_WEAPON(mo)	((void*)(mo) + offsetof(mobj_t,sound_weapon) - sizeof(thinker_t))

// SNDSEQ
#define	SEQ_IS_DOOR	0x8000

enum
{
	SFX_QUAKE = NUMSFX,
	SFX_FREEZE,
	SFX_ICEBREAK,
	//
	NEW_NUMSFX
};

enum
{
	SNDSEQ_DOOR,
	SNDSEQ_PLAT,
	SNDSEQ_CEILING,
	SNDSEQ_FLOOR,
	//
	NUMSNDSEQ
};

//

void init_sound();

void start_music(int32_t lump, uint32_t loop);

void sfx_rng_fix(uint16_t *idx, uint32_t pmatch);

uint64_t sfx_alias(uint8_t*);

uint16_t sfx_by_alias(uint64_t);
uint16_t sfx_by_name(uint8_t*);

sound_seq_t *snd_seq_by_sector(sector_t*, uint32_t);
sound_seq_t *snd_seq_by_id(uint32_t id);
sound_seq_t *snd_seq_by_name(const uint8_t *name);

void S_StopSound(mobj_t*);

