// kgsws' ACE Engine
////

void init_sound();

void sfx_rng_fix(uint16_t *idx, uint32_t pmatch);

uint64_t sfx_alias(uint8_t*);

uint16_t sfx_by_alias(uint64_t);
uint16_t sfx_by_name(uint8_t*);

