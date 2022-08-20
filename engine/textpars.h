// kgsws' ACE Engine
////

#define TP_MEMORY_ADDR	(screen_buffer + 64000 + 69632)
#define TP_MEMORY_SIZE	0x28000

//

extern uint8_t *tp_text_ptr;

//

void tp_load_lump(lumpinfo_t*);

uint8_t *tp_get_keyword();

