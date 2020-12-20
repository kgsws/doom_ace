// kgsws' Doom ACE
//

extern dextra_weapon_t *weapon_now;
extern pspdef_t *weapon_ps;
extern uint32_t weapon_flash_state;

uint32_t weapon_pickup(mobj_t *item, mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));

void weapon_setup(player_t *pl) __attribute((regparm(2),no_caller_saved_registers));
void weapon_set_sprite(player_t *pl, uint32_t slot, uint32_t state) __attribute((regparm(2),no_caller_saved_registers));
void weapon_drop(player_t *pl) __attribute((regparm(2),no_caller_saved_registers));
void weapon_fire(player_t *pl, int32_t type) __attribute((regparm(2),no_caller_saved_registers));

// hooks
void weapon_tick(player_t *pl) __attribute((regparm(2),no_caller_saved_registers));

// state changes in codeptr
// could even be new animation code
static inline void weapon_change_state(uint32_t code)
{
	weapon_ps->state = (void*)code;
	weapon_ps->tics = -2;
}

