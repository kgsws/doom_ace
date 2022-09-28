// kgsws' ACE Engine
////

extern uint32_t **texturetranslation;
extern uint8_t ***texturecomposite;
extern uint32_t **texturecompositesize;
extern fixed_t **textureheight;

//

void init_textures(uint32_t count);

int32_t texture_num_get(const uint8_t *name) __attribute((regparm(2),no_caller_saved_registers));
int32_t texture_num_check(const uint8_t *name) __attribute((regparm(2),no_caller_saved_registers));
uint64_t texture_get_name(uint32_t idx) __attribute((regparm(2),no_caller_saved_registers));

