// kgsws' ACE Engine
////

#define MAX_SPRITE_NAMES	1024

#define EXTRA_STORAGE_PTR	((void*)0x0002D0A0 + doom_data_segment)
#define EXTRA_STORAGE_SIZE	12288

#define UNKNOWN_MOBJ_IDX	29 // TODO: 'unknown thing'

extern uint32_t mobj_netid;

extern uint32_t num_spr_names;
extern uint32_t sprite_table[MAX_SPRITE_NAMES];

extern uint32_t num_mobj_types;
extern mobjinfo_t *mobjinfo;

extern uint32_t num_states;
extern state_t *states;

extern uint8_t *parse_actor_name;

//

void init_decorate();

int32_t mobj_check_type(uint64_t alias);

void *dec_es_alloc(uint32_t size);

void mobj_damage(mobj_t *target, mobj_t *cause, mobj_t *source, uint32_t damage, uint32_t extra);

void explode_missile(mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));
uint32_t mobj_set_state(mobj_t *mo, uint32_t state) __attribute((regparm(2),no_caller_saved_registers));

#define mobj_set_animation(mo,anim)	mobj_set_state((mo), STATE_SET_ANIMATION((anim), 0))

