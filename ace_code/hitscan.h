// kgsws' Doom ACE
//

extern int32_t *la_damage;
extern mobj_t *hitscan_mobj;

// hooks
void hitscan_HitMobj(mobj_t*,fixed_t) __attribute((regparm(2),no_caller_saved_registers));
