// kgsws' ACE Engine
////

#define PLAYER_SOUND_CLASS_LEN	64
#define PLAYER_SOUND_SLOT_LEN	12

//

extern musicinfo_t *S_music;

//

void init_sound();

void start_music(int32_t lump, uint32_t loop);

void sfx_rng_fix(uint16_t *idx, uint32_t pmatch);

uint64_t sfx_alias(uint8_t*);

uint16_t sfx_by_alias(uint64_t);
uint16_t sfx_by_name(uint8_t*);

