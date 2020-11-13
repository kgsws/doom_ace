// kgsws' Doom ACE
// Custom code pointers.
#include "engine.h"
#include "defs.h"
#include "utils.h"
#include "textpars.h"
#include "action.h"
#include "decorate.h"

//#define ARG_PARSE_DEBUG

enum
{
	ARGTYPE_TERMINATOR,
	ARGTYPE_BOOLEAN,
	ARGTYPE_INT32,
	ARGTYPE_ENUM,
	ARGTYPE_FIXED,
	ARGTYPE_ACTOR,
	ARGTYPE_STATE,
	ARGTYPE_FLAGLIST,
	//
	ARGFLAG_OPTIONAL
};

typedef struct
{
	uint8_t *match; // must be lowercase
	uint32_t value;
} argflag_t;

typedef struct
{
	uint8_t type;
	uint8_t flags;
	void *flaglist;
	uint32_t def;
	uint32_t result;
} argtype_t;

//
//

static uint8_t kw_true[] = {'t', 'r', 'u', 'e', 0};
static uint8_t kw_false[] = {'f', 'a', 'l', 's', 'e', 0};

// args: optional boolean, default true
static argtype_t arg_boolean_opt_true[] =
{
	{ARGTYPE_BOOLEAN, ARGFLAG_OPTIONAL, NULL, 1},
	// terminator
	{ARGTYPE_TERMINATOR}
};

//
//

#ifdef ARG_PARSE_DEBUG
static void arg_dump(argtype_t *list)
{
	uint32_t idx = 0;
	while(list->type != ARGTYPE_TERMINATOR)
	{
		doom_printf("arg %u: value %u / %d / 0x%08X\n", idx++, list->result, list->result, list->result);
		list++;
	}
}
#endif

static int parse_args(argtype_t *list, uint8_t *arg, uint8_t *end)
{
	if(!arg)
	{
		if(list->flags & ARGFLAG_OPTIONAL)
		{
set_defaults:
			while(list->type != ARGTYPE_TERMINATOR)
			{
				list->result = list->def;
				list++;
			}
			return 0;
		}
		return 1;
	}

	tp_kw_is_func = 1;

	while(list->type != ARGTYPE_TERMINATOR)
	{
		// skip WS
		arg = tp_skip_wsc(arg, end);
		if(arg == end)
			goto end_eof;

		// parse
		switch(list->type)
		{
			case ARGTYPE_BOOLEAN:
			{
				uint8_t *tmp = tp_ncompare_skip(arg, end, kw_true);
				if(!tmp)
				{
					arg = tp_ncompare_skip(arg, end, kw_false);
					if(!arg)
						goto end_fail;
					list->result = 0;
				} else
				{
					list->result = 1;
					arg = tmp;
				}
			}
			break;
			case ARGTYPE_INT32:
				arg = tp_get_uint(arg, end, &list->result);
				if(arg == end)
					goto end_fail;
				if(!arg)
					goto end_fail;
			break;
			case ARGTYPE_ENUM:
				// TODO
			break;
			case ARGTYPE_ACTOR:
			{
				uint8_t *tmp = tp_get_string(arg, end);
				if(!tmp)
					goto end_fail;
				list->result = decorate_get_actor(tp_clean_string(arg));
				arg = tmp;
			}
			break;
			case ARGTYPE_FIXED:
				arg = tp_get_fixed(arg, end, &list->result);
				if(arg == end)
					goto end_fail;
				if(!arg)
					goto end_fail;
			break;
			case ARGTYPE_STATE:
				if(*arg == '"')
				{
					uint8_t *txt;
					uint8_t *tmp = tp_get_string(arg, end);
					if(!tmp)
						goto end_fail;

					txt = tp_clean_string(arg);

					if(*txt == '_')
						list->result = decorate_custom_state_find(txt + 1, tmp);
					else
						list->result = decorate_animation_state_find(txt, tmp);

					arg = tmp;
				} else
				{
					arg = tp_get_uint(arg, end, &list->result);
					if(!arg)
						goto end_fail;
					if(!list->result)
						goto end_fail;
				}
			break;
			case ARGTYPE_FLAGLIST:
				list->result = 0;
				while(1)
				{

					argflag_t *flag = list->flaglist + ace_segment;
					while(flag->match)
					{
						uint8_t *tmp = tp_ncompare_skip(arg, end, flag->match + ace_segment);
						if(tmp)
						{
							arg = tmp;
							break;
						}
						flag++;
					}
					if(!flag->match)
						goto end_fail;
					list->result |= flag->value;

					// skip WS
					arg = tp_skip_wsc(arg, end);
					if(arg == end)
						goto end_eof;

					// check for next flag
					if(*arg != '|')
						break;
					arg++;
					if(arg == end)
						goto end_eof;

					// skip WS
					arg = tp_skip_wsc(arg, end);
					if(arg == end)
						goto end_eof;
				}
			break;
		}

		// skip WS
		arg = tp_skip_wsc(arg, end);
		if(arg == end)
			goto end_eof;

		list++;

		if(*arg != ',')
		{
			if(list->type != ARGTYPE_TERMINATOR)
				goto end_eof;
		} else
		{
			if(list->type == ARGTYPE_TERMINATOR)
				return 1;
			arg++;
		}
	}

	tp_kw_is_func = 0;

	return 0;

end_eof:
	tp_kw_is_func = 0;

	if(list->flags & ARGFLAG_OPTIONAL)
		goto set_defaults;

	return 1;

end_fail:
	tp_kw_is_func = 0;
	return 1;
}

//
// helpers

__attribute((regparm(2),no_caller_saved_registers))
static void missile_stuff(mobj_t *mo, mobj_t *source, mobj_t *target, angle_t angle)
{
	if(mo->info->seesound)
		S_StartSound(mo, mo->info->seesound);

	mo->target = source;
	if(mo->flags & MF_SEEKERMISSILE)
		mo->tracer = target;
	mo->angle = angle;

	angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[angle]);

	if(mo->flags & MF_RANDOMIZE && mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	mo->x += mo->momx >> 1;
	mo->y += mo->momy >> 1;
	mo->z += mo->momz >> 1;

	if(!P_TryMove(mo, mo->x, mo->y))
	{
		if(mo->flags & MF_SHOOTABLE)
			P_DamageMobj(mo, NULL, NULL, 10000);
		else
			P_ExplodeMissile(mo);
	}
}

//
// A_DoNothing

void A_DoNothing(mobj_t *mo)
{
	// yep ...
}

//
// A_NoBlocking

void *arg_NoBlocking(void *func, uint8_t *arg, uint8_t *end)
{
	if(parse_args(arg_boolean_opt_true, arg, end))
		return NULL;
	if(!arg_boolean_opt_true[0].result)
		func_extra_data = (void*)1;
	return ACTFIX_NoBlocking;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_NoBlocking(mobj_t *mo)
{
	arg_droplist_t *drop;
	mobj_t *item;

	// make non-solid
	mo->flags &= ~MF_SOLID;

	// drop items
	drop = mo->state->extra;
	if(!drop)
		return;

	for(uint32_t i = 0; i < drop->count+1; i++)
	{
		uint32_t tc = drop->typecombo[i];
		if(P_Random() > (tc >> 24))
			continue;
		item = P_SpawnMobj(mo->x, mo->y, mo->z + (8 << FRACBITS), tc & 0xFFFFFF);
		item->flags |= MF_DROPPED;
		item->angle = P_Random() << 24;
		item->momx = 2*FRACUNIT - (P_Random() << 9);
		item->momy = 2*FRACUNIT - (P_Random() << 9);
		item->momz = (3 << 15) + 2*(P_Random() << 9);
	}
}

//
// A_SpawnProjectile

typedef struct
{
	uint32_t actor;
	fixed_t height;
	fixed_t offset;
	fixed_t angle;
	uint32_t flags;
	fixed_t pitch;
} __attribute__((packed)) arg_spawn_projectile_t;

#define CMF_AIMOFFSET	1
#define CMF_AIMDIRECTION	2
#define CMF_TRACKOWNER	4
#define CMF_CHECKTARGETDEAD	8
#define CMF_ABSOLUTEPITCH	16
#define CMF_OFFSETPITCH	32
#define CMF_SAVEPITCH	64
#define CMF_ABSOLUTEANGLE	128
static argflag_t arg_spawn_projectile_flags[] =
{
	{"cmf_aimoffset", CMF_AIMOFFSET},
	{"cmf_aimdirection", CMF_AIMDIRECTION},
	{"cmf_trackowner", CMF_TRACKOWNER},
	{"cmf_checktargetdead", CMF_CHECKTARGETDEAD},
	{"cmf_absolutepitch", CMF_ABSOLUTEPITCH},
	{"cmf_offsetpitch", CMF_OFFSETPITCH},
	{"cmf_savepitch", CMF_SAVEPITCH},	// TODO
	{"cmf_absoluteangle", CMF_ABSOLUTEANGLE},
	// terminator
	{NULL}
};

static argtype_t arg_spawn_projectile[] =
{
	{ARGTYPE_ACTOR, 0, NULL},
	{ARGTYPE_FIXED, ARGFLAG_OPTIONAL, NULL, 32 << FRACBITS},
	{ARGTYPE_FIXED, ARGFLAG_OPTIONAL, NULL, 0},
	{ARGTYPE_FIXED, ARGFLAG_OPTIONAL, NULL, 0},
	{ARGTYPE_FLAGLIST, ARGFLAG_OPTIONAL, arg_spawn_projectile_flags, 0},
	{ARGTYPE_FIXED, ARGFLAG_OPTIONAL, NULL, 0},
	{ARGTYPE_ENUM, ARGFLAG_OPTIONAL, NULL, 0},
	// terminator
	{ARGTYPE_TERMINATOR}
};

void *arg_SpawnProjectile(void *func, uint8_t *arg, uint8_t *end)
{
	arg_spawn_projectile_t *info;

	if(parse_args(arg_spawn_projectile, arg, end))
		return NULL;

	if(arg_spawn_projectile[0].result == (uint32_t)-1)
		return A_DoNothing;

	info = decorate_get_storage(sizeof(arg_spawn_projectile_t));
	info->actor = arg_spawn_projectile[0].result;
	info->height = arg_spawn_projectile[1].result;
	info->offset = arg_spawn_projectile[2].result;
	info->angle = arg_spawn_projectile[3].result;
	info->flags = arg_spawn_projectile[4].result;
	info->pitch = arg_spawn_projectile[5].result; // TODO

	if(info->angle >= 360 << FRACBITS)
		info->angle = 0;
	info->angle = (11930464 * (uint64_t)info->angle) >> 16;

	func_extra_data = info;

	return A_SpawnProjectile;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_SpawnProjectile(mobj_t *mo)
{
	angle_t angle = 0;
	fixed_t pitch = 0;
	angle_t aaa;
	int dist = 0;
	fixed_t x, y, z;
	mobj_t *item, *target;
	arg_spawn_projectile_t *info = mo->state->extra;

	x = mo->x;
	y = mo->y;
	z = mo->z;
	aaa = mo->angle;

	if(info->flags & CMF_TRACKOWNER && mo->flags & MF_MISSILE)
		mo = mo->target;

	target = mo->target;

	if(!target)
		return;

	if(info->flags & CMF_CHECKTARGETDEAD && mo->health <= 0)
		return;

	if(info->flags & CMF_AIMOFFSET)
	{
		angle = R_PointToAngle2(x, y, target->x, target->y);
		dist = P_AproxDistance(target->x - x, target->y - y);
	}

	if(info->offset)
	{
		angle_t a = aaa >> ANGLETOFINESHIFT;
		x += FixedMul(info->offset, finesine[a]);
		y += FixedMul(info->offset, finecosine[a]);
	}

	if((info->flags & (CMF_AIMOFFSET|CMF_AIMDIRECTION)) == 0)
	{
		angle = R_PointToAngle2(x, y, target->x, target->y);
		dist = P_AproxDistance(target->x - x, target->y - y);
	}

	if(info->flags & CMF_ABSOLUTEANGLE)
		angle = info->angle;
	else
	{
		angle += info->angle;
		if(target->flags & MF_SHADOW)
			angle += (P_Random() - P_Random()) << 20;
	}

	item = P_SpawnMobj(x, y, z + info->height, info->actor);

	if(info->flags & (CMF_AIMDIRECTION|CMF_ABSOLUTEPITCH))
		item->momz = -info->pitch;
	else
	if(item->info->speed)
	{
		dist /= item->info->speed;
		if(dist <= 0)
			dist = 1;
		item->momz = (target->z - z) / dist;
		if(info->flags & CMF_OFFSETPITCH)
			item->momz -= pitch;
	}

	missile_stuff(item, mo, target, angle);
}

//
// A_JumpIfCloser

static argtype_t arg_jump_distance[] =
{
	{ARGTYPE_FIXED, 0, NULL},
	{ARGTYPE_STATE, 0, NULL},
	{ARGTYPE_BOOLEAN, ARGFLAG_OPTIONAL, NULL, 0},
	// terminator
	{ARGTYPE_TERMINATOR}
};

void *arg_JumpIfCloser(void *func, uint8_t *arg, uint8_t *end)
{
	arg_jump_distance_t *info;

	if(parse_args(arg_jump_distance, arg, end))
		return NULL;

	info = decorate_get_storage(sizeof(arg_jump_distance_t));
	info->distance = arg_jump_distance[0].result;
	info->state = arg_jump_distance[1].result;
	info->noz = arg_jump_distance[2].result;

	func_extra_data = info;

	return ACTFIX_JumpIfCloser;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfCloser(mobj_t *mo)
{
	fixed_t distance;
	arg_jump_distance_t *info = mo->state->extra;

	distance = P_AproxDistance(mo->target->x - mo->x, mo->target->y - mo->y);
	if(!info->noz)
		distance = P_AproxDistance(mo->target->z - mo->z, distance);

	if(distance >= info->distance)
		return;

	P_SetMobjState(mo, info->state);
}

//
// A_Jump

static argtype_t arg_jump_random[] =
{
	{ARGTYPE_INT32, 0, NULL},
	{ARGTYPE_STATE, 0, NULL},
	{ARGTYPE_STATE, ARGFLAG_OPTIONAL, NULL, 0xFFFFFFFF},
	{ARGTYPE_STATE, ARGFLAG_OPTIONAL, NULL, 0xFFFFFFFF},
	{ARGTYPE_STATE, ARGFLAG_OPTIONAL, NULL, 0xFFFFFFFF},
	{ARGTYPE_STATE, ARGFLAG_OPTIONAL, NULL, 0xFFFFFFFF},
	{ARGTYPE_STATE, ARGFLAG_OPTIONAL, NULL, 0xFFFFFFFF},
	{ARGTYPE_STATE, ARGFLAG_OPTIONAL, NULL, 0xFFFFFFFF},
	{ARGTYPE_STATE, ARGFLAG_OPTIONAL, NULL, 0xFFFFFFFF},
	// terminator
	{ARGTYPE_TERMINATOR}
};

void *arg_Jump(void *func, uint8_t *arg, uint8_t *end)
{
	int i;
	arg_jump_random_t *info;

	if(parse_args(arg_jump_random, arg, end))
		return NULL;

	info = decorate_get_storage(sizeof(arg_jump_random_t));
	info->count = 0;
	info->chance = arg_jump_random[0].result;
	for(i = 0; i < 8; i++)
	{
		uint32_t st = arg_jump_random[i+1].result;
		if(st == 0xFFFFFFFF)
			break;
		info->state[i] = st;
		info->count++;
	}

	func_extra_data = info;

	return ACTFIX_Jump;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_Jump(mobj_t *mo)
{
	arg_jump_random_t *info = mo->state->extra;
	uint32_t idx;
	uint8_t ran = P_Random();

	if(ran >= info->chance)
		return;

	idx = P_Random() % info->count;

	P_SetMobjState(mo, info->state[idx]);
}
