// kgsws' ACE Engine
////

#define FONT_MAGIC_ID	0x1AD5E6E1
#define FONT_VERSION	0x11

enum
{
	FCOL_BRICK,
	FCOL_TAN,
	FCOL_GRAY,
	FCOL_GREEN,
	FCOL_BROWN,
	FCOL_GOLD,
	FCOL_RED,
	FCOL_BLUE,
	FCOL_ORANGE,
	FCOL_WHITE,
	FCOL_YELLOW,
	// 'untranslated' does not exist
	FCOL_BLACK,
	FCOL_LIGHTBLUE,
	FCOL_CREAM,
	FCOL_OLIVE,
	FCOL_DARKGREEN,
	FCOL_DARKRED,
	FCOL_DARKBROWN,
	FCOL_PURPLE,
	FCOL_DARKGRAY,
	FCOL_CYAN,
	FCOL_ICE,
	FCOL_FIRE,
	FCOL_SAPPHIRE,
	FCOL_TEAL,
	//
	FCOL_COUNT,
	FCOL_ORIGINAL = -1
};

//

extern uint8_t *font_color;

extern void *smallfont;
extern uint32_t smallfont_height;

//

void font_generate();
void *font_load(int32_t lump);

uint32_t font_message_to_print();

uint32_t font_draw_text(int32_t x, int32_t y, const uint8_t *text, void *font);
void font_center_text(int32_t x, int32_t y, const uint8_t *text, void *font, uint32_t linecount);
void font_menu_text(int32_t x, int32_t y, const uint8_t *text) __attribute((regparm(3),no_caller_saved_registers)); // three!

