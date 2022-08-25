// kgsws' ACE Engine
////
// Action functions for DECORATE.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "dehacked.h"
#include "decorate.h"
#include "action.h"
#include "textpars.h"
#include "sound.h"

#define MAKE_FLAG(x)	{.name = #x, .bits = x}

typedef struct
{
	const uint8_t *name;
	uint32_t bits;
} dec_arg_flag_t;

typedef struct dec_arg_s
{
	const uint8_t *name;
	uint8_t *(*handler)(uint8_t*,const struct dec_arg_s*);
	uint16_t offset;
	uint16_t optional;
	const dec_arg_flag_t *flags;
} dec_arg_t;

typedef struct
{
	uint32_t size;
	const void *def;
	dec_arg_t arg[];
} dec_args_t;

typedef struct
{
	const uint8_t *name;
	void *func;
	const dec_args_t *args;
} dec_action_t;

//

static const uint8_t *action_name;

void *parse_action_func;
void *parse_action_arg;

static const dec_action_t mobj_action[];

//
// argument parsers

static uint8_t *handle_mobjtype(uint8_t *kw, const dec_arg_t *arg)
{
	int32_t type;

	type = mobj_check_type(tp_hash64(kw));

	if(type < 0)
		// TODO: 'unknown thing'
		type = 29;

	*((uint16_t*)(parse_action_arg + arg->offset)) = type;

	return tp_get_keyword();
}

static uint8_t *handle_fixed(uint8_t *kw, const dec_arg_t *arg)
{
	fixed_t value;
	uint_fast8_t negate;

	if(kw[0] == '-')
	{
		kw = tp_get_keyword();
		if(!kw)
			return NULL;
		negate = 1;
	} else
		negate = 0;

	if(tp_parse_fixed(kw, &value))
		I_Error("[DECORATE] Unable to parse fixed_t '%s' for action '%s' in '%s'!", arg->name, action_name, parse_actor_name);

	if(negate)
		value = -value;

	*((fixed_t*)(parse_action_arg + arg->offset)) = value;

	return tp_get_keyword();
}

static uint8_t *handle_angle(uint8_t *kw, const dec_arg_t *arg)
{
	kw = handle_fixed(kw, arg);

	if(kw)
	{
		fixed_t value = *((fixed_t*)(parse_action_arg + arg->offset));
		value = (11930464 * (int64_t)value) >> 16;
		*((fixed_t*)(parse_action_arg + arg->offset)) = value;
	}

	return kw;
}

static uint8_t *handle_flags(uint8_t *kw, const dec_arg_t *arg)
{
	I_Error("TODO: handle_flags");
	return NULL;
}

//
// projectile spawn

static void missile_stuff(mobj_t *mo, mobj_t *source, mobj_t *target, angle_t angle)
{
	S_StartSound(mo, mo->info->seesound);

	mo->target = source;
//	if(mo->flags1 & MF1_SEEKERMISSILE)
		mo->tracer = target;
	mo->angle = angle;

	angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[angle]);

	if(/*mo->flags1 & MF1_RANDOMIZE &&*/ mo->tics > 0)
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
		if(mo->flags1 & MF_SHOOTABLE)
			P_DamageMobj(mo, NULL, NULL, 0x100000);
		else
			explode_missile(mo);
	}
}

//
// basic sounds

static __attribute((regparm(2),no_caller_saved_registers))
void A_Pain(mobj_t *mo)
{
	// TODO: boss check
	S_StartSound(mo, mo->info->painsound);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_Scream(mobj_t *mo)
{
	// TODO: boss check
	S_StartSound(mo, mo->info->deathsound);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_XScream(mobj_t *mo)
{
	// TODO: boss check
	S_StartSound(mo, 31);
}

//
// A_FaceTarget

static __attribute((regparm(2),no_caller_saved_registers))
void A_FaceTarget(mobj_t *mo)
{
	if(!mo->target)
		return;

	mo->flags &= ~MF_AMBUSH;
	mo->angle = R_PointToAngle2(mo->x, mo->y, mo->target->x, mo->target->y);

	// TODO: use correct flag
	if(mo->target->flags & MF_SHADOW)
		mo->angle += (P_Random() - P_Random()) << 21;
}

//
// A_NoBlocking

static __attribute((regparm(2),no_caller_saved_registers))
void A_NoBlocking(mobj_t *mo)
{
	mo->flags &= ~MF_SOLID;
	// TODO: drop items
}

//
// A_Look

static __attribute((regparm(2),no_caller_saved_registers))
void A_Look(mobj_t *mo)
{
	doom_A_Look(mo);
}

//
// A_Chase

static __attribute((regparm(2),no_caller_saved_registers))
void A_Chase(mobj_t *mo)
{
	// workaround for non-fixed speed
	fixed_t speed = mo->info->speed;

	mo->info->speed = (speed + (FRACUNIT / 2)) >> FRACBITS;

	doom_A_Chase(mo);

	mo->info->speed = speed;
}

//
// A_SpawnProjectile

static const args_SpawnProjectile_t def_SpawnProjectile =
{
	.spawnheight = 32 * FRACUNIT
};

static const dec_arg_flag_t flags_SpawnProjectile[] =
{
	MAKE_FLAG(CMF_AIMOFFSET),
	MAKE_FLAG(CMF_AIMDIRECTION),
	MAKE_FLAG(CMF_TRACKOWNER),
	MAKE_FLAG(CMF_CHECKTARGETDEAD),
	MAKE_FLAG(CMF_ABSOLUTEPITCH),
	MAKE_FLAG(CMF_OFFSETPITCH),
	MAKE_FLAG(CMF_SAVEPITCH),
	MAKE_FLAG(CMF_ABSOLUTEANGLE),
	// terminator
	{NULL}
};

static const dec_args_t args_SpawnProjectile =
{
	.size = sizeof(args_SpawnProjectile_t),
	.def = &def_SpawnProjectile,
	.arg =
	{
		{"missiletype", handle_mobjtype, offsetof(args_SpawnProjectile_t, missiletype)},
		{"spawnheight", handle_fixed, offsetof(args_SpawnProjectile_t, spawnheight), 1},
		{"spawnofs_xy", handle_fixed, offsetof(args_SpawnProjectile_t, spawnofs_xy), 1},
		{"angle", handle_angle, offsetof(args_SpawnProjectile_t, angle), 1},
		{"flags", handle_flags, offsetof(args_SpawnProjectile_t, flags), 1, flags_SpawnProjectile},
//		{"ptr", handle_pointer, offsetof(args_SpawnProjectile_t, ptr), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_SpawnProjectile(mobj_t *mo)
{
	angle_t angle = 0;
	fixed_t pitch = 0;
	int32_t dist = 0;
	angle_t aaa;
	fixed_t x, y, z;
	mobj_t *item, *target;
	args_SpawnProjectile_t *info = mo->state->arg;

	x = mo->x;
	y = mo->y;
	z = mo->z + info->spawnheight;
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

	if(info->spawnofs_xy)
	{
		angle_t a = (aaa - ANG90) >> ANGLETOFINESHIFT;
		x += FixedMul(info->spawnofs_xy, finecosine[a]);
		y += FixedMul(info->spawnofs_xy, finesine[a]);
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

	item = P_SpawnMobj(x, y, z, info->missiletype);

	if(info->flags & (CMF_AIMDIRECTION|CMF_ABSOLUTEPITCH))
		item->momz = -info->pitch;
	else
	if(item->info->speed)
	{
		dist /= item->info->speed;
		if(dist <= 0)
			dist = 1;
		item->momz = ((target->z + 32 * FRACUNIT) - z) / dist; // TODO: maybe aim for the middle?
		if(info->flags & CMF_OFFSETPITCH)
			item->momz -= pitch;
	}

	missile_stuff(item, mo, target, angle);
}

//
// parser

uint8_t *action_parser(uint8_t *name)
{
	uint8_t *kw;
	const dec_action_t *act = mobj_action;

	// find action
	while(act->name)
	{
		if(!strcmp(act->name, name))
			break;
		act++;
	}
	if(!act->name)
		I_Error("[DECORATE] Unknown action '%s' in '%s'!", name, parse_actor_name);

	// action function
	parse_action_func = act->func;

	// next keyword
	kw = tp_get_keyword();
	if(!kw)
		return NULL;

	if(act->args)
	{
		const dec_arg_t *arg = act->args->arg;

		parse_action_arg = dec_es_alloc(act->args->size);
		if(act->args->def)
			memcpy(parse_action_arg, act->args->def, act->args->size);
		else
			memset(parse_action_arg, 0, act->args->size);

		action_name = act->name;

		if(kw[0] != '(')
		{
			if(!arg->optional)
				I_Error("[DECORATE] Missing argument '%s' for action '%s' in '%s'!", arg->name, act->name, parse_actor_name);
		} else
		{
			while(arg->name)
			{
				kw = tp_get_keyword();
				if(!kw)
					return NULL;

				if(kw[0] == ')')
				{
args_end:
					if(arg->name && !arg->optional)
						I_Error("[DECORATE] Missing argument '%s' for action '%s' in '%s'!", arg->name, act->name, parse_actor_name);
					// return next keyword
					return tp_get_keyword();
				}

				kw = arg->handler(kw, arg);
				if(!kw || (kw[0] != ',' && kw[0] != ')'))
					I_Error("[DECORATE] Failed to parse arguments for action '%s' in '%s'!", act->name, parse_actor_name);

				arg++;

				if(kw[0] == ')')
					goto args_end;

				if(!arg->name)
					I_Error("[DECORATE] Too many arguments for action '%s' in '%s'!", act->name, parse_actor_name);
			}
		}
	}

	return kw;
}

//
// table of actions

static const dec_action_t mobj_action[] =
{
	// basic sounds
	{"a_pain", A_Pain},
	{"a_scream", A_Scream},
	{"a_xscream", A_XScream},
	// basic control
	{"a_facetarget", A_FaceTarget},
	{"a_noblocking", A_NoBlocking},
	// "AI"
	{"a_look", A_Look},
	{"a_chase", A_Chase},
	// enemy attack
	{"a_spawnprojectile", A_SpawnProjectile, &args_SpawnProjectile},
	// terminator
	{NULL}
};

