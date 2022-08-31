// kgsws' ACE Engine
////
// MOBJ handling changes.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "decorate.h"
#include "map.h"

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t set_mobj_animation(mobj_t *mo, uint8_t anim)
{
	mobj_set_state(mo, STATE_SET_ANIMATION(anim, 0));
}

static __attribute((regparm(2),no_caller_saved_registers))
mobjinfo_t *prepare_mobj(mobj_t *mo, uint32_t type)
{
	uint32_t tmp = 8;
	mobjinfo_t *info;

	// find replacement
	while(mobjinfo[type].replacement)
	{
		if(!tmp)
			I_Error("[DECORATE] Too many replacements!");
		type = mobjinfo[type].replacement;
		tmp--;
	}

	info = mobjinfo + type;

	// clear memory
	memset(mo, 0, sizeof(mobj_t));

	// fill in new stuff
	mo->type = type;
	mo->flags1 = info->flags1;
	mo->netid = mobj_netid++;

	// return offset
	return info;
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t finish_mobj(mobj_t *mo)
{
	// add thinker
	P_AddThinker(&mo->thinker);

	// ZDoom compatibility
	// teleport fog starts teleport sound
	if(mo->type == 39)
		S_StartSound(mo, 35);
}

static __attribute((regparm(2),no_caller_saved_registers))
void touch_mobj(mobj_t *mo, mobj_t *toucher)
{
	// old items; workaround for 'sprite' changes in 'mobj_t'
	uint16_t temp = mo->frame;
	mo->frame = 0;
	P_TouchSpecialThing(mo, toucher);
	mo->frame = temp;
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t mobj_set_state(mobj_t *mo, uint32_t state)
{
	state_t *st;

	do
	{
		if(state & 0x80000000)
		{
			// change animation
			const dec_anim_t *anim;
			uint16_t offset;

			offset = state & 0xFFFF;
			mo->animation = (state >> 16) & 0xFF;

			anim = mobj_anim + mo->animation;

			state = *((uint16_t*)((void*)mo->info + anim->offset));
			if(state)
				state += offset;

			if(state >= mo->info->state_idx_limit)
				I_Error("[MOBJ] State set '%s + %u' is invalid!", anim->name, offset);
		}

		if(!state)
		{
			P_RemoveMobj(mo);
			return 0;
		}

		st = states + state;
		mo->state = st;
		mo->sprite = st->sprite;
		mo->frame = st->frame;
		mo->tics = st->tics;
		state = st->nextstate;

		if(st->acp)
			st->acp(mo);

	} while(!mo->tics);

	return 1;
}

__attribute((regparm(2),no_caller_saved_registers))
static void kill_animation(mobj_t *mo)
{
	// custom death animations can be added

	if(mo->info->xdeathstate && mo->health < -mo->info->spawnhealth)
		set_mobj_animation(mo, ANIM_XDEATH);
	else
		set_mobj_animation(mo, ANIM_DEATH);

	if(mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}
}

__attribute((regparm(2),no_caller_saved_registers))
void explode_missile(mobj_t *mo)
{
	mo->momx = 0;
	mo->momy = 0;
	mo->momz = 0;

	set_mobj_animation(mo, ANIM_DEATH);

	if(mo->flags1 & MF1_RANDOMIZE && mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	mo->flags &= ~MF_MISSILE;

	S_StartSound(mo, mo->info->deathsound);
}

void mobj_damage(mobj_t *target, mobj_t *inflictor, mobj_t *source, uint32_t damage, uint32_t extra)
{
	// target = what is damaged
	// inflictor = damage source (projectile or ...)
	// source = what is responsible
	player_t *player;

	if(!(target->flags & MF_SHOOTABLE))
		return;

	if(target->health <= 0)
		return;

	if(target->flags & MF_SKULLFLY)
	{
		target->momx = 0;
		target->momy = 0;
		target->momz = 0;
	}

	player = target->player;

	if(player && !*gameskill)
		damage /= 2;

	if(	inflictor &&
		!(target->flags1 & MF1_DONTTHRUST) &&
		!(target->flags & MF_NOCLIP) &&
		!(inflictor->flags1 & MF1_NODAMAGETHRUST) && // TODO: extra steps for hitscan
		(!source || !source->player || source->player->readyweapon != 7) // chainsaw hack // TODO: replace with weapon.kickback
	) {
		angle_t angle;
		int32_t thrust;

		thrust = damage > 10000 ? 10000 : damage;

		angle = R_PointToAngle2(inflictor->x, inflictor->y, target->x, target->y);
		thrust = thrust * (FRACUNIT >> 3) * 100 / target->info->mass;

		if(	!(target->flags1 & MF1_NOFORWARDFALL) &&
			!(inflictor->flags1 & MF1_NOFORWARDFALL) && // TODO: extra steps for hitscan
			damage < 40 &&
			damage > target->health &&
			target->z - inflictor->z > 64 * FRACUNIT &&
			P_Random() & 1
		) {
			angle += ANG180;
			thrust *= 4;
		}

		angle >>= ANGLETOFINESHIFT;
		target->momx += FixedMul(thrust, finecosine[angle]);
		target->momy += FixedMul(thrust, finesine[angle]);
	}

	if(player)
	{
		if(target->subsector->sector->special == 11 && damage >= target->health)
			damage = target->health - 1;

		if(	damage < 1000000 &&
			( player->cheats & CF_GODMODE ||
			player->powers[pw_invulnerability] )
		)
			return;

		if(player->armortype)
		{
			uint32_t saved;

			saved = (player->armortype == 1) ? (damage / 3) : (damage / 2);
			if(player->armorpoints <= saved)
			{
				saved = player->armorpoints;
				player->armortype = 0;
			}

			player->armorpoints -= saved;
			damage -= saved;
		}

		player->health -= damage;
		if(player->health < 0)
			player->health = 0;

		player->attacker = source;

		player->damagecount += damage;
		if(player->damagecount > 100)
			player->damagecount = 100;

		// I_Tactile ...
	}

	target->health -= damage;
	if(target->health <= 0)
	{
		P_KillMobj(source, target);
		return;
	}

	if(	P_Random() < target->info->painchance &&
		!(target->flags & MF_SKULLFLY)
	) {
		target->flags |= MF_JUSTHIT;
		mobj_set_state(target, STATE_SET_ANIMATION(ANIM_PAIN, 0));
	}

	target->reactiontime = 0;

	if(	(!target->threshold || target->flags1 & MF1_QUICKTORETALIATE) &&
		source && source != target &&
		!(source->flags1 & MF1_NOTARGET)
	) {
		target->target = source;
		target->threshold = 100;
		if(target->info->seestate && target->state == states + target->info->spawnstate)
			mobj_set_state(target, STATE_SET_ANIMATION(ANIM_SEE, 0));
	}
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'memset' in 'P_SpawnMobj'
	{0x00031569, CODE_HOOK | HOOK_UINT16, 0xEA89},
	{0x00031571, CODE_HOOK | HOOK_UINT32, 0x90C38900},
	{0x0003156D, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)prepare_mobj},
	// use 'mobjinfo' pointer from new 'prepare_mobj'
	{0x00031585, CODE_HOOK | HOOK_SET_NOPS, 6},
	// update 'P_SpawnMobj'; disable '->type'
	{0x00031579, CODE_HOOK | HOOK_SET_NOPS, 3},
	// replace call to 'P_AddThinker' in 'P_SpawnMobj'
	{0x00031647, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)finish_mobj},
	// replace call to 'P_TouchSpecialThing' in 'PIT_CheckThing'
	{0x0002B031, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)touch_mobj},
	// replace call to 'P_SetMobjState' in 'P_MobjThinker'
	{0x000314F0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)mobj_set_state},
	// fix jump condition in 'P_MobjThinker'
	{0x000314E4, CODE_HOOK | HOOK_UINT8, 0x7F},
	// replace 'P_SetMobjState'
	{0x00030EA0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)mobj_set_state},
	// replace 'P_DamageMobj' - use trampoline
	{0x0002A460, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)hook_mobj_damage},
	// extra stuff in 'P_KillMobj' - replaces animation change
	{0x0002A3C8, CODE_HOOK | HOOK_UINT16, 0xD889},
	{0x0002A3CA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)kill_animation},
	{0x0002A3CF, CODE_HOOK | HOOK_JMP_DOOM, 0x0002A40D},
	// replace 'P_ExplodeMissile'
	{0x00030F00, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)explode_missile},
	// change 'mobj_t' size
	{0x00031552, CODE_HOOK | HOOK_UINT32, sizeof(mobj_t)},
	// fix 'P_SpawnMobj'; disable old 'frame'
	{0x000315F9, CODE_HOOK | HOOK_SET_NOPS, 3},
	// replace 'P_SetMobjState' with new animation system
	{0x00027776, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_SEE << 8)}, // A_Look
	{0x00027779, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Look
	{0x0002782A, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_SPAWN << 8)}, // A_Chase
	{0x0002782D, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	{0x0002789D, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_MELEE << 8)}, // A_Chase
	{0x000278A0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	{0x000278DD, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_MISSILE << 8)}, // A_Chase
	{0x000278E0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	// replace 'P_SetMobjState' with new animation system (PIT_CheckThing, MF_SKULLFLY)
	{0x0002AF2F, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_SPAWN << 8)},
	{0x0002AF32, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation},
	// replace 'P_SetMobjState' with new animation system (PIT_ChangeSector, gibs)
	{0x0002BEBA, CODE_HOOK | HOOK_UINT8, ANIM_CRUSH},
	{0x0002BEC0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation},
	// replace 'P_SetMobjState' with new animation system (P_MovePlayer)
	{0x0003324B, CODE_HOOK | HOOK_UINT16, 0xB880},
	{0x0003324D, CODE_HOOK | HOOK_UINT32, offsetof(mobj_t, animation)},
	{0x00033251, CODE_HOOK | HOOK_UINT8, ANIM_SPAWN}, // check for
	{0x00033255, CODE_HOOK | HOOK_UINT32, ANIM_SEE}, // replace with
	{0x00033259, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation},
};
