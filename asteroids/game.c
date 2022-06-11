// kgsws' Doom ACE
// ASTEROIDS game example.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "game.h"
#include "intro.h"

#define SCORE_DIGITS	6

#define PLASMA_COUNT	8
#define PLASMA_SPEED	64
#define PLASMA_TICKS	35
#define APLASMA_SPEED	56
#define APLASMA_TICKS	40
#define FIRE_COOLDOWN	5

#define ROCK_COUNT	32
#define SPARK_COUNT	32

#define ROCK_LIMIT	16
#define ROCK_ALIEN_LIMIT	4

#define SOUND_COOLDOWN	10
#define SOUND_COOLDOWN_BOOM	25

#define GAME_OVER_TIME	(10 * 35)
#define MAX_IDLE_TIME	(15 * 35)

#define COLOR_SHIP	0x71
#define COLOR_SHIP_HARD	0xFF
#define COLOR_PLASMA	0x1C2
#define COLOR_ROCK	0x58
#define COLOR_ALIEN	0x1A0
#define COLOR_APLASMA	0x1AE
#define COLOR_EXHAUST	0x1A1
#define COLOR_SPARK_R	0x1B0	// 'red to dark' animation 0 to 15
#define COLOR_SPARK_G	0x170	// 'green to dark' animation 0 to 15
#define COLOR_SPARK_B	0x188	// 'brown to dark' animation 0 to 15
#define COLOR_SPARK_Y	0x1A0	// 'yellow to brown' animation 0 to 7
#define COLOR_SPARK_W	0x160	// 'gray to dark' animation 0 to 15

#define ALIEN_VEL	(28 << FRACBITS)
#define SPARK_SPEED	8

#define LIMIT_X	(2240 << FRACBITS)
#define LIMIT_Y	(1400 << FRACBITS)
#define LIMIT_VEL	(128 << FRACBITS)

#define RADIUS_SHIP	(80 << FRACBITS)
#define RADIUS_ALIEN	(128 << FRACBITS)
#define RADIUS_ROCK_B	(470 << FRACBITS)
#define RADIUS_ROCK_M	(254 << FRACBITS)
#define RADIUS_ROCK_S	(128 << FRACBITS)
#define RADIUS_ROCK_T	(64 << FRACBITS)
#define RADIUS_MARGIN	(16 << FRACBITS)

#define KEY_UP	1
#define KEY_DOWN	2
#define KEY_LEFT	4
#define KEY_RIGHT	8
#define KEY_FIRE0	16
#define KEY_FIRE1	32
#define KEY_HARDCORE	64

#define SHIP_IDX	0
#define ALIEN_IDX	1

#define APLASMA_IDX	2
#define PLASMA_FIRST	3
#define PLASMA_END	(PLASMA_FIRST + PLASMA_COUNT)

#define ROCK_FIRST	PLASMA_END
#define ROCK_END	(ROCK_FIRST + ROCK_COUNT)

#define SPARK_FIRST	ROCK_END
#define SPARK_END	(SPARK_FIRST + SPARK_COUNT)

#define OBJECT_COUNT	(1 + PLASMA_COUNT + ROCK_COUNT + SPARK_COUNT)

enum
{
	TYPE_SHIP,
	TYPE_ALIEN,
	TYPE_PLASMA,
	TYPE_ROCK_BIG,
	TYPE_ROCK_MEDIUM,
	TYPE_ROCK_SMALL,
	TYPE_ROCK_TINY,
	TYPE_SPARK_8,
	TYPE_SPARK_16
};

typedef struct
{
	fixed_t x, y;
	fixed_t mx, my;
	angle_t angle;
	fixed_t radius;
	uint16_t color;
	uint8_t type;
	uint8_t ticks;
} object_t;

//
//

hook_t hook_restore[] =
{
	// restore D_PageDrawer
	{0x0001d319, CODE_HOOK | HOOK_RELADDR_DOOM, 0x00039020},
	// restore D_PageTicker
	{0x0002083f, CODE_HOOK | HOOK_RELADDR_DOOM, 0x0001D610},
	// restore M_Responder
	{0x0001d13a, CODE_HOOK | HOOK_RELADDR_DOOM, 0x00023010},
	// enable 'wipe' again
	{0x0001d4a4, CODE_HOOK | HOOK_UINT16, 0x0775}, // original instruction
	// terminator
	{0}
};

//
//

static object_t *objects;
uint32_t keys;
uint32_t fire_cooldown;
uint32_t sound_cooldown;
uint32_t rock_cooldown;
uint32_t rock_destroyed;
uint32_t rock_count;
uint32_t rocks_to_alien;
uint32_t alien_cooldown;
uint32_t new_game_flag;
uint32_t is_game_over;
uint32_t is_you_win;
uint32_t idle_time;
uint32_t game_time;
uint32_t hardcore_mode;
uint32_t randomize;

uint32_t snd_plasma = sfx_plasma;
uint32_t snd_alien = sfx_skeact;

uint8_t score[SCORE_DIGITS];

// shapes
static point_t shape_exhaust[] =
{
	{24, 64},
	{0, 128},
	{-24, 64},
	// terminator
	{0x7FFF, 0x7FFF}
};
static point_t shape_ship[] =
{
	{0, -96},
	{64, 96},
	{32, 64},
	{-32, 64},
	{-64, 96},
	// terminator
	{0x7FFF, 0x7FFF}
};
static point_t shape_alien[] =
{
	{0, -128},
	{48, 0},
	{0, 128},
	{-48, 96},
	{-48, -96},
	{0, -128},
	{0, 128},
	// terminator
	{0x7FFF, 0x7FFF}
};
static point_t shape_rock_big[] =
{
	{39, 446},
	{285, 345},
	{250, 250}, // dent
	{330, 250}, // dent
	{436, 102},
	{400, -200},
	{175, -411},
	{100, -330}, // dent
	{120, -400}, // dent
	{-160, -418},
	{-382, -233},
	{-435, 106},
	{-335, 56}, // dent
	{-420, 180}, // dent
	{-310, 322},
	// terminator
	{0x7FFF, 0x7FFF}
};
static point_t shape_rock_medium[] =
{
	{-13, 255},
	{168, 193},
	{180, 120}, // dent
	{254, 24},
	{208, -148},
	{70, -246},
	{00, -200}, // dent
	{-97, -236},
	{-226, -119},
	{-252, 39},
	{-145, 210},
	{-40, 210}, // dent
	// terminator
	{0x7FFF, 0x7FFF}
};
static point_t shape_rock_small[] =
{
	{0, 127},
	{86, 94},
	{125, 23},
	{113, -59},
	{36, -122},
	{-31, -124},
	{-115, -55},
	{-127, 10},
	{-90, 90},
	// terminator
	{0x7FFF, 0x7FFF}
};
static point_t shape_rock_tiny[] =
{
	{0, 63},
	{53, 34},
	{61, -17},
	{26, -58},
	{-33, -54},
	{-61, -17},
	{-50, 39},
	// terminator
	{0x7FFF, 0x7FFF}
};
static point_t shape_plasma[] =
{
	{-8, -8},
	{8, 8},
	{-8, 8},
	// terminator
	{0x7FFF, 0x7FFF}
};
static point_t shape_spark[] =
{
	{0, 0},
	{0, 8},
	// terminator
	{0x7FFF, 0x7FFF}
};
static void *shape_table[] =
{
	shape_ship,
	shape_alien,
	shape_plasma,
	shape_rock_big,
	shape_rock_medium,
	shape_rock_small,
	shape_rock_tiny,
	shape_spark,
	shape_spark,
	shape_spark
};

static uint8_t rock_radius[] =
{
	RADIUS_ROCK_B >> (FRACBITS + 1),
	RADIUS_ROCK_M >> (FRACBITS + 1),
	RADIUS_ROCK_S >> (FRACBITS + 1),
	RADIUS_ROCK_T >> (FRACBITS + 1)
};

//
// funcs

static inline angle_t objs_angle(object_t *o0, object_t *o1)
{
	*viewx = o0->x;
	*viewy = o0->y;
	return R_PointToAngle(o1->x, o1->y);
}

static void rotate_point(map_point_t *mp, point_t *pt, uint32_t angle)
{
	fixed_t x = (int32_t)pt->x << FRACBITS;
	fixed_t y = (int32_t)pt->y << FRACBITS;
	mp->x = FixedMul(x, finesine[angle]) - FixedMul(y, finecosine[angle]);
	mp->y = FixedMul(x, finecosine[angle]) + FixedMul(y, finesine[angle]);
}

static void draw_shape(point_t *points, fixed_t x, fixed_t y, uint32_t angle, int color)
{
	map_line_t transform;
	point_t *pt = points;

	angle >>= ANGLETOFINESHIFT;

	rotate_point(&transform.a, pt, angle);
	transform.a.x += x;
	transform.a.y += y;
	pt++;
	rotate_point(&transform.b, pt, angle);
	transform.b.x += x;
	transform.b.y += y;
	pt++;

	AM_drawMline(&transform, color);

	while(pt->x != 0x7FFF)
	{
		transform.a = transform.b;
		rotate_point(&transform.b, pt, angle);
		transform.b.x += x;
		transform.b.y += y;
		pt++;

		AM_drawMline(&transform, color);
	}

	if(color & 0x100)
		return;

	transform.a = transform.b;
	rotate_point(&transform.b, points, angle);
	transform.b.x += x;
	transform.b.y += y;

	AM_drawMline(&transform, color);
}

static uint32_t random_byte()
{
	uint32_t ret;

	ret = M_Random();
	ret ^= randomize;
	ret ^= objects[SHIP_IDX].x;
	ret ^= objects[SHIP_IDX].y;

	return ret & 0xFF;
}

//
// game

static object_t *spawn_spark(uint32_t color, uint32_t type, uint32_t ticks)
{
	object_t *obj;
	object_t *pick;
	uint8_t life = -1;

	// find a free slot
	for(obj = objects + SPARK_FIRST; obj < objects + SPARK_END; obj++)
	{
		if(!obj->color)
		{
			pick = obj;
			break;
		}

		if(obj->color < COLOR_SPARK_W || obj->color > COLOR_SPARK_W + 16)
			continue;

		if(obj->ticks < life)
		{
			pick = obj;
			life = obj->ticks;
		}
	}

	pick->color = color;
	pick->type = type;
	pick->ticks = ticks;

	return pick;
}

static object_t *spawn_rock(fixed_t x, fixed_t y, uint8_t type)
{
	object_t *obj;

	// find a free slot
	for(obj = objects + ROCK_FIRST; obj < objects + ROCK_END; obj++)
	{
		if(!obj->color)
			break;
	}
	if(obj >= objects + ROCK_END)
		// no more slots
		return NULL;

	obj->color = COLOR_ROCK;
	obj->type = type;
	obj->radius = (uint32_t)rock_radius[type - TYPE_ROCK_BIG] << (FRACBITS + 1);
	obj->x = x;
	obj->y = y;
	obj->mx = random_byte() << 11;
	obj->my = random_byte() << 11;
	obj->angle = random_byte() << 24;

	rock_count++;

	return obj;
}

static void destroy_ship()
{
	for(uint32_t i = 0; i < 12; i++)
	{
		object_t *obj;
		uint32_t angle;

		angle = 32 + (random_byte() & 7);
		if(i < 7)
			obj = spawn_spark(hardcore_mode ? COLOR_SPARK_B : COLOR_SPARK_G, TYPE_SPARK_16, angle);
		else
		if(i < 10)
			obj = spawn_spark(COLOR_SPARK_Y, TYPE_SPARK_8, angle);
		else
			obj = spawn_spark(COLOR_SPARK_R, TYPE_SPARK_16, angle);
		angle = random_byte() << 24;
		obj->x = objects[SHIP_IDX].x + (random_byte() << 15) - (64 << FRACBITS);
		obj->y = objects[SHIP_IDX].y + (random_byte() << 15) - (64 << FRACBITS);
		obj->angle = angle;
		obj->mx = finecosine[angle >> ANGLETOFINESHIFT] * SPARK_SPEED;
		obj->my = -finesine[angle >> ANGLETOFINESHIFT] * SPARK_SPEED;
		obj->mx += objects[SHIP_IDX].mx / 2;
		obj->my += objects[SHIP_IDX].my / 2;
	}

	new_game_flag = 3;
	rocks_to_alien = 0;
	alien_cooldown = 0;
	objects[SHIP_IDX].x = 0;
	objects[SHIP_IDX].y = 0;
	if(!rock_cooldown && !objects[ALIEN_IDX].color)
		rock_cooldown = 5;

	// game over screen
	is_game_over = GAME_OVER_TIME;
	intro_start = 0;
	intro_end = 0;
}

static void destroy_rock(object_t *rock, object_t *other)
{
	uint32_t angle;
	fixed_t x, y;
	fixed_t sx, sy;
	fixed_t mx, my;
	uint8_t type;

	rock_count--;

	rock_destroyed++;

	if(!objects[ALIEN_IDX].color)
	{
		if(!(rock_destroyed & 7) && (rock_cooldown > 5 || !rock_cooldown))
			rock_cooldown = 5;
		if(objects[SHIP_IDX].color && rocks_to_alien)
		{
			rocks_to_alien--;
			if(!rocks_to_alien)
				alien_cooldown = 15 + (random_byte() & 31);
		}
	}

	x = rock->x;
	y = rock->y;

	if(other->type == TYPE_PLASMA)
		angle = ((other->angle & 0xFF) << 24) + 0x40000000;
	else
		angle = objs_angle(rock, other) + 0x40000000;

	sx = finecosine[angle >> ANGLETOFINESHIFT];
	sy = -finesine[angle >> ANGLETOFINESHIFT];

	switch(rock->type)
	{
		case TYPE_ROCK_BIG:
			type = TYPE_ROCK_MEDIUM;
			sx *= (RADIUS_ROCK_M >> FRACBITS) + 1;
			sy *= (RADIUS_ROCK_M >> FRACBITS) + 1;
		break;
		case TYPE_ROCK_MEDIUM:
			type = TYPE_ROCK_SMALL;
			sx *= (RADIUS_ROCK_S >> FRACBITS) + 1;
			sy *= (RADIUS_ROCK_S >> FRACBITS) + 1;
		break;
		case TYPE_ROCK_SMALL:
			type = TYPE_ROCK_TINY;
			sx *= (RADIUS_ROCK_T >> FRACBITS) + 1;
			sy *= (RADIUS_ROCK_T >> FRACBITS) + 1;
		break;
		default:
			for(uint32_t i = 0; i < 5; i++)
			{
				other = spawn_spark(COLOR_SPARK_W, TYPE_SPARK_16, 48);
				other->x = x + (random_byte() << 15) - (64 << FRACBITS);
				other->y = y + (random_byte() << 15) - (64 << FRACBITS);
				other->angle = random_byte() << 24;
				other->mx = finecosine[other->angle >> ANGLETOFINESHIFT] * SPARK_SPEED;
				other->my = -finesine[other->angle >> ANGLETOFINESHIFT] * SPARK_SPEED;
				other->mx += rock->mx;
				other->my += rock->my;
			}
			return;
	}

	for(uint32_t i = 0; i < 2; i++)
	{
		other = spawn_spark(COLOR_SPARK_W, TYPE_SPARK_16, 48);
		other->x = x + (random_byte() << 15);
		other->y = y + (random_byte() << 15);
		other->angle = angle - 0xE0000000 + 0x20000000 * i;
		other->mx = finecosine[other->angle >> ANGLETOFINESHIFT] * SPARK_SPEED;
		other->my = -finesine[other->angle >> ANGLETOFINESHIFT] * SPARK_SPEED;
		other->mx += rock->mx;
		other->my += rock->my;
	}
	for(uint32_t i = 0; i < 2; i++)
	{
		other = spawn_spark(COLOR_SPARK_W, TYPE_SPARK_16, 48);
		other->x = x + (random_byte() << 15);
		other->y = y + (random_byte() << 15);
		other->angle = angle - 0x60000000 + 0x20000000 * i;
		other->mx = finecosine[other->angle >> ANGLETOFINESHIFT] * SPARK_SPEED;
		other->my = -finesine[other->angle >> ANGLETOFINESHIFT] * SPARK_SPEED;
		other->mx += rock->mx;
		other->my += rock->my;
	}

	rock = spawn_rock(x + sx, y + sy, type);
	if(!rock)
		return;

	other = spawn_rock(x - sx, y - sy, type);

	x = (uint32_t)rock->mx >> 15;
	y = (uint32_t)rock->my >> 15;

	angle -= 0x50000000;
	rock->mx = finecosine[angle >> ANGLETOFINESHIFT] * (8 + x);
	rock->my = -finesine[angle >> ANGLETOFINESHIFT] * (8 + x);

	if(!other)
		return;

	angle -= 0x30000000;
	other->mx = finecosine[angle >> ANGLETOFINESHIFT] * (8 + y);
	other->my = -finesine[angle >> ANGLETOFINESHIFT] * (8 + y);
}

static void reset_game()
{
	// delete everything
	memset(objects, 0, sizeof(object_t) * OBJECT_COUNT);

	// initialize plasma
	for(object_t *obj = objects + APLASMA_IDX; obj < objects + PLASMA_END; obj++)
		obj->type = TYPE_PLASMA;

	// clear
	hardcore_mode = keys & KEY_HARDCORE;
	keys = 0;
	sound_cooldown = 0;
	fire_cooldown = 0;
	rock_count = 0;
	alien_cooldown = 0;
	rocks_to_alien = 32;
	new_game_flag = 0;
	is_game_over = 0;
	is_you_win = 0;
	idle_time = 0;
	game_time = 0;
	memset(score, 0, SCORE_DIGITS);

	// prepare ship
	if(hardcore_mode)
		objects[SHIP_IDX].color = COLOR_SHIP_HARD;
	else
		objects[SHIP_IDX].color = COLOR_SHIP;
	objects[SHIP_IDX].type = TYPE_SHIP;
	objects[SHIP_IDX].radius = RADIUS_SHIP;

	// first rocks
	rock_cooldown = 20;
}

static uint32_t collision_calc(fixed_t o0_x, fixed_t o0_y, fixed_t o1_x, fixed_t o1_y)
{
	int32_t dist, tmp;

	tmp = (o0_x - o1_x) >> FRACBITS;
	dist = tmp * tmp;
	tmp = (o0_y - o1_y) >> FRACBITS;
	dist += tmp * tmp;

	return dist;
}

static int32_t collision_check_full(object_t *o0, object_t *o1)
{
	int32_t ret;
	int32_t rad;

	ret = o0->radius >> FRACBITS;
	rad = ret * ret;
	ret = o1->radius >> FRACBITS;
	rad += ret * ret;

	ret = collision_calc(o0->x, o0->y, o1->x, o1->y);
	if(ret <= rad)
		return 1;

	if(o0->x < o1->x)
	{
		ret = collision_calc(o0->x + LIMIT_X*2, o0->y, o1->x, o1->y);
		if(ret <= rad)
			return 1;

		if(o0->y < o1->y)
		{
			ret = collision_calc(o0->x + LIMIT_X*2, o0->y + LIMIT_Y*2, o1->x, o1->y);
			if(ret <= rad)
				return 1;
		} else
		{
			ret = collision_calc(o0->x + LIMIT_X*2, o0->y - LIMIT_Y*2, o1->x, o1->y);
			if(ret <= rad)
				return 1;
		}
	} else
	{
		ret = collision_calc(o0->x - LIMIT_X*2, o0->y, o1->x, o1->y);
		if(ret <= rad)
			return 1;

		if(o0->y < o1->y)
		{
			ret = collision_calc(o0->x - LIMIT_X*2, o0->y + LIMIT_Y*2, o1->x, o1->y);
			if(ret <= rad)
				return 1;
		} else
		{
			ret = collision_calc(o0->x - LIMIT_X*2, o0->y - LIMIT_Y*2, o1->x, o1->y);
			if(ret <= rad)
				return 1;
		}
	}

	if(o0->y < o1->y)
	{
		ret = collision_calc(o0->x, o0->y + LIMIT_Y*2, o1->x, o1->y);
		if(ret <= rad)
			return 1;
	} else
	{
		ret = collision_calc(o0->x, o0->y - LIMIT_Y*2, o1->x, o1->y);
		if(ret <= rad)
			return 1;
	}

	return 0;
}

static void score_dec()
{
	uint32_t digit;

	if(!objects[SHIP_IDX].color)
		return;

	for(digit = 0; digit < SCORE_DIGITS; digit++)
	{
		score[digit]--;
		if(score[digit] < 9)
			break;
		score[digit] = 9;
	}

	if(digit == SCORE_DIGITS)
		memset(score, 0, SCORE_DIGITS);
}

static void score_add(uint8_t digit, uint8_t count)
{
	if(!objects[SHIP_IDX].color)
		return;

	score[digit] += count;
	while(score[digit] > 9)
	{
		score[digit] -= 10;
		digit++;
		if(digit == SCORE_DIGITS)
		{
			keys &= ~(KEY_FIRE0 | KEY_FIRE1);
			new_game_flag = 3;
			is_you_win = 1;
			intro_start = 0;
			intro_end = 0;
			rock_cooldown = 0;
			alien_cooldown = 0;
			// show victory time in score
			for(uint32_t i = 0; i < SCORE_DIGITS; i++)
			{
				score[i] = game_time % 10;
				game_time /= 10;
			}
			// explode all the rocks
			S_StartSound(playermo, sfx_barexp);
			for(object_t *obj = objects + ROCK_FIRST; obj < objects + ROCK_END; obj++)
			{
				if(obj->color)
				{
					obj->color = 0;
					for(uint32_t i = 0; i < 5; i++)
					{
						object_t *other = spawn_spark(COLOR_SPARK_W, TYPE_SPARK_16, 48);
						other->x = obj->x + (random_byte() << 15) - (64 << FRACBITS);
						other->y = obj->y + (random_byte() << 15) - (64 << FRACBITS);
						other->angle = random_byte() << 24;
						other->mx = finecosine[other->angle >> ANGLETOFINESHIFT] * SPARK_SPEED;
						other->my = -finesine[other->angle >> ANGLETOFINESHIFT] * SPARK_SPEED;
						other->mx += obj->mx;
						other->my += obj->my;
					}
				}
			}
			// remove alien
			objects[ALIEN_IDX].color = 0;
			objects[APLASMA_IDX].color = 0;
			break;
		}
		score[digit]++;
	}
}

//
// API

void game_init()
{
	int32_t lmp;

	lmp = W_CheckNumForName("DSSKEACT");
	if(lmp < 0)
	{
		// shareware sounds
		snd_plasma = sfx_rlaunc;
		snd_alien = sfx_itmbk;
	}

	randomize = *gametic;
	keys = 0;

	// initialize table
	for(uint32_t i = 0; i < sizeof(shape_table) / sizeof(void*); i++)
		shape_table[i] = (void*)relocate_addr_ace(shape_table[i]);

	// allocate objects
	objects = Z_Malloc(sizeof(object_t) * OBJECT_COUNT, PU_STATIC, 0);

	// start
	reset_game();
}

__attribute((regparm(2),no_caller_saved_registers))
void game_drawer()
{
	memset(screens0, 0, 320*200);
	V_MarkRect(0, 0, 320, 200);

	// ship exhaust
	if(objects[SHIP_IDX].color && keys & KEY_UP)
		draw_shape(shape_exhaust, objects[SHIP_IDX].x, objects[SHIP_IDX].y, objects[SHIP_IDX].angle, COLOR_EXHAUST + (*gametic & 3));

	for(object_t *obj = objects + OBJECT_COUNT - 1; obj >= objects; obj--)
	{
		uint32_t flags;

		if(obj->color <= 1)
			continue;

		draw_shape(shape_table[obj->type], obj->x, obj->y, obj->angle, obj->color);

		if(!obj->radius)
			continue;

		flags = obj->x + obj->radius + RADIUS_MARGIN > LIMIT_X;
		flags |= (obj->x - obj->radius - RADIUS_MARGIN < LIMIT_X) << 1;
		flags |= (obj->y + obj->radius + RADIUS_MARGIN > LIMIT_Y) << 2;
		flags |= (obj->y - obj->radius - RADIUS_MARGIN < LIMIT_Y) << 3;

		if(flags & 0b0001)
			draw_shape(shape_table[obj->type], obj->x - LIMIT_X*2, obj->y, obj->angle, obj->color);
		if(flags & 0b0010)
			draw_shape(shape_table[obj->type], obj->x + LIMIT_X*2, obj->y, obj->angle, obj->color);
		if(flags & 0b0100)
			draw_shape(shape_table[obj->type], obj->x, obj->y - LIMIT_Y*2, obj->angle, obj->color);
		if(flags & 0b1000)
			draw_shape(shape_table[obj->type], obj->x, obj->y + LIMIT_Y*2, obj->angle, obj->color);
		if(flags & 0b0101)
			draw_shape(shape_table[obj->type], obj->x - LIMIT_X*2, obj->y - LIMIT_Y*2, obj->angle, obj->color);
		if(flags & 0b1010)
			draw_shape(shape_table[obj->type], obj->x + LIMIT_X*2, obj->y + LIMIT_Y*2, obj->angle, obj->color);
		if(flags & 0b0110)
			draw_shape(shape_table[obj->type], obj->x - LIMIT_X*2, obj->y + LIMIT_Y*2, obj->angle, obj->color);
		if(flags & 0b1001)
			draw_shape(shape_table[obj->type], obj->x + LIMIT_X*2, obj->y - LIMIT_Y*2, obj->angle, obj->color);
	}

	// game over
	if(is_game_over)
		intro_draw_text(1);

	if(is_you_win)
		// victory
		intro_draw_text(2);

	// score / victory time
	for(uint32_t i = 0, x = 315; i < SCORE_DIGITS; i++, x -= 5)
		V_DrawPatch(x, 1, 0, shortnum[score[i]]);
}

__attribute((regparm(2),no_caller_saved_registers))
void game_ticker()
{
	randomize = (randomize << 1) | (randomize >> 31);
	randomize += keys;

	if(!keys)
	{
		idle_time++;
		if(idle_time >= MAX_IDLE_TIME && objects[SHIP_IDX].color)
		{
			objects[SHIP_IDX].color = 0;
			destroy_ship();
			sound_cooldown = SOUND_COOLDOWN_BOOM;
			S_StartSound(playermo, sfx_barexp);
		}
	} else
		idle_time = 0;

	// victory
	if(is_you_win)
	{
		if(*gametic & 1)
			intro_end++;
		idle_time = 0;
	} else
		game_time++;

	// game over
	if(is_game_over)
	{
		if(is_game_over & 1)
		{
			if(intro_end < (GAME_OVER_TIME / 2) - 35)
				intro_end++;
			else
				intro_start++;
		}
		is_game_over--;
	}

	// new game
	if(new_game_flag)
	{
		if(keys & (KEY_FIRE0 | KEY_FIRE1))
		{
			if(new_game_flag == 1 && is_game_over < GAME_OVER_TIME - 35)
				reset_game();
		} else
			new_game_flag = 1;
	}

	// ship stuff
	if(sound_cooldown)
		sound_cooldown--;
	if(objects[SHIP_IDX].color)
	{
		if(keys & KEY_LEFT)
			objects[SHIP_IDX].angle -= 0x05000000;
		if(keys & KEY_RIGHT)
			objects[SHIP_IDX].angle += 0x05000000;
		if(keys & KEY_UP)
		{
			// accelerate
			uint32_t angle = objects[SHIP_IDX].angle >> ANGLETOFINESHIFT;
			objects[SHIP_IDX].mx += finecosine[angle];
			objects[SHIP_IDX].my -= finesine[angle];
			if(objects[SHIP_IDX].mx > LIMIT_VEL)
				objects[SHIP_IDX].mx = LIMIT_VEL;
			if(objects[SHIP_IDX].mx < -LIMIT_VEL)
				objects[SHIP_IDX].mx = -LIMIT_VEL;
			if(objects[SHIP_IDX].my > LIMIT_VEL)
				objects[SHIP_IDX].my = LIMIT_VEL;
			if(objects[SHIP_IDX].my < -LIMIT_VEL)
				objects[SHIP_IDX].my = -LIMIT_VEL;
			if(!sound_cooldown)
			{
				sound_cooldown = SOUND_COOLDOWN;
				S_StartSound(playermo, sfx_stnmov);
			}
		}
		if(fire_cooldown)
			fire_cooldown--;
		else
		if(keys & (KEY_FIRE0 | KEY_FIRE1))
		{
			// find free slot for plasma
			for(object_t *obj = objects + PLASMA_FIRST; obj < objects + PLASMA_END; obj++)
			{
				if(!obj->color)
				{
					// FIRE!
					uint32_t angle = objects[SHIP_IDX].angle >> ANGLETOFINESHIFT;
					obj->color = COLOR_PLASMA;
					obj->x = objects[SHIP_IDX].x;
					obj->y = objects[SHIP_IDX].y;
					obj->mx = objects[SHIP_IDX].mx + finecosine[angle] * PLASMA_SPEED;
					obj->my = objects[SHIP_IDX].my - finesine[angle] * PLASMA_SPEED;
					obj->ticks = PLASMA_TICKS + 1;
					obj->angle = objects[SHIP_IDX].angle >> 24;
					fire_cooldown = FIRE_COOLDOWN;
					S_StartSound(NULL, snd_plasma);
					// remove ammo
					if(hardcore_mode)
					{
						score_dec();
					}
					break;
				}
			}
		}
	}

	// alien stuff
	if(objects[ALIEN_IDX].color)
	{
		if(!objects[APLASMA_IDX].color && objects[SHIP_IDX].color)
		{
			// FIRE!
			uint32_t angle = objs_angle(objects + ALIEN_IDX, objects + SHIP_IDX) >> ANGLETOFINESHIFT;
			objects[APLASMA_IDX].color = COLOR_APLASMA;
			objects[APLASMA_IDX].x = objects[ALIEN_IDX].x;
			objects[APLASMA_IDX].y = objects[ALIEN_IDX].y;
			objects[APLASMA_IDX].mx = finecosine[angle] * APLASMA_SPEED;
			objects[APLASMA_IDX].my = finesine[angle] * APLASMA_SPEED;
			objects[APLASMA_IDX].ticks = APLASMA_TICKS + 1;
			objects[APLASMA_IDX].angle = angle >> (24 - ANGLETOFINESHIFT);
			sound_cooldown = SOUND_COOLDOWN_BOOM;
			S_StartSound(playermo, snd_alien);
		}

		// plasma / alien collisions
		for(object_t *obj = objects + PLASMA_FIRST; obj < objects + PLASMA_END; obj++)
		{
			if(!obj->color)
				continue;

			if(collision_check_full(obj, objects + ALIEN_IDX))
			{
				// collision
				rocks_to_alien = 28 + (random_byte() & 7);
				rock_cooldown = 15;
				objects[ALIEN_IDX].color = 0;
				obj->color = 0;
				score_add(3, 1);
				sound_cooldown = SOUND_COOLDOWN_BOOM;
				S_StartSound(playermo, sfx_barexp);

				for(uint32_t i = 0; i < 8; i++)
				{
					object_t *obj;
					obj = spawn_spark(COLOR_SPARK_Y, TYPE_SPARK_8, 16 + (random_byte() & 7));
					obj->x = objects[ALIEN_IDX].x + (random_byte() << 15) - (64 << FRACBITS);
					obj->y = objects[ALIEN_IDX].y + (random_byte() << 15) - (64 << FRACBITS);
					obj->angle = random_byte() << 24;
					obj->mx = finecosine[obj->angle >> ANGLETOFINESHIFT] * SPARK_SPEED;
					obj->my = -finesine[obj->angle >> ANGLETOFINESHIFT] * SPARK_SPEED;
					obj->mx += objects[ALIEN_IDX].mx / 2;
					obj->my += objects[ALIEN_IDX].my / 2;
				}
			}
		}
	}

	// ship / alien plasma collision
	if(objects[APLASMA_IDX].color > 1 && objects[SHIP_IDX].color)
	{
		if(collision_check_full(objects + APLASMA_IDX, objects + SHIP_IDX))
		{
			// collision
			objects[APLASMA_IDX].color = 0;
			objects[SHIP_IDX].color = 0;
			destroy_ship();
			sound_cooldown = SOUND_COOLDOWN_BOOM;
			S_StartSound(playermo, sfx_barexp);
		}
	}

	// ship / rock collisions
	if(objects[SHIP_IDX].color)
	for(object_t *obj = objects + ROCK_FIRST; obj < objects + ROCK_END; obj++)
	{
		if(!obj->color)
			continue;

		if(collision_check_full(obj, objects + SHIP_IDX))
		{
			// collision
			destroy_rock(obj, objects + SHIP_IDX);
			obj->color = 0;
			objects[SHIP_IDX].color = 0;
			destroy_ship();
			sound_cooldown = SOUND_COOLDOWN_BOOM;
			S_StartSound(playermo, sfx_barexp);
			break;
		}
	}

	// plasma / rock collisions
	for(object_t *obj = objects + PLASMA_FIRST; obj < objects + PLASMA_END; obj++)
	{
		if(!obj->color)
			continue;

		for(object_t *obk = objects + ROCK_FIRST; obk < objects + ROCK_END; obk++)
		{
			if(!obk->color)
				continue;

			if(collision_check_full(obj, obk))
			{
				// collision
				destroy_rock(obk, obj);
				obj->color = 0;
				obk->color = 0;
				score_add(2, 1);
				sound_cooldown = SOUND_COOLDOWN_BOOM;
				S_StartSound(playermo, sfx_barexp);
				break;
			}
		}
	}

	// rock / rock collisions
	for(object_t *obj = objects + ROCK_FIRST; obj < objects + ROCK_END; obj++)
	{
		if(!obj->color)
			continue;

		for(object_t *obk = objects + ROCK_FIRST; obk < objects + ROCK_END; obk++)
		{
			if(!obk->color)
				continue;

			if(obk == obj)
				continue;

			if(collision_check_full(obj, obk))
			{
				// collision
				destroy_rock(obk, obj);
				destroy_rock(obj, obk);
				obj->color = 0;
				obk->color = 0;
				sound_cooldown = SOUND_COOLDOWN_BOOM;
				S_StartSound(playermo, sfx_barexp);
				break;
			}
		}
	}

	// object movement
	for(object_t *obj = objects; obj < objects + OBJECT_COUNT; obj++)
	{
		if(!obj->color)
			continue;

		if(obj->ticks)
		{
			obj->ticks--;
			if(!obj->ticks)
			{
				obj->color = 0;
				continue;
			}
		}

		obj->x += obj->mx;
		obj->y += obj->my;

		if(obj->type == TYPE_PLASMA)
			obj->angle += 0x20000000;

		// color changes
		if(obj->type == TYPE_SPARK_8)
		{
			if(obj->ticks < 8)
				obj->color++;
		} else
		if(obj->type == TYPE_SPARK_16)
		{
			if(obj->ticks < 16)
				obj->color++;
		}

		while(obj->x > LIMIT_X)
			obj->x -= LIMIT_X * 2;
		while(obj->x < -LIMIT_X)
			obj->x += LIMIT_X * 2;
		while(obj->y > LIMIT_Y)
			obj->y -= LIMIT_Y * 2;
		while(obj->y < -LIMIT_Y)
			obj->y += LIMIT_Y * 2;
	}

	// alien ship
	if(alien_cooldown)
	{
		if(rock_count < ROCK_ALIEN_LIMIT)
		{
			alien_cooldown--;
			if(!alien_cooldown)
			{
				uint32_t tmp;

				objects[ALIEN_IDX].color = COLOR_ALIEN;
				objects[ALIEN_IDX].type = TYPE_ALIEN;
				objects[ALIEN_IDX].radius = RADIUS_ALIEN;

				tmp = random_byte();
				if(tmp & 1)
					objects[ALIEN_IDX].x = objects[SHIP_IDX].x + LIMIT_X;
				else
					objects[ALIEN_IDX].x = objects[SHIP_IDX].x - LIMIT_X;
				if(tmp & 2)
					objects[ALIEN_IDX].y = objects[SHIP_IDX].y + LIMIT_Y / 2;
				else
					objects[ALIEN_IDX].y = objects[SHIP_IDX].y - LIMIT_Y / 2;

				if(tmp & 4)
					objects[ALIEN_IDX].mx = ALIEN_VEL;
				else
					objects[ALIEN_IDX].mx = -ALIEN_VEL;

				if(tmp & 8)
				{
					if(tmp & 16)
						objects[ALIEN_IDX].my = ALIEN_VEL / 4;
					else
						objects[ALIEN_IDX].my = -ALIEN_VEL / 4;
				}

				// fake plasma shot to delay actual shooting
				objects[APLASMA_IDX].color = 1; // a hack to make it inert
				objects[APLASMA_IDX].ticks = APLASMA_TICKS + APLASMA_TICKS / 4;
				objects[APLASMA_IDX].x = objects[ALIEN_IDX].x;
				objects[APLASMA_IDX].y = objects[ALIEN_IDX].y;
				objects[APLASMA_IDX].mx = objects[ALIEN_IDX].mx;
				objects[APLASMA_IDX].my = objects[ALIEN_IDX].my;
			}
		}
		// no new rocks
		rock_cooldown = 0;
	}
	// new rocks
	if(rock_cooldown && rock_count < ROCK_LIMIT)
	{
		rock_cooldown--;
		if(!rock_cooldown)
		{
			object_t *rock;
			fixed_t ox, oy;

			oy = random_byte();
			if(oy & 1)
				ox = LIMIT_X;
			else
				ox = -LIMIT_X;
			if(oy & 2)
				oy = LIMIT_Y;
			else
				oy = -LIMIT_Y;

			rock = spawn_rock(objects[SHIP_IDX].x + ox, objects[SHIP_IDX].y + oy, TYPE_ROCK_BIG);
			if(rock)
			{
				// check new location
				for(object_t *obj = objects + ROCK_FIRST; obj < objects + ROCK_END; obj++)
				{
					if(!obj->color)
						continue;

					if(obj == rock)
						continue;

					if(collision_check_full(obj, rock))
					{
						// can't spawn here
						rock->color = 0;
						rock_count--;
						// try again later
						rock_cooldown = 1;
						break;
					}
				}
			} else
				// try again later
				rock_cooldown = 1;
		}
	}
}

__attribute((regparm(2),no_caller_saved_registers))
int game_input(event_t *ev)
{
	randomize += ev->data1;

	if(ev->type == ev_keydown)
	{
		switch(ev->data1)
		{
			case KEY_ESCAPE:
				// exit the game
				S_StartSound(NULL, sfx_pdiehi);
				M_StartControlPanel();
				// uninstall game hooks
				utils_install_hooks(hook_restore);
				// fix map
				*am_height = 200 - 32;
				// free game memory
				Z_Free(objects);
				Z_Free((uint8_t*)ace_segment + 0x1000);
				// done
				return 1;
			break;
			case KEY_UPARROW:
				keys |= KEY_UP;
			break;
			case KEY_DOWNARROW:
				keys |= KEY_DOWN;
			break;
			case KEY_LEFTARROW:
				keys |= KEY_LEFT;
			break;
			case KEY_RIGHTARROW:
				keys |= KEY_RIGHT;
			break;
			case ' ':
				keys |= KEY_FIRE0;
			break;
			case KEY_CTRL:
				keys |= KEY_FIRE1;
			break;
			case KEY_SHIFT:
				keys |= KEY_HARDCORE;
			break;
		}
	} else
	if(ev->type == ev_keyup)
	{
		switch(ev->data1)
		{
			case KEY_UPARROW:
				keys &= ~KEY_UP;
			break;
			case KEY_DOWNARROW:
				keys &= ~KEY_DOWN;
			break;
			case KEY_LEFTARROW:
				keys &= ~KEY_LEFT;
			break;
			case KEY_RIGHTARROW:
				keys &= ~KEY_RIGHT;
			break;
			case ' ':
				keys &= ~KEY_FIRE0;
			break;
			case KEY_CTRL:
				keys &= ~KEY_FIRE1;
			break;
			case KEY_SHIFT:
				keys &= ~KEY_HARDCORE;
			break;
		}
	}
	// always 'eat' the key
	return 1;
}

