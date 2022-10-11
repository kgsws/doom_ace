// kgsws' ACE Engine
////
// This is a DEHACKED parser.

//

typedef struct
{
	uint32_t start_health;
	uint32_t start_bullets;
	uint32_t max_bonus_health;
	uint32_t max_bonus_armor;
	uint32_t max_soulsphere;
	uint32_t hp_soulsphere;
	uint32_t hp_megasphere;
	uint32_t bfg_cells;
	uint8_t no_species;
	uint8_t no_infight;
} deh_stuff_t;

//

extern deh_stuff_t dehacked;

extern const uint8_t deh_pickup_type[NUMSPRITES];

//

void init_dehacked();

