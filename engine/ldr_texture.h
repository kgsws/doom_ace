// kgsws' ACE Engine
////

extern uint_fast8_t tex_was_composite;

extern uint8_t *textureheightpow;

//

void init_textures(uint32_t count);

uint8_t *texture_get_column(uint32_t tex, uint32_t col);

int32_t texture_num_get(const uint8_t *name) __attribute((regparm(2),no_caller_saved_registers));
int32_t texture_num_check(const uint8_t *name) __attribute((regparm(2),no_caller_saved_registers));
uint64_t texture_get_name(uint32_t idx) __attribute((regparm(2),no_caller_saved_registers));

