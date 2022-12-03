// kgsws' ACE Engine
////

#define POLYOBJ_MAX	255

typedef struct polyobj_s
{
	seg_t **segs;
	vertex_t *origin;
	union
	{
		struct polyobj_s *mirror;
		subsector_t *subsec;
	};
	fixed_t x, y;
	angle_t angle;
	angle_t angle_old;
	int32_t box[4];
	uint32_t validcount;
	union
	{
		struct
		{
			uint8_t segcount;
			uint8_t busy;
			uint8_t type;
			uint8_t id;
			uint8_t sndseq;
		};
		degenmobj_t soundorg;
	};
} polyobj_t;

typedef struct
{
	thinker_t thinker;
	polyobj_t *poly;
	seq_sounds_t *up_seq;
	seq_sounds_t *dn_seq;
	fixed_t spd_x, spd_y;
	fixed_t dst_x, dst_y;
	fixed_t org_x, org_y;
	fixed_t thrust;
	uint16_t dir;
	uint16_t wait;
	uint16_t delay;
	uint16_t sndwait;
} poly_move_t;

typedef struct
{
	thinker_t thinker;
	polyobj_t *poly;
	seq_sounds_t *up_seq;
	seq_sounds_t *dn_seq;
	int32_t now;
	int32_t dst;
	int32_t spd;
	angle_t org;
	fixed_t thrust;
	uint16_t dir;
	uint16_t wait;
	uint16_t delay;
	uint16_t sndwait;
} poly_rotate_t;

//

extern uint32_t poly_count;
extern polyobj_t polyobj[POLYOBJ_MAX];

extern uint8_t *polybmap;

//

void poly_reset();
void poly_object(map_thinghex_t*);
void poly_create();

polyobj_t *poly_find(uint32_t id, uint32_t create);
void poly_update_position(polyobj_t *poly);

uint32_t poly_BlockLinesIterator(int32_t x, int32_t y, line_func_t func);

void think_poly_move(poly_move_t *pm) __attribute((regparm(2),no_caller_saved_registers));
void think_poly_rotate(poly_rotate_t *pr) __attribute((regparm(2),no_caller_saved_registers));

poly_move_t *poly_mover(polyobj_t *poly);
poly_rotate_t *poly_rotater(polyobj_t *poly);

uint32_t poly_move(polyobj_t *mirror, uint32_t door);
uint32_t poly_rotate(polyobj_t *mirror, uint32_t type);

