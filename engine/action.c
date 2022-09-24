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
#include "player.h"
#include "weapon.h"
#include "inventory.h"
#include "sound.h"
#include "stbar.h"
#include "demo.h"
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

mobj_t **linetarget;

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

static uint8_t *handle_sound(uint8_t *kw, const dec_arg_t *arg)
{
	*((uint16_t*)(parse_action_arg + arg->offset)) = sfx_by_alias(tp_hash64(kw));
	return tp_get_keyword();
}

static uint8_t *handle_mobjtype(uint8_t *kw, const dec_arg_t *arg)
{
	int32_t type;

	type = mobj_check_type(tp_hash64(kw));
	if(type < 0)
		type = MOBJ_IDX_UNKNOWN;

	*((uint16_t*)(parse_action_arg + arg->offset)) = type;

	return tp_get_keyword();
}

static uint8_t *handle_bool(uint8_t *kw, const dec_arg_t *arg)
{
	uint32_t tmp;

	if(!strcmp(kw, "true"))
		tmp = 1;
	else
	if(!strcmp(kw, "false"))
		tmp = 0;
	else
	if(doom_sscanf(kw, "%d", &tmp) != 1)
		I_Error("[DECORATE] Unable to parse number '%s' for action '%s' in '%s'!", kw, action_name, parse_actor_name);

	*((uint8_t*)(parse_action_arg + arg->offset)) = !!tmp;

	return tp_get_keyword();
}

static uint8_t *handle_bool_invert(uint8_t *kw, const dec_arg_t *arg)
{
	kw = handle_bool(kw, arg);

	*((uint8_t*)(parse_action_arg + arg->offset)) ^= 1;

	return kw;
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
// slope to angle

static inline uint32_t SlopeDiv(unsigned num, unsigned den)
{
	unsigned ans;

	if(den < 512)
		return SLOPERANGE;

	ans = (num << 3) / (den >> 8);

	return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

static angle_t slope_to_angle(fixed_t slope)
{
	if(slope > 0)
	{
		if(slope < FRACUNIT)
			return tantoangle[SlopeDiv(slope, FRACUNIT)];
		else
			return ANG90 - 1 - tantoangle[SlopeDiv(FRACUNIT, slope)];
	} else
	{
		slope = -slope;
		if(slope < FRACUNIT)
			return -tantoangle[SlopeDiv(slope, FRACUNIT)];
		else
			return ANG270 + tantoangle[SlopeDiv(FRACUNIT, slope)];
	}
}

//
// player aim

static uint32_t player_aim(player_t *pl, angle_t *angle, fixed_t *slope, uint32_t seeker)
{
	// TODO: cachce result for same-tick attacks?
	mobj_t *mo = pl->mo;
	fixed_t sl;
	angle_t an = *angle;

	if(pl->info_flags & PLF_AUTO_AIM)
	{
		// autoaim enabled
		sl = P_AimLineAttack(mo, an, 1024 * FRACUNIT);
		if(!*linetarget)
		{
			an += 1 << 26;
			sl = P_AimLineAttack(mo, an, 1024 * FRACUNIT);

			if(!*linetarget)
			{
				an -= 2 << 26;
				sl = P_AimLineAttack(mo, an, 1024 * FRACUNIT);

				if(!*linetarget)
				{
					*slope = 0;
					return 0;
				}
			}
		}
		*slope = sl;
		*angle = an;
		return 1;
	} else
	{
		// autoaim disabled
		*slope = 0;
		// seeker missile check
		*linetarget = NULL;
		if(seeker)
			P_AimLineAttack(mo, an, 1024 * FRACUNIT);
		return 0;
	}
}

//
// projectile spawn

void missile_stuff(mobj_t *mo, mobj_t *source, mobj_t *target, angle_t angle, angle_t pitch, fixed_t slope)
{
	fixed_t speed = mo->info->speed;

	S_StartSound(mo, mo->info->seesound);

	mo->target = source;
	if(mo->flags1 & MF1_SEEKERMISSILE)
		mo->tracer = target;
	mo->angle = angle;

	if(slope || pitch)
	{
		if(slope)
			pitch += slope_to_angle(slope);
		pitch >>= ANGLETOFINESHIFT;
		mo->momz = FixedMul(speed, finesine[pitch]);
		speed = FixedMul(speed, finecosine[pitch]);
	}

	angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul(speed, finecosine[angle]);
	mo->momy = FixedMul(speed, finesine[angle]);

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
		if(mo->flags & MF_SHOOTABLE)
			P_DamageMobj(mo, NULL, NULL, 100000);
		else
			explode_missile(mo);
	}
}

//
// original weapon attacks
// these are not available for DECORATE
// and so are only used in primary fire

__attribute((regparm(2),no_caller_saved_registers))
void A_OldProjectile(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint16_t count;
	uint16_t proj = (uint32_t)st->arg;
	player_t *pl = mo->player;
	uint16_t ammo = pl->readyweapon->weapon.ammo_type[0];
	fixed_t slope, z;
	angle_t angle, pitch;
	mobj_t *th;

	if(proj == 35) // BFG
		count = dehacked.bfg_cells;
	else
		count = 1;

	if(proj == 34) // plasma
	{
		uint32_t state;
		state = pl->readyweapon->st_weapon.flash;
		state += P_Random() & 1;
		pl->psprites[1].state = states + state;
		pl->psprites[1].tics = 0;
	}

	inventory_take(mo, ammo, count);

	if(*demoplayback == DEMO_OLD)
	{
		P_SpawnPlayerMissile(mo, proj);
		return;
	}

	pitch = 0;
	angle = mo->angle;

	if(!player_aim(pl, &angle, &slope, 0))
		pitch = mo->pitch;

	z = mo->z;
	z += mo->height / 2;
	z += mo->info->player.attack_offs;

	th = P_SpawnMobj(mo->x, mo->y, z, proj);
	missile_stuff(th, mo, NULL, angle, pitch, slope);
}

__attribute((regparm(2),no_caller_saved_registers))
void A_OldBullets(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint16_t count;
	uint16_t sound = (uint32_t)st->arg;
	player_t *pl = mo->player;
	uint16_t ammo = pl->readyweapon->weapon.ammo_type[0];
	uint16_t hs, vs;
	state_t *state;
	angle_t angle;

	if(sound == 4)
		count = 2;
	else
		count = 1;

	if(!inventory_take(mo, ammo, count))
		return;

	state = states + pl->readyweapon->st_weapon.flash;

	switch(sound)
	{
		case 86:
			state += pl->psprites[0].state - &states[52];
		case 1:
			count = 1;
			hs = pl->refire ? 18 : 0;
			vs = 0;
		break;
		case 2:
			count = 7;
			hs = 18;
			vs = 0;
		break;
		default:
			count = 20;
			hs = 19;
			vs = 5;
		break;
	}

	S_StartSound(mo, sound);

	// mobj to 'missile' animation
	if(mo->animation != ANIM_MISSILE && mo->info->state_missile)
		mobj_set_animation(mo, ANIM_MISSILE);

	pl->psprites[1].state = state;
	pl->psprites[1].tics = 0;

	angle = mo->angle;

	if(*demoplayback != DEMO_OLD)
	{
		if(!player_aim(pl, &angle, bulletslope, 0))
			*bulletslope = finetangent[(pl->mo->pitch + ANG90) >> ANGLETOFINESHIFT];
	} else
		P_BulletSlope(mo);

	for(uint32_t i = 0; i < count; i++)
	{
		uint16_t damage;
		angle_t aaa;
		fixed_t sss;

		damage = 5 + 5 * (P_Random() % 3);
		aaa = angle;
		if(hs)
			aaa += (P_Random() - P_Random()) << hs;
		sss = *bulletslope;
		if(vs)
			sss += (P_Random() - P_Random()) << 5;

		P_LineAttack(mo, aaa, MISSILERANGE, sss, damage);
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
	player_t *pl = mo->player;
	const args_singleFixed_t *arg = st->arg;

	if(!pl)
		return;

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

	pl->stbar_update |= STU_WEAPON_NOW;

	S_StartSound(mo, pl->readyweapon->weapon.sound_up);

	stfunc(mo, pl->readyweapon->st_weapon.raise);
}

__attribute((regparm(2),no_caller_saved_registers))
void A_Raise(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	const args_singleFixed_t *arg = st->arg;

	if(!pl)
		return;

	pl->weapon_ready = 0;
	pl->psprites[0].sy -= arg->value;

	if(pl->psprites[0].sy > WEAPONTOP)
		return;

	pl->psprites[0].sy = WEAPONTOP;

	stfunc(mo, pl->readyweapon->st_weapon.ready);
}

__attribute((regparm(2),no_caller_saved_registers))
void A_GunFlash(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t state;
	player_t *pl = mo->player;

	if(!pl)
		return;

	// mobj to 'missile' animation
	if(mo->animation != ANIM_MISSILE && mo->info->state_missile)
		mobj_set_animation(mo, ANIM_MISSILE);

	if(pl->attackdown > 1)
		state = pl->readyweapon->st_weapon.flash_alt;
	else
		state = pl->readyweapon->st_weapon.flash;

	if(!state)
		return;

	// just hope that this is called in 'weapon PSPR'
	pl->psprites[1].state = states + state;
	pl->psprites[1].tics = 0;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_CheckReload(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->player)
		return;
	weapon_check_ammo(mo->player);
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
	player_t *pl = mo->player;
	pspdef_t *psp;

	if(!pl)
		return;

	psp = pl->psprites;

	// mobj animation (not shooting)
	if(mo->animation == ANIM_MELEE || mo->animation == ANIM_MISSILE)
		mobj_set_animation(mo, ANIM_SPAWN);

	// sound
	if(	pl->readyweapon->weapon.sound_ready &&
		st == states + pl->readyweapon->st_weapon.ready
	)
		S_StartSound(mo, pl->readyweapon->weapon.sound_ready);

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
	player_t *pl = mo->player;
	uint32_t btn;

	if(!pl)
		return;

	if(pl->attackdown > 1)
		btn = BT_ALTACK;
	else
		btn = BT_ATTACK;

	if(	pl->cmd.buttons & btn &&
		!pl->pendingweapon &&
		pl->health
	) {
		pl->refire++;
		weapon_fire(pl, pl->attackdown, 1);
		return;
	}

	pl->refire = 0;
	weapon_check_ammo(pl);
}

//
// basic sounds

static __attribute((regparm(2),no_caller_saved_registers))
void A_Pain(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	S_StartSound(mo, mo->info->painsound);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_Scream(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	S_StartSound(mo->flags1 & MF1_BOSS ? NULL : mo, mo->info->deathsound);
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
// A_StartSound

static const dec_arg_flag_t flags_StartSound[] =
{
	MAKE_FLAG(CHAN_BODY), // default, 0
	MAKE_FLAG(CHAN_WEAPON),
	MAKE_FLAG(CHAN_VOICE),
	MAKE_FLAG(CHAN_ITEM),
	// terminator
	{NULL}
};

static const dec_args_t args_StartSound =
{
	.size = sizeof(args_StartSound_t),
	.arg =
	{
		{"sound", handle_sound, offsetof(args_StartSound_t, sound)},
		{"slot", handle_flags, offsetof(args_StartSound_t, slot), 1, flags_StartSound},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_StartSound(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	// NOTE: sound slots are not implemented, yet
	const args_StartSound_t *arg = st->arg;
	S_StartSound(mo, arg->sound);
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
		{"pitch", handle_angle, offsetof(args_SpawnProjectile_t, pitch), 1},
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
	fixed_t x, y, z;
	mobj_t *th, *target;
	const args_SpawnProjectile_t *arg = st->arg;

	x = mo->x;
	y = mo->y;
	z = mo->z + arg->spawnheight;

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
		angle_t a = (mo->angle - ANG90) >> ANGLETOFINESHIFT;
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

	th = P_SpawnMobj(x, y, z, arg->missiletype);

	if(arg->flags & (CMF_AIMDIRECTION|CMF_ABSOLUTEPITCH))
		pitch = -arg->pitch;
	else
	if(th->info->speed)
	{
		dist /= th->info->speed;
		if(dist <= 0)
			dist = 1;
		dist = ((target->z + 32 * FRACUNIT) - z) / dist; // TODO: maybe aim for the middle?
		th->momz = FixedDiv(dist, th->info->speed);
		if(arg->flags & CMF_OFFSETPITCH)
			pitch = -arg->pitch;
	}

	missile_stuff(th, mo, target, angle, pitch, th->momz);
}

//
// A_FireProjectile

static const dec_arg_flag_t flags_FireProjectile[] =
{
	MAKE_FLAG(FPF_AIMATANGLE),
	MAKE_FLAG(FPF_TRANSFERTRANSLATION),
	MAKE_FLAG(FPF_NOAUTOAIM),
	// terminator
	{NULL}
};

static const dec_args_t args_FireProjectile =
{
	.size = sizeof(args_SpawnProjectile_t),
	.arg =
	{
		{"missiletype", handle_mobjtype, offsetof(args_SpawnProjectile_t, missiletype)},
		{"angle", handle_angle, offsetof(args_SpawnProjectile_t, angle), 1},
		{"useammo", handle_bool_invert, offsetof(args_SpawnProjectile_t, noammo), 1},
		{"spawnofs_xy", handle_fixed, offsetof(args_SpawnProjectile_t, spawnofs_xy), 1},
		{"spawnheight", handle_fixed, offsetof(args_SpawnProjectile_t, spawnheight), 1},
		{"flags", handle_flags, offsetof(args_SpawnProjectile_t, flags), 1, flags_FireProjectile},
		{"pitch", handle_angle, offsetof(args_SpawnProjectile_t, pitch), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_FireProjectile(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	const args_SpawnProjectile_t *arg = st->arg;
	mobj_t *th;
	angle_t angle, pitch;
	fixed_t slope;
	fixed_t x, y, z;

	if(!pl)
		return;

	if(!mo->custom_inventory && !arg->noammo)
	{
		mobjinfo_t *weap = pl->readyweapon;
		uint32_t take;
		// ammo check
		if(!weapon_has_ammo(mo, weap, pl->attackdown))
			return;
		// which ammo
		take = pl->attackdown;
		if(take > 1)
		{
			if(weap->eflags & MFE_WEAPON_ALT_USES_BOTH)
				take |= 1;
		} else
		{
			if(weap->eflags & MFE_WEAPON_PRIMARY_USES_BOTH)
				take |= 2;
		}
		// ammo use
		if(take & 1 && weap->weapon.ammo_type[0])
			inventory_take(mo, weap->weapon.ammo_type[0], weap->weapon.ammo_use[0]);
		if(take & 2 && weap->weapon.ammo_type[1])
			inventory_take(mo, weap->weapon.ammo_type[1], weap->weapon.ammo_use[1]);
	}

	pitch = -arg->pitch;
	angle = mo->angle;

	if(arg->flags & FPF_AIMATANGLE)
		angle += arg->angle;

	if(!player_aim(pl, &angle, &slope, mobjinfo[arg->missiletype].flags1 & MF1_SEEKERMISSILE))
		pitch += mo->pitch;

	if(arg->angle && !(arg->flags & FPF_AIMATANGLE))
		angle = mo->angle + arg->angle;

	x = mo->x;
	y = mo->y;
	z = mo->z;
	z += mo->height / 2;
	z += mo->info->player.attack_offs;
	z += arg->spawnheight;

	if(arg->spawnofs_xy)
	{
		angle_t a = (mo->angle - ANG90) >> ANGLETOFINESHIFT;
		x += FixedMul(arg->spawnofs_xy, finecosine[a]);
		y += FixedMul(arg->spawnofs_xy, finesine[a]);
	}

	th = P_SpawnMobj(x, y, z, arg->missiletype);
	if(th->flags & MF_MISSILE)
		missile_stuff(th, mo, *linetarget, angle, pitch, slope);
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
// DEBUG

__attribute((regparm(2),no_caller_saved_registers))
void debug_codeptr(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	doom_printf("debug_codeptr:\n");
	doom_printf(" mobj 0x%08X\n", mo);
	doom_printf(" arg 0x%08X\n", st->arg);
	doom_printf(" target 0x%08X\n", mo->target);
	doom_printf(" tracer 0x%08X\n", mo->tracer);
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
	{"a_gunflash", A_GunFlash},
	{"a_checkreload", A_CheckReload},
	{"a_light0", A_Light0},
	{"a_light1", A_Light1},
	{"a_light2", A_Light2},
	{"a_weaponready", A_WeaponReady},
	{"a_refire", A_ReFire},
	// sound
	{"a_pain", A_Pain},
	{"a_scream", A_Scream},
	{"a_xscream", A_XScream},
	{"a_activesound", A_ActiveSound},
	{"a_startsound", A_StartSound, &args_StartSound},
	// basic control
	{"a_facetarget", A_FaceTarget},
	{"a_noblocking", A_NoBlocking},
	// "AI"
	{"a_look", A_Look},
	{"a_chase", A_Chase},
	// enemy attack
	{"a_spawnprojectile", A_SpawnProjectile, &args_SpawnProjectile},
	// player attack
	{"a_fireprojectile", A_FireProjectile, &args_FireProjectile},
	// misc
	{"a_setangle", A_SetAngle, &args_SetAngle},
	// inventory
	{"a_giveinventory", A_GiveInventory, &args_GiveInventory},
	// terminator
	{NULL}
};

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x0002B9F8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&linetarget},
};

