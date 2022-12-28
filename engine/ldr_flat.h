// kgsws' ACE Engine
////

extern uint16_t *flatlump;

extern uint8_t **flattexture_data;
extern uint16_t *flattexture_idx;
extern uint8_t *flattexture_mask;

//

void init_flats();

int32_t flat_num_get(const uint8_t *name) __attribute((regparm(2),no_caller_saved_registers));
int32_t flat_num_check(const uint8_t *name) __attribute((regparm(2),no_caller_saved_registers));
uint64_t flat_get_name(uint32_t idx) __attribute((regparm(2),no_caller_saved_registers));
void *flat_generate_composite(uint32_t idx);

void flat_init_data() __attribute((regparm(2),no_caller_saved_registers));

