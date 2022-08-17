// kgsws' ACE Engine
////

extern uint32_t **flattranslation;
extern uint16_t *flatlump;

//

void init_flats();

int32_t flat_num_check(uint8_t *name) __attribute((regparm(2),no_caller_saved_registers));

