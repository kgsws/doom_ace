// kgsws' ACE Engine
////

#define MVF_CRUSH	1
#define MVF_WAIT_STOP	2
#define MVF_TOP_REVERSE	4
#define MVF_BOT_REVERSE	8

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
	fixed_t speed;
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

