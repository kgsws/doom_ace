// kgsws' ACE Engine
////

struct polyobj_s;

typedef struct
{
	thinker_t thinker;
	struct polyobj_s *poly;
	seq_sounds_t *up_seq;
	seq_sounds_t *dn_seq;
	fixed_t spd_x, spd_y;
	fixed_t dst_x, dst_y;
	fixed_t org_x, org_y;
	fixed_t thrust;
	uint16_t wait;
	uint16_t delay;
	uint16_t sndwait;
} poly_move_t;

typedef struct
{
	thinker_t thinker;
	struct polyobj_s *poly;
	seq_sounds_t *up_seq;
	seq_sounds_t *dn_seq;
	int32_t now;
	int32_t dst;
	int32_t spd;
	angle_t org;
	fixed_t thrust;
	uint16_t wait;
	uint16_t delay;
	uint16_t sndwait;
} poly_rotate_t;

//

extern uint8_t *polybmap;

//

void poly_reset();
void poly_object(map_thinghex_t*);
void poly_create();

uint32_t poly_BlockLinesIterator(int32_t x, int32_t y, line_func_t func);

uint32_t poly_move(struct polyobj_s *mirror, uint32_t door);
uint32_t poly_rotate(struct polyobj_s *mirror, uint32_t type);

