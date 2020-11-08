// kgsws' Doom ACE
//

extern int tp_nl_is_ws;

uint8_t *tp_skip_section_full(uint8_t *start, uint8_t *end, uint8_t *mark, uint8_t **err);
uint8_t *tp_get_uint(uint8_t *start, uint8_t *end, uint32_t *val);
uint8_t *tp_get_fixed(uint8_t *start, uint8_t *end, fixed_t *val);
uint8_t *tp_ncompare_skip(uint8_t *src, uint8_t *eof, uint8_t *templ);
uint8_t *tp_get_function(uint8_t *start, uint8_t *end);
uint8_t *tp_get_string(uint8_t *start, uint8_t *end);
uint8_t *tp_get_keyword(uint8_t *start, uint8_t *end);
int tp_is_ws_next(uint8_t *start, uint8_t *end, int allow_comments);
uint8_t *tp_skip_wsc(uint8_t *start, uint8_t *end);
uint8_t *tp_skip_ws(uint8_t *start, uint8_t *end);

