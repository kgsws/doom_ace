// kgsws' Doom ACE
//

extern int32_t *la_damage;

// hooks
void hitscan_HitMobj(mobj_t*,fixed_t) __attribute((regparm(2),no_caller_saved_registers));
