// kgsws' ACE Engine
////

#define MAX_SPRITE_NAMES	1024

extern uint32_t mobj_netid;

extern uint32_t num_spr_names;
extern uint32_t sprite_table[MAX_SPRITE_NAMES];

extern uint32_t num_mobj_types;
extern mobjinfo_t *mobjinfo;
extern state_t *states;

//

void init_decorate();

int32_t mobj_check_type(uint64_t alias);

