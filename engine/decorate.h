// kgsws' ACE Engine
////

#define MAX_SPRITE_NAMES	1024
#define MAX_PLAYER_CLASSES	512

#define EXTRA_STORAGE_PTR	((void*)0x0002D0A0 + doom_data_segment)	// drawsegs
#define EXTRA_STORAGE_SIZE	12288
#define EXTRA_STORAGE_END	(EXTRA_STORAGE_PTR + EXTRA_STORAGE_SIZE)

#define MOBJ_IDX_UNKNOWN	(NUMMOBJTYPES + 0)
#define MOBJ_IDX_FIST	(NUMMOBJTYPES + 1)
#define MOBJ_IDX_PISTOL	(NUMMOBJTYPES + 2)

//

extern uint32_t num_spr_names;
extern uint32_t sprite_table[MAX_SPRITE_NAMES];

extern uint32_t num_mobj_types;
extern mobjinfo_t *mobjinfo;

extern uint32_t num_states;
extern state_t *states;

extern uint32_t num_player_classes;
extern uint16_t player_class[MAX_PLAYER_CLASSES];

extern uint8_t *parse_actor_name;

//

void init_decorate();

int32_t mobj_check_type(uint64_t alias);

void *dec_es_alloc(uint32_t size);

