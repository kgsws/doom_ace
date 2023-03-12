// kgsws' ACE Engine
////

#define TP_MEMORY_ADDR	(screen_buffer + 64000 + 69632)
#define TP_MEMORY_SIZE	0x28000

//

extern uint8_t *tp_text_ptr;
extern uint_fast8_t tp_is_string;

extern uint_fast8_t tp_enable_math;
extern uint_fast8_t tp_enable_script;
extern uint_fast8_t tp_enable_array;
extern uint_fast8_t tp_enable_newline;

//

void tp_load_lump(lumpinfo_t*);
uint32_t tp_load_file(const uint8_t *path);
void tp_use_text(uint8_t *ptr);

uint8_t *tp_get_keyword();
uint8_t *tp_get_keyword_uc();
uint8_t *tp_get_keyword_lc();

uint32_t tp_must_get(const uint8_t *kv);
uint32_t tp_must_get_lc(const uint8_t *kv);

void tp_push_keyword(uint8_t *kw);

uint32_t tp_skip_code_block(uint32_t depth);

//

uint64_t tp_hash64(const uint8_t *name);
uint32_t tp_hash32(const uint8_t *name);
uint32_t tp_parse_fixed(const uint8_t *text, fixed_t *value, uint32_t fracbits);

