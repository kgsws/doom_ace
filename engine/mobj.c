// kgsws' ACE Engine
////
// MOBJ handling changes.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "dehacked.h"
#include "decorate.h"
#include "inventory.h"
#include "mobj.h"
#include "map.h"

uint32_t *playeringame;
player_t *players;

uint32_t *consoleplayer;
uint32_t *displayplayer;

thinker_t *thinkercap;

static const uint32_t view_height_ptr[] =
{
	0x00031227, // P_ZMovement
	0x00033088, // P_CalcHeight
	0x000330EE, // P_CalcHeight
	0x000330F7, // P_CalcHeight
	0, // separator (half height)
	0x00033101, // P_CalcHeight
	0x0003310D, // P_CalcHeight
};

// this only exists because original animations are all over the plase in 'mobjinfo_t'
static const uint16_t base_anim_offs[NUM_MOBJ_ANIMS] =
{
	[ANIM_SPAWN] = offsetof(mobjinfo_t, state_spawn),
	[ANIM_SEE] = offsetof(mobjinfo_t, state_see),
	[ANIM_PAIN] = offsetof(mobjinfo_t, state_pain),
	[ANIM_MELEE] = offsetof(mobjinfo_t, state_melee),
	[ANIM_MISSILE] = offsetof(mobjinfo_t, state_missile),
	[ANIM_DEATH] = offsetof(mobjinfo_t, state_death),
	[ANIM_XDEATH] = offsetof(mobjinfo_t, state_xdeath),
	[ANIM_RAISE] = offsetof(mobjinfo_t, state_raise),
	[ANIM_CRUSH] = offsetof(mobjinfo_t, state_crush),
	[ANIM_HEAL] = offsetof(mobjinfo_t, state_heal),
};

//
// funcs

static void set_viewheight(fixed_t wh)
{
	// TODO: do this in 'coop spy' too
	for(uint32_t i = 0; i < sizeof(view_height_ptr) / sizeof(uint32_t); i++)
	{
		if(!view_height_ptr[i])
		{
			wh /= 2;
			continue;
		}

		*((fixed_t*)(view_height_ptr[i] + doom_code_segment)) = wh;
	}
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void spawn_player(mapthing_t *mt)
{
	player_t *pl;
	int32_t idx = mt->type - 1;
	mobj_t *mo;
	mobjinfo_t *info;

	if(!playeringame[idx])
		return;

	pl = players + idx;
	info = mobjinfo + player_class[0];

	// create body
	mo = P_SpawnMobj((fixed_t)mt->x << FRACBITS, (fixed_t)mt->y << FRACBITS, 0x80000000, player_class[0]);

	// check for reset
	if(pl->playerstate == PST_REBORN)
	{
		// cleanup
		uint32_t killcount;
		uint32_t itemcount;
		uint32_t secretcount;

		killcount = pl->killcount;
		itemcount = pl->itemcount;
		secretcount = pl->secretcount;

		memset(pl, 0, sizeof(player_t));

		pl->killcount = killcount;
		pl->itemcount = itemcount;
		pl->secretcount = secretcount;

		pl->usedown = 1;
		pl->attackdown = 1;
		pl->playerstate = PST_LIVE;
		pl->health = deh_plr_health;

		//
		// OLD INVENTORY - REMOVE
		pl->readyweapon = 1;
		pl->pendingweapon = 1;
		pl->weaponowned[0] = 1;
		pl->weaponowned[1] = 1;
		pl->ammo[0] = deh_plr_bullets;
		for(uint32_t i = 0; i < 4; i++)
			pl->maxammo[i] = *((uint32_t*)(0x00012D70 + doom_data_segment) + i);
		if(*deathmatch)
			memset(pl->cards, 0xFF, NUMCARDS * sizeof(uint32_t));

		// default inventory
		for(plrp_start_item_t *si = info->start_item.start; si < (plrp_start_item_t*)info->start_item.end; si++)
			inventory_give(mo, si->type, si->count);
	}

	// TODO: translation not in flags
	mo->flags |= idx << 26;

	mo->angle = ANG45 * (mt->angle / 45);
	mo->player = pl;
	mo->health = pl->health;

	pl->mo = mo;
	pl->playerstate = PST_LIVE;
	pl->refire = 0;
	pl->message = NULL;
	pl->damagecount = 0;
	pl->bonuscount = 0;
	pl->extralight = 0;
	pl->fixedcolormap = 0;
	pl->viewheight = info->player.view_height;

	P_SetupPsprites(pl);

	if(idx == *consoleplayer)
	{
		ST_Start();
		HU_Start();
		set_viewheight(pl->viewheight);
	}
}

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
	mobjinfo_t *info;
	player_t *pl;
	uint32_t left;
	fixed_t diff;

	if(!toucher->player || toucher->player->playerstate != PST_LIVE)
		return;

	diff = mo->z - toucher->z;
	if(diff > toucher->height || diff < -mo->height)
		return;

	pl = toucher->player;

	if(mo->type < NUMMOBJTYPES)
	{
		// old items; workaround for 'sprite' changes in 'mobj_t'

		// TODO: rework
		uint16_t temp = mo->frame;
		mo->frame = 0;
		P_TouchSpecialThing(mo, toucher);
		mo->frame = temp;

		return;
	}

	// new inventory stuff
	info = mo->info;

	switch(info->extra_type)
	{
		case ETYPE_WEAPON:
		{
			uint32_t given;

			// add to inventory
			given = !inventory_give(toucher, mo->type, info->inventory.count);

			// primary ammo
			if(info->weapon.ammo_type[0] && info->weapon.ammo_give[0])
			{
				left = inventory_give(toucher, info->weapon.ammo_type[0], info->weapon.ammo_give[0]);
				given |= left < info->weapon.ammo_give[0];
			}

			// secondary ammo
			if(info->weapon.ammo_type[1] && info->weapon.ammo_give[1])
			{
				left = inventory_give(toucher, info->weapon.ammo_type[1], info->weapon.ammo_give[1]);
				given |= left < info->weapon.ammo_give[1];
			}

			if(!given)
				return;
		}
		break;
		case ETYPE_AMMO:
		case ETYPE_AMMO_LINK:
			// add to inventory
			left = inventory_give(toucher, mo->type, info->inventory.count);
			if(left >= info->inventory.count)
				// can't pickup
				return;
		break;
		default:
			// this should not be set
			mo->flags &= ~MF_SPECIAL;
		return;
	}

	// remove
	P_RemoveMobj(mo);

	if(info->eflags & MFE_INVENTORY_QUIET)
		return;

	// sound & flash
	pl->bonuscount += 6;
	S_StartSound(toucher, info->inventory.sound_pickup);

	// message
	if(info->inventory.message)
		pl->message = info->inventory.message;
}

__attribute((regparm(2),no_caller_saved_registers))
static void kill_animation(mobj_t *mo)
{
	// custom death animations can be added

	if(mo->info->state_xdeath && mo->health < -mo->info->spawnhealth)
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

//
// API

void mobj_for_each(uint32_t (*cb)(mobj_t*))
{
	if(!thinkercap->next)
		// this happens only before any level was loaded
		return;

	for(thinker_t *th = thinkercap->next; th != thinkercap; th = th->next)
	{
		if(th->function != (void*)0x00031490 + doom_code_segment)
			continue;
		if(cb((mobj_t*)th))
			return;
	}
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
			uint16_t offset;

			offset = state & 0xFFFF;
			mo->animation = (state >> 16) & 0xFF;

			if(mo->animation < NUM_MOBJ_ANIMS)
				state = *((uint16_t*)((void*)mo->info + base_anim_offs[mo->animation]));
			else
				state = mo->info->extra_states[mo->animation - NUM_MOBJ_ANIMS];

			if(state)
				state += offset;

			if(state >= mo->info->state_idx_limit)
				I_Error("[MOBJ] State jump '+%u' is invalid!", offset);
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

	if(	!deh_no_infight &&
		(!target->threshold || target->flags1 & MF1_QUICKTORETALIATE) &&
		source && source != target &&
		!(source->flags1 & MF1_NOTARGET)
	) {
		target->target = source;
		target->threshold = 100;
		if(target->info->state_see && target->state == states + target->info->state_spawn)
			mobj_set_state(target, STATE_SET_ANIMATION(ANIM_SEE, 0));
	}
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x0002B3D8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&displayplayer},
	{0x0002B3DC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&consoleplayer},
	{0x0002B2D8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&playeringame},
	{0x0002AE78, DATA_HOOK | HOOK_IMPORT, (uint32_t)&players},
	{0x0002CF74, DATA_HOOK | HOOK_IMPORT, (uint32_t)&thinkercap},
	// replace 'P_SpawnPlayer'
	{0x000317F0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)spawn_player},
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
	// fix player check in 'PIT_CheckThing'
	{0x0002AFC5, CODE_HOOK | HOOK_UINT16, 0xBB83},
	{0x0002AFC7, CODE_HOOK | HOOK_UINT32, offsetof(mobj_t, player)},
	{0x0002AFCB, CODE_HOOK | HOOK_UINT32, 0x427400},
	{0x0002AFCE, CODE_HOOK | HOOK_SET_NOPS, 6},
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

