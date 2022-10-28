// kgsws' ACE Engine
////

#define MAX_SPRITE_NAMES	1024
#define MAX_PLAYER_CLASSES	512

#define EXTRA_STORAGE_PTR	((void*)d_drawsegs)
#define EXTRA_STORAGE_SIZE	12288
#define EXTRA_STORAGE_END	(EXTRA_STORAGE_PTR + EXTRA_STORAGE_SIZE)

enum
{
	MOBJ_IDX_UNKNOWN = NUMMOBJTYPES,
	MOBJ_IDX_FIST,
	MOBJ_IDX_PISTOL,
	MOBJ_IDX_ICE_CHUNK,
	MOBJ_IDX_ICE_CHUNK_HEAD,
	//
	NEW_NUMMOBJTYPES
};

enum
{
	STATE_UNKNOWN_ITEM = NUMSTATES,
	STATE_PISTOL,
	STATE_ICE_DEATH_0,
	STATE_ICE_DEATH_1,
	STATE_ICE_CHUNK_0,
	STATE_ICE_CHUNK_1,
	STATE_ICE_CHUNK_2,
	STATE_ICE_CHUNK_3,
	STATE_ICE_CHUNK_4,
	STATE_ICE_CHUNK_PLR,
	//
	NEW_NUMSTATES
};

enum
{
	SPR_TNT1 = NUMSPRITES,
	SPR_PIST,
	SPR_ICEC,
};

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

extern uint8_t *damage_type_name[NUM_DAMAGE_TYPES];

extern void *dec_es_ptr;

//

void init_decorate();

int32_t mobj_check_type(uint64_t alias);

void *dec_es_alloc(uint32_t size);
void *dec_reloc_es(void *target, void *ptr);

custom_damage_state_t *dec_get_damage_animation(custom_damage_state_t *cst, uint32_t type);

