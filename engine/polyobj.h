// kgsws' ACE Engine
////

struct polyobj_s;

typedef struct
{
	thinker_t thinker;
	struct polyobj_s *poly;
	fixed_t spd_x, spd_y;
	fixed_t dst_x, dst_y;
	fixed_t org_x, org_y;
	fixed_t thrust;
	uint16_t wait;
	uint16_t delay;
} poly_move_t;

//

extern uint8_t *polybmap;

//

void poly_reset();
void poly_object(map_thinghex_t*);
void poly_create();

uint32_t poly_BlockLinesIterator(int32_t x, int32_t y, line_func_t func);

uint32_t poly_door_slide(struct polyobj_s *mirror);

