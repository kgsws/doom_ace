// kgsws' ACE Engine
////

//

void powerup_give(player_t *pl, mobjinfo_t *info);

void weapon_setup(player_t *pl);
uint32_t weapon_fire(player_t *pl, uint32_t secondary, uint32_t refire);

void wpn_codeptr(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));

