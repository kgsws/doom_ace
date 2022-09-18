// kgsws' ACE Engine
////

#define STU_WEAPON	1
#define STU_KEYS	2
#define STU_BACKPACK	4
#define STU_WEAPON_NOW	8

#define STU_WEAPON_NEW	0x8000 // evil grin

#define STU_EVERYTHING	0x00FF

extern uint32_t *stbar_refresh_force;
extern uint32_t *screenblocks;

void stbar_start(player_t *pl);
void stbar_update(player_t *pl);

