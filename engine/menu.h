// kgsws' ACE Engine
////

#define SKULLXOFF	-32
#define LINEHEIGHT	16
#define CURSORX_SMALL	-10
#define LINEHEIGHT_SMALL	9

//

extern uint16_t *menu_item_now;

//

void init_menu();
void menu_draw_slot_bg(uint32_t x, uint32_t y, uint32_t width);

void menu_setup_episodes();
