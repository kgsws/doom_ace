// kgsws' Doom ACE
//

#define INVFLAG_NO_SCREEN_FLASH	0x0001
#define INVFLAG_IS_CUSTOM	0x8000	// CustomInventory

enum
{
	DECORATE_EXTRA_INVENTORY,
	DECORATE_EXTRA_PLAYERCLASS,
	DECORATE_EXTRA_WEAPON,
	DECORATE_EXTRA_AMMO,
	DECORATE_NUM_EXTRA
};

typedef struct
{
	uint16_t type;
	uint16_t flags;
	uint16_t maxcount;
	uint16_t pickupcount;
	uint16_t pickupsound;
	uint16_t usesound;
	uint32_t state_pickup;
	uint32_t state_use;
	uint8_t *message;
} __attribute__((packed)) dextra_inventory_t;

typedef struct dextra_playerclass_s
{
	uint16_t type;
	uint16_t flags;
	uint16_t motype;
	int16_t menuidx;
	fixed_t viewheight;
	fixed_t attackz;
	fixed_t jumpz;
	uint32_t maxhealth;
	uint8_t spawnclass; // 0 to 3
//	Player.ColorRange
//	Player.MorphWeapon
//	Player.WeaponSlot
//	Player.StartItem
//	uint8_t *name;
} __attribute__((packed)) dextra_playerclass_t;

typedef struct
{
	uint16_t type;
	uint16_t flags;
	uint16_t motype;
	uint16_t ammotype[2];
	uint16_t ammogive[2];
	uint16_t ammouse[2];
	uint16_t pickupsound;
	uint16_t upsound;
	uint16_t readysound;
	uint16_t kickback;
	uint16_t selection;
	uint32_t ready_state;
	uint32_t deselect_state;
	uint32_t select_state;
	uint32_t fire1_state;
	uint32_t fire2_state;
	uint32_t hold1_state;
	uint32_t hold2_state;
	uint32_t flash1_state;
	uint32_t flash2_state;
	uint32_t deadlow_state;
	uint8_t *message;
} __attribute__((packed)) dextra_weapon_t;

typedef union decorate_extra_info_u
{
	struct
	{
		uint16_t type;
		uint16_t flags;
	};
	dextra_inventory_t inventory;
	dextra_playerclass_t playerclass;
	dextra_weapon_t weapon;
} decorate_extra_info_t;


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

extern fixed_t *viletryx;

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
fixed_t vile_chase_move(mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));

