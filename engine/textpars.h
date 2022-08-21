// kgsws' ACE Engine
////

#define TP_MEMORY_ADDR	(screen_buffer + 64000 + 69632)
#define TP_MEMORY_SIZE	0x28000

//

extern uint8_t *tp_text_ptr;
extern uint_fast8_t tp_is_string;

extern uint_fast8_t tp_enable_math;

//

void tp_load_lump(lumpinfo_t*);

uint8_t *tp_get_keyword();
uint8_t *tp_get_keyword_uc();

uint32_t tp_skip_code_block(uint32_t depth);

//

uint64_t tp_hash64(uint8_t *name);
uint32_t tp_parse_fixed(uint8_t *text, fixed_t *value);

