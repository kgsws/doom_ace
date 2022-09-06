// kgsws' ACE Engine
////
// Action functions for DECORATE.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "dehacked.h"
#include "decorate.h"
#include "action.h"
#include "mobj.h"
#include "weapon.h"
#include "sound.h"
#include "textpars.h"

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
// common

static const dec_arg_flag_t flags_empty[] =
{
	// just terminator, these flags are not supported
	{NULL}
};

//
// argument parsers

static uint8_t *handle_mobjtype(uint8_t *kw, const dec_arg_t *arg)
{
	int32_t type;

	type = mobj_check_type(tp_hash64(kw));
	if(type < 0)
		type = MOBJ_IDX_UNKNOWN;

	*((uint16_t*)(parse_action_arg + arg->offset)) = type;

	return tp_get_keyword();
}

static uint8_t *handle_u16(uint8_t *kw, const dec_arg_t *arg)
{
	int32_t tmp;

	if(doom_sscanf(kw, "%d", &tmp) != 1 || tmp < 0 || tmp > 65535)
		I_Error("[DECORATE] Unable to parse uint16_t '%s' for action '%s' in '%s'!", kw, action_name, parse_actor_name);

	*((uint16_t*)(parse_action_arg + arg->offset)) = tmp;

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
		I_Error("[DECORATE] Unable to parse fixed_t '%s' for action '%s' in '%s'!", kw, action_name, parse_actor_name);

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
	const dec_arg_flag_t *flag;

	while(1)
	{
		if(kw[0] != '0')
		{
			flag = arg->flags;
			while(flag->name)
			{
				if(!strcmp(flag->name, kw))
					break;
				flag++;
			}

			if(!flag->name)
				I_Error("[DECORATE] Unknown flag '%s' for action '%s' in '%s'!", kw, action_name, parse_actor_name);

			*((uint32_t*)(parse_action_arg + arg->offset)) |= flag->bits;
		}

		kw = tp_get_keyword(kw, arg);
		if(!kw)
			return NULL;

		if(kw[0] == ',' || kw[0] == ')')
			return kw;

		if(kw[0] != '|')
			I_Error("[DECORATE] Unable to parse flags for action '%s' in '%s'!", action_name, parse_actor_name);

		kw = tp_get_keyword(kw, arg);
		if(!kw)
			return NULL;
	}
}

//
// projectile spawn

static void missile_stuff(mobj_t *mo, mobj_t *source, mobj_t *target, angle_t angle)
{
	S_StartSound(mo, mo->info->seesound);

	mo->target = source;
	if(mo->flags1 & MF1_SEEKERMISSILE)
		mo->tracer = target;
	mo->angle = angle;

	angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(mo->info->speed, finecosine[angle]);
	mo->momy = FixedMul(mo->info->speed, finesine[angle]);

	if(mo->flags1 & MF1_RANDOMIZE && mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	// this is an extra addition
	// projectile spawn can be called in pickup function,
	// that creates nested 'P_TryMove' call
	// which breaks basicaly everything
	// so let's avoid that
	if(source->custom_inventory)
		return;

	mo->x += mo->momx >> 1;
	mo->y += mo->momy >> 1;
	mo->z += mo->momz >> 1;

	if(!P_TryMove(mo, mo->x, mo->y))
	{
		if(mo->flags1 & MF_SHOOTABLE)
			P_DamageMobj(mo, NULL, NULL, 100000);
		else
			explode_missile(mo);
	}
}

//
// weapon (logic)

const args_singleFixed_t def_LowerRaise =
{
	.value = 6 * FRACUNIT
};

static const dec_args_t args_LowerRaise =
{
	.size = sizeof(args_singleFixed_t),
	.def = &def_LowerRaise,
	.arg =
	{
		{"speed", handle_fixed, offsetof(args_singleFixed_t, value), 1},
		// terminator
		{NULL}
	}
};

__attribute((regparm(2),no_caller_saved_registers))
void A_Lower(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl;
	const args_singleFixed_t *arg;

	if(!mo->player)
		return;

	pl = mo->player;
	arg = st->arg;

	pl->weapon_ready = 0;
	pl->psprites[0].sy += arg->value;

	if(pl->psprites[0].sy < WEAPONBOTTOM)
		return;

	pl->psprites[0].sy = WEAPONBOTTOM;

	if(pl->playerstate == PST_DEAD)
	{
		if(pl->readyweapon->st_weapon.deadlow)
			stfunc(mo, pl->readyweapon->st_weapon.deadlow);
		return;
	}

	if(!pl->health)
	{
		// Voodoo Doll ...
		for(uint32_t i = 0; i < NUMPSPRITES; i++)
			pl->psprites[i].state = NULL;
		return;
	}

	pl->readyweapon = pl->pendingweapon;
	pl->pendingweapon = NULL;

	if(!pl->readyweapon)
		return;

	S_StartSound(pl->mo, pl->readyweapon->weapon.sound_up);

	stfunc(mo, pl->readyweapon->st_weapon.raise);
}

__attribute((regparm(2),no_caller_saved_registers))
void A_Raise(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl;
	const args_singleFixed_t *arg;

	if(!mo->player)
		return;

	pl = mo->player;
	arg = st->arg;

	pl->weapon_ready = 0;
	pl->psprites[0].sy -= arg->value;

	if(pl->psprites[0].sy > WEAPONTOP)
		return;

	pl->psprites[0].sy = WEAPONTOP;

	stfunc(mo, pl->readyweapon->st_weapon.ready);
}

//
// weapon (light)

__attribute((regparm(2),no_caller_saved_registers))
void A_Light0(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->player)
		return;
	mo->player->extralight = 0;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_Light1(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->player)
		return;
	mo->player->extralight = 1;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_Light2(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->player)
		return;
	mo->player->extralight = 2;
}

//
// weapon (attack)

__attribute((regparm(2),no_caller_saved_registers))
void A_WeaponReady(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl;
	pspdef_t *psp;

	if(!mo->player)
		return;

	pl = mo->player;
	psp = pl->psprites;

	// TODO: mobj animation (not shooting)

	// sound
	if(	pl->readyweapon->weapon.sound_ready &&
		st == states + pl->readyweapon->st_weapon.ready
	)
		S_StartSound(pl->mo, pl->readyweapon->weapon.sound_ready);

	// new selection
	if(pl->pendingweapon || !pl->health)
	{
		stfunc(mo, pl->readyweapon->st_weapon.lower);
		return;
	}

	// primary attack
	if(pl->cmd.buttons & BT_ATTACK)
	{
		if(weapon_fire(pl, 1, 0))
			return;
	}

	// secondary attack
	if(pl->cmd.buttons & BT_ALTACK)
	{
		if(weapon_fire(pl, 2, 0))
			return;
	}

	// not shooting
	pl->attackdown = 0;

	// enable bob
	pl->weapon_ready = 1;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_ReFire(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl;

	if(!mo->player)
		return;

	pl = mo->player;

	if(	pl->cmd.buttons & (BT_ATTACK | BT_ALTACK) &&
		!pl->pendingweapon &&
		pl->health
	) {
		pl->refire++;
		weapon_fire(pl, pl->attackdown, 1);
		return;
	}

	pl->refire = 0;
	// TODO: ammo check
}

//
// basic sounds	// TODO: boss checks

static __attribute((regparm(2),no_caller_saved_registers))
void A_Pain(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	S_StartSound(mo, mo->info->painsound);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_Scream(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	S_StartSound(mo, mo->info->deathsound);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_XScream(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	S_StartSound(mo, 31);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_ActiveSound(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	S_StartSound(mo, mo->info->activesound);
}

//
// A_FaceTarget

static __attribute((regparm(2),no_caller_saved_registers))
void A_FaceTarget(mobj_t *mo, state_t *st, stfunc_t stfunc)
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
void A_NoBlocking(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mo->flags &= ~MF_SOLID;

	if(mo->info->extra_type == ETYPE_PLAYERPAWN)
		// no item drops for player classes
		return;

	// drop items
	for(mobj_dropitem_t *drop = mo->info->dropitem.start; drop < (mobj_dropitem_t*)mo->info->dropitem.end; drop++)
	{
		mobj_t *item;

		if(drop->chance < 255 && drop->chance > P_Random())
			continue;

		item = P_SpawnMobj(mo->x, mo->y, mo->z + (8 << FRACBITS), drop->type);
		item->flags |= MF_DROPPED;
		item->angle = P_Random() << 24;
		item->momx = 2*FRACUNIT - (P_Random() << 9);
		item->momy = 2*FRACUNIT - (P_Random() << 9);
		item->momz = (3 << 15) + (P_Random() << 10);
	}
}

//
// A_Look

static __attribute((regparm(2),no_caller_saved_registers))
void A_Look(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	doom_A_Look(mo);
}

//
// A_Chase

static __attribute((regparm(2),no_caller_saved_registers))
void A_Chase(mobj_t *mo, state_t *st, stfunc_t stfunc)
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
void A_SpawnProjectile(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	angle_t angle = mo->angle;
	fixed_t pitch = 0;
	int32_t dist = 0;
	angle_t aaa;
	fixed_t x, y, z;
	mobj_t *item, *target;
	const args_SpawnProjectile_t *arg = st->arg;

	x = mo->x;
	y = mo->y;
	z = mo->z + arg->spawnheight;
	aaa = mo->angle;

	if(arg->flags & CMF_TRACKOWNER && mo->flags & MF_MISSILE)
	{
		mo = mo->target;
		if(!mo)
			return;
	}

	if(arg->flags & CMF_AIMDIRECTION)
		target = NULL;
	else
	{
		target = mo->target; // TODO: flags
		if(!target)
			return;
	}

	if(target && arg->flags & CMF_CHECKTARGETDEAD && target->health <= 0)
		return;

	if(target && arg->flags & CMF_AIMOFFSET)
	{
		angle = R_PointToAngle2(x, y, target->x, target->y);
		dist = P_AproxDistance(target->x - x, target->y - y);
	}

	if(arg->spawnofs_xy)
	{
		angle_t a = (aaa - ANG90) >> ANGLETOFINESHIFT;
		x += FixedMul(arg->spawnofs_xy, finecosine[a]);
		y += FixedMul(arg->spawnofs_xy, finesine[a]);
	}

	if(!(arg->flags & (CMF_AIMOFFSET|CMF_AIMDIRECTION)))
	{
		angle = R_PointToAngle2(x, y, target->x, target->y);
		dist = P_AproxDistance(target->x - x, target->y - y);
	}

	if(arg->flags & CMF_ABSOLUTEANGLE)
		angle = arg->angle;
	else
	{
		angle += arg->angle;
		if(target && target->flags & MF_SHADOW)
			angle += (P_Random() - P_Random()) << 20;
	}

	item = P_SpawnMobj(x, y, z, arg->missiletype);

	if(arg->flags & (CMF_AIMDIRECTION|CMF_ABSOLUTEPITCH))
		item->momz = -arg->pitch;
	else
	if(item->info->speed)
	{
		dist /= item->info->speed;
		if(dist <= 0)
			dist = 1;
		item->momz = ((target->z + 32 * FRACUNIT) - z) / dist; // TODO: maybe aim for the middle?
		if(arg->flags & CMF_OFFSETPITCH)
			item->momz -= pitch;
	}

	missile_stuff(item, mo, target, angle);
}

//
// A_GiveInventory

static const dec_args_t args_GiveInventory =
{
	.size = sizeof(args_GiveInventory_t),
	.arg =
	{
		{"type", handle_mobjtype, offsetof(args_GiveInventory_t, type)},
		{"amount", handle_u16, offsetof(args_GiveInventory_t, amount), 1},
//		{"ptr", handle_pointer, offsetof(args_GiveInventory_t, ptr), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_GiveInventory(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_GiveInventory_t *arg = st->arg;
	mobj_give_inventory(mo, arg->type, arg->amount);
}

//
// A_SetAngle

static const dec_args_t args_SetAngle =
{
	.size = sizeof(args_SetAngle_t),
	.arg =
	{
		{"angle", handle_angle, offsetof(args_SetAngle_t, angle)},
		{"flags", handle_flags, offsetof(args_SetAngle_t, flags), 1, flags_empty},
//		{"ptr", handle_pointer, offsetof(args_SetAngle_t, ptr), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_SetAngle(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_SetAngle_t *arg = st->arg;
	mo->angle = arg->angle;
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
	// weapon
	{"a_lower", A_Lower, &args_LowerRaise},
	{"a_raise", A_Raise, &args_LowerRaise},
	{"a_light0", A_Light0},
	{"a_light1", A_Light1},
	{"a_light2", A_Light2},
	{"a_weaponready", A_WeaponReady},
	{"a_refire", A_ReFire},
	// basic sounds
	{"a_pain", A_Pain},
	{"a_scream", A_Scream},
	{"a_xscream", A_XScream},
	{"a_activesound", A_ActiveSound},
	// basic control
	{"a_facetarget", A_FaceTarget},
	{"a_noblocking", A_NoBlocking},
	// "AI"
	{"a_look", A_Look},
	{"a_chase", A_Chase},
	// enemy attack
	{"a_spawnprojectile", A_SpawnProjectile, &args_SpawnProjectile},
	// misc
	{"a_setangle", A_SetAngle, &args_SetAngle},
	// inventory
	{"a_giveinventory", A_GiveInventory, &args_GiveInventory},
	// terminator
	{NULL}
};

