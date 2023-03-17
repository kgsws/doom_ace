// kgsws' ACE Engine
////

#define STU_WEAPON	1
#define STU_AMMO	2
#define STU_KEYS	4
#define STU_BACKPACK	8
#define STU_WEAPON_NOW	16

#define STU_WEAPON_NEW	0x8000 // evil grin

#define STU_EVERYTHING	0x00FF

#define ORIGINAL_KEY_COUNT	6

extern uint_fast8_t stbar_refresh_force;
extern uint8_t show_fps;

//

void stbar_init();

void stbar_start(player_t *pl);
void stbar_update(player_t *pl);

void stbar_draw(player_t *pl);

void stbar_set_xhair();

patch_t *stbar_icon_ptr(int32_t lump);

