// kgsws' ACE Engine
////

#define DAMAGE_IS_PROJECTILE	0x40000000
#define DAMAGE_IS_RIPPER	0x60000000
#define DAMAGE_TYPE_CHECK	0x60000000

#define DAMAGE_IS_CUSTOM	0x80000000
#define DAMAGE_CUSTOM(lo,hi,mul,add)	((lo) | ((hi) << 9) | ((add) << 18) | ((mul) << 27) | DAMAGE_IS_CUSTOM)

//

extern uint32_t *respawnmonsters;

extern thinker_t *thinkercap;
extern uint32_t mobj_netid;

extern uint32_t mo_puff_type;
extern uint32_t mo_puff_flags;

//

#define mobj_set_animation(mo,anim)	mobj_set_state((mo), STATE_SET_ANIMATION((anim), 0))

uint32_t mobj_set_state(mobj_t *mo, uint32_t state) __attribute((regparm(2),no_caller_saved_registers));

void mobj_remove(mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));

// spawns
void spawn_player(mapthing_t *mt) __attribute((regparm(2),no_caller_saved_registers));
void mobj_spawn_puff(divline_t *trace, mobj_t *target);
void mobj_spawn_blood(divline_t *trace, mobj_t *target, uint32_t damage);

// internaction
void mobj_damage(mobj_t *target, mobj_t *cause, mobj_t *source, uint32_t damage, uint32_t extra);
void explode_missile(mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));

// inventory
uint32_t mobj_give_inventory(mobj_t *mo, uint16_t type, uint16_t count);
void mobj_use_item(mobj_t *mo, struct inventory_s *item);

// helpers
uint32_t mobj_for_each(uint32_t (*cb)(mobj_t*));
mobj_t *mobj_by_netid(uint32_t netid);

