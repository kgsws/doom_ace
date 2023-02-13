// kgsws' ACE Engine
////

#define MAX_SPRITE_NAMES	1024
#define MAX_PLAYER_CLASSES	512

#define EXTRA_STORAGE_PTR	((void*)ptr_visplanes)
#define EXTRA_STORAGE_SIZE	(mod_config.visplane_count * sizeof(visplane_t))
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
	STATE_SPECIAL_HIDE,
	STATE_SPECIAL_RESTORE,
	//
	NEW_NUMSTATES
};

enum
{
	SPR_TNT1 = NUMSPRITES,
	SPR_PIST,
	SPR_ICEC,
};

typedef struct
{
	const uint8_t *name;
	uint32_t bits;
} dec_flag_t;

typedef struct
{
	const uint8_t *name;
	uint8_t idx;
	uint8_t type;
	uint16_t offset;
} dec_anim_t;

typedef struct
{
	const uint8_t *name;
	uint32_t pain;
	uint32_t death;
	uint32_t xdeath;
	uint32_t crash;
	uint32_t xcrash;
} dec_damage_type_t;

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

extern dec_damage_type_t damage_type_config[NUM_DAMAGE_TYPES];

extern void *dec_es_ptr;

extern uint32_t dec_mod_csum;

//
extern const dec_flag_t mobj_flags0[];
extern const dec_flag_t mobj_flags1[];
extern const dec_flag_t mobj_flags2[];

//

void init_decorate();

int32_t mobj_check_type(uint64_t alias);
int32_t mobj_by_spawnid(uint32_t id);

void *dec_es_alloc(uint32_t size);
void *dec_reloc_es(void *target, void *ptr);

uint32_t dec_resolve_animation(mobjinfo_t *info, uint32_t offset, uint16_t anim, uint32_t limit);

uint32_t dec_get_custom_damage(const uint8_t *name, const uint8_t *mmod);
int32_t dec_get_powerup_type(const uint8_t *name);

uint32_t dec_mobj_custom_state(mobjinfo_t *info, uint32_t alias);

uint32_t dec_reslove_state(mobjinfo_t *info, uint32_t current, uint32_t next, uint32_t extra);

const dec_anim_t *dec_find_animation(const uint8_t *name);

