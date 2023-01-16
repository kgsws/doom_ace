// kgsws' ACE Engine
////

#define DAMAGE_IS_PROJECTILE	0x40000000
#define DAMAGE_IS_RIPPER	0x60000000
#define DAMAGE_TYPE_CHECK	0x60000000

#define DAMAGE_IS_CUSTOM	0x80000000
#define DAMAGE_CUSTOM_TYPE	0x70000000
#define DAMAGE_CUSTOM(lo,hi,mul,add)	((lo) | ((hi) << 9) | ((add) << 18) | ((mul) << 27) | DAMAGE_IS_CUSTOM)
#define DAMAGE_WITH_TYPE(damage,type)	((damage) | ((type) << 20) | DAMAGE_CUSTOM_TYPE)

#define TELEF_USE_Z	1
#define TELEF_USE_ANGLE	2
#define TELEF_NO_KILL	4
#define TELEF_FOG	8

//

extern uint32_t mobj_netid;

extern uint32_t mo_puff_type;
extern uint32_t mo_puff_flags;

extern uint_fast8_t mo_dmg_skip_armor;

extern uint_fast8_t reborn_inventory_hack;

//

void mobj_set_animation(mobj_t*,uint8_t) __attribute((regparm(2),no_caller_saved_registers));

void mobj_remove(mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));

// spawns
mobj_t *mobj_spawn_player(uint32_t idx, fixed_t x, fixed_t y, angle_t angle);
void mobj_spawn_puff(divline_t *trace, mobj_t *target, uint32_t puff_type);
void mobj_spawn_blood(divline_t *trace, mobj_t *target, uint32_t damage, uint32_t puff_type);

// internaction
uint32_t mobj_calc_damage(uint32_t damage);
void mobj_damage(mobj_t *target, mobj_t *cause, mobj_t *source, uint32_t damage, mobjinfo_t *pufftype);
void mobj_explode_missile(mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));
uint32_t mobj_range_check(mobj_t *mo, mobj_t *target, fixed_t range, uint32_t check_z);
uint32_t mobj_check_melee_range(mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));

// inventory
uint32_t mobj_give_inventory(mobj_t *mo, uint16_t type, uint16_t count);
void mobj_use_item(mobj_t *mo, struct invitem_s *item);
uint8_t *mobj_check_keylock(mobj_t *mo, uint32_t lockdef, uint32_t is_remote);

uint32_t mobj_give_health(mobj_t *mo, uint32_t count, uint32_t maxhp);

// teleport
void mobj_telestomp(mobj_t *mo, fixed_t x, fixed_t y);
uint32_t mobj_teleport(mobj_t *mo, fixed_t x, fixed_t y, fixed_t z, angle_t angle, uint32_t flags);

// helpers
uint32_t mobj_for_each(uint32_t (*cb)(mobj_t*));
mobj_t *mobj_by_tid_first(uint32_t tid);
mobj_t *mobj_by_netid(uint32_t netid);

// replacements
uint32_t mobj_change_sector(sector_t*,uint32_t) __attribute((regparm(2),no_caller_saved_registers));

