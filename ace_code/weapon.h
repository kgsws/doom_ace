// kgsws' Doom ACE
//

#define WPN_AMMO_CHECK_AND_USE	0x80000000

extern dextra_weapon_t *weapon_now;
extern pspdef_t *weapon_ps;
extern uint32_t weapon_flash_state;
extern uint32_t weapon_fire_type;

uint32_t weapon_pickup(mobj_t *item, mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));
uint32_t ammo_pickup(mobj_t *item, mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));

void weapon_setup(player_t *pl) __attribute((regparm(2),no_caller_saved_registers));
void weapon_set_sprite(player_t *pl, uint32_t slot, uint32_t state) __attribute((regparm(2),no_caller_saved_registers));
void weapon_drop(player_t *pl) __attribute((regparm(2),no_caller_saved_registers));
uint32_t weapon_fire(player_t *pl, int32_t type) __attribute((regparm(2),no_caller_saved_registers));
uint32_t weapon_check_ammo(player_t *pl, dextra_weapon_t *wp, uint32_t flags) __attribute((regparm(2),no_caller_saved_registers));
void weapon_pick_usable(player_t *pl) __attribute((regparm(2),no_caller_saved_registers));

// hooks
void weapon_tick(player_t *pl) __attribute((regparm(2),no_caller_saved_registers));
uint8_t weapon_ticcmd_set() __attribute((regparm(2),no_caller_saved_registers));
void weapon_ticcmd_parse() __attribute((regparm(2),no_caller_saved_registers));

// state changes in codeptr
// could even be new animation code
static inline void weapon_change_state(uint32_t code)
{
	weapon_ps->state = (void*)code;
	weapon_ps->tics = -2;
}

