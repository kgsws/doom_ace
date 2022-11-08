// kgsws' ACE Engine
////

#define MVF_WAIT_STOP	0x0001
#define MVF_BLOCK_STAY	0x0002
#define MVF_BLOCK_SLOW	0x0004
#define MVF_BLOCK_GO_UP	0x0008
#define MVF_BLOCK_GO_DN	0x0010
#define MVF_TOP_REVERSE	0x0020
#define MVF_BOT_REVERSE	0x0040
#define MVF_SET_TEXTURE	0x0080
#define MVF_SET_SPECIAL	0x0100

#define ACT_CEILING	1
#define ACT_FLOOR	2

enum
{
	DIR_UP,
	DIR_DOWN,
};

//

typedef struct
{
	thinker_t thinker;
	sector_t *sector;
	seq_sounds_t *up_seq;
	seq_sounds_t *dn_seq;
	fixed_t top_height;
	fixed_t bot_height;
	fixed_t speed_now;
	union
	{
		fixed_t speed_start;
		fixed_t gap_height;
	};
	uint8_t type;
	uint8_t direction;
	uint16_t flags;
	uint16_t sndwait;
	uint16_t wait;
	uint16_t delay;
	uint16_t crush;
	uint16_t lighttag;
	uint16_t texture;
	uint16_t special;
	uint8_t seq_save;
} generic_mover_t;

//

generic_mover_t *generic_ceiling(sector_t *sec, uint32_t dir, uint32_t def_seq, uint32_t is_fast);
generic_mover_t *generic_ceiling_by_sector(sector_t *sec);

generic_mover_t *generic_floor(sector_t *sec, uint32_t dir, uint32_t def_seq, uint32_t is_fast);
generic_mover_t *generic_floor_by_sector(sector_t *sec);

generic_mover_t *generic_dual(sector_t *sec, uint32_t dir, uint32_t def_seq, uint32_t is_fast);

//
void think_ceiling(generic_mover_t *gm) __attribute((regparm(2),no_caller_saved_registers));
void think_floor(generic_mover_t *gm) __attribute((regparm(2),no_caller_saved_registers));
void think_dual(generic_mover_t *gm) __attribute((regparm(2),no_caller_saved_registers));

