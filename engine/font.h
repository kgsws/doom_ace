// kgsws' ACE Engine
////

#define FONT_MAGIC_ID	0x1AD5E6E1
#define FONT_VERSION	0x11

//

extern uint8_t *font_color;

//

void font_generate();
void *font_load(int32_t lump);

uint32_t font_message_to_print();

uint32_t font_draw_text(int32_t x, int32_t y, const uint8_t *text, void *font);
void font_center_text(int32_t y, const uint8_t *text, void *font, uint32_t linecount);

