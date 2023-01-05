// kgsws' ACE Engine
////

#define FONT_MAGIC_ID	0x1AD5E6E1
#define FONT_VERSION	0x11

//

void font_generate();
void *font_load(int32_t lump);

void font_center_text(const uint8_t *text, int32_t font, uint32_t linecount);

