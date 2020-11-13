// kgsws' Doom ACE
// Custom code pointers.
#include "engine.h"
#include "defs.h"
#include "textpars.h"
#include "action.h"
#include "decorate.h"

//#define ARG_PARSE_DEBUG

enum
{
	ARGTYPE_TERMINATOR,
	ARGTYPE_BOOLEAN,
	ARGTYPE_INT32, // TODO
	ARGTYPE_FIXED,
	ARGTYPE_ACTOR,
	ARGTYPE_STATE,
	ARGTYPE_FLAGLIST, // TODO
	//
	ARGFLAG_OPTIONAL
};

typedef struct
{
	uint8_t *match;
	uint32_t value;
} argflag_t;

typedef struct
{
	uint8_t type;
	uint8_t flags;
	argflag_t *flaglist;
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
	uint8_t *tmp;

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
				tmp = tp_ncompare_skip(arg, end, kw_true);
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
			break;
			case ARGTYPE_ACTOR:
				tmp = tp_get_string(arg, end);
				if(!tmp)
					goto end_fail;
				list->result = decorate_get_actor(tp_clean_string(arg));
				arg = tmp;
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
					tmp = tp_get_string(arg, end);
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
	int dist;

	if(mo->info->seesound)
		S_StartSound(mo, mo->info->seesound);

	mo->target = source;
	mo->tracer = target;
	mo->angle = angle;

	angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[angle]);

	dist = P_AproxDistance(target->x - source->x, target->y - source->y);
	dist /= mo->info->speed;
	if(dist < 0)
		dist = 1;

	mo->momz = (target->z - source->z) / dist;

	if(mo->tics > 0)
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
	return A_NoBlocking;
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
} __attribute__((packed)) arg_spawn_projectile_t;

static argtype_t arg_spawn_projectile[] =
{
	{ARGTYPE_ACTOR, 0, NULL},
	{ARGTYPE_FIXED, ARGFLAG_OPTIONAL, NULL, 32 << FRACBITS},
	{ARGTYPE_FIXED, ARGFLAG_OPTIONAL, NULL, 0},
	{ARGTYPE_FIXED, ARGFLAG_OPTIONAL, NULL, 0},
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

	func_extra_data = info;

	return A_SpawnProjectile;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_SpawnProjectile(mobj_t *mo)
{
	angle_t angle;
	fixed_t x, y;
	mobj_t *item, *target;
	arg_spawn_projectile_t *info = mo->state->extra;

	target = mo->target;

	x = mo->x;
	y = mo->y;
	if(info->offset)
	{
		angle_t a = mo->angle >> ANGLETOFINESHIFT;
		x += FixedMul(info->offset, finesine[a]);
		y += FixedMul(info->offset, finecosine[a]);
	}

	angle = R_PointToAngle2(x, y, target->x, target->y);

	angle += info->angle;

	if(target->flags & MF_SHADOW)
		angle += (P_Random() - P_Random()) << 20;

	item = P_SpawnMobj(x, y, mo->z + info->height, info->actor);
	missile_stuff(item, mo, mo->target, angle);
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

	info = decorate_get_storage(sizeof(arg_spawn_projectile_t));
	info->distance = arg_jump_distance[0].result;
	info->state = arg_jump_distance[1].result;
	info->noz = arg_jump_distance[2].result;

	func_extra_data = info;

	return A_JumpIfCloser;
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

