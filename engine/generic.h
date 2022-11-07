// kgsws' ACE Engine
////

#define MVF_CRUSH	1
#define MVF_WAIT_STOP	2
#define MVF_BLOCK_STAY	4
#define MVF_BLOCK_SLOW	8
#define MVF_BLOCK_GO_UP	16
#define MVF_BLOCK_GO_DN	32
#define MVF_TOP_REVERSE	64
#define MVF_BOT_REVERSE	128

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
	uint16_t flags;
	uint8_t type;
	uint8_t direction;
	uint16_t delay;
	uint16_t wait;
	uint16_t sndwait;
	uint16_t lighttag;
} generic_mover_t;

//

generic_mover_t *generic_ceiling(sector_t *sec, uint32_t dir, uint32_t def_seq, uint32_t is_fast);
generic_mover_t *generic_ceiling_by_sector(sector_t *sec);

generic_mover_t *generic_floor(sector_t *sec, uint32_t dir, uint32_t def_seq, uint32_t is_fast);
generic_mover_t *generic_floor_by_sector(sector_t *sec);

generic_mover_t *generic_dual(sector_t *sec, uint32_t dir, uint32_t def_seq, uint32_t is_fast);

