// kgsws' Doom ACE
//

enum
{
	DECORATE_EXTRA_INVENTORY,
	DECORATE_EXTRA_PLAYERCLASS,
	DECORATE_EXTRA_WEAPON,
	DECORATE_NUM_EXTRA
};

typedef struct
{
	uint16_t type;
	uint16_t maxcount;
	uint16_t itemcount;
	uint16_t pickupsound;
	uint16_t usesound;
	uint32_t state;
	uint8_t *message;
} __attribute__((packed)) dextra_inventory_t;

extern weaponinfo_t *weaponinfo;
extern mobjinfo_t *mobjinfo;
extern state_t *states;

extern uint32_t decorate_playerclass_count;
extern uint32_t decorate_weapon_count;
extern uint32_t decorate_inventory_count;

extern uint8_t *actor_names_ptr;
extern uint32_t decorate_num_mobjinfo;
extern uint32_t decorate_mobjinfo_idx;

extern uint32_t decorate_num_sprites;

extern void *func_extra_data;

void decorate_prepare();
void decorate_init(int enabled);
void decorate_count_actors(uint8_t *start, uint8_t *end);
void decorate_parse(uint8_t *start, uint8_t *end);

void decorate_keyconf(uint8_t *start, uint8_t *end);

int32_t decorate_get_actor(uint8_t *name);
uint32_t decorate_custom_state_find(uint8_t *name, uint8_t *end);
uint32_t decorate_animation_state_find(uint8_t *name, uint8_t *end);
void *decorate_get_storage(uint32_t size);

uint32_t decorate_get_animation_state(uint32_t);

// hooks
uint32_t enemy_chase_move(mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));

