// kgsws' ACE Engine
////
// New weapon code.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "dehacked.h"
#include "decorate.h"
#include "inventory.h"
#include "weapon.h"
#include "config.h"
#include "mobj.h"
#include "action.h"
#include "map.h"
#include "sound.h"
#include "demo.h"
#include "player.h"

// from mobj.c
extern const uint16_t base_anim_offs[NUM_MOBJ_ANIMS];

//
// weapon states

static __attribute((regparm(3),no_caller_saved_registers)) // three!
uint32_t weapon_change_state0(mobj_t *mo, uint32_t state, uint16_t extra)
{
	// defer state change for later
	mo->player->psprites[0].state = NULL;
	mo->player->psprites[0].tics = state;
	mo->player->psprites[0].extra = extra;
}

static __attribute((regparm(3),no_caller_saved_registers)) // three!
uint32_t weapon_change_state1(mobj_t *mo, uint32_t state, uint16_t extra)
{
	// defer state change for later
	mo->player->psprites[1].state = NULL;
	mo->player->psprites[1].tics = state;
	mo->player->psprites[1].extra = extra;
}

void weapon_set_state(player_t *pl, uint32_t idx, mobjinfo_t *info, uint32_t state, uint16_t extra)
{
	// normal state changes
	state_t *st;
	pspdef_t *psp;
	void *func;
	uint32_t oldstate = 0;

	psp = pl->psprites + idx;
	if(idx)
		func = weapon_change_state1;
	else
		func = weapon_change_state0;

	while(1)
	{
		state = dec_reslove_state(info, oldstate, state, extra);

		if(!state)
		{
			psp->state = NULL;
			return;
		}

		st = states + state;
		psp->state = st;
		psp->tics = st->tics == 0xFFFF ? -1 : st->tics;
		oldstate = state;
		extra = st->next_extra;
		state = st->nextstate;

		if(pl->powers[pw_attack_speed] && psp->tics > 1)
			psp->tics >>= 1;

		if(!idx)
		{
			if(st->misc1)
				pl->psprites[1].sx = st->misc1;
			if(st->misc2)
				pl->psprites[1].sy = st->misc2;
		}

		if(st->acp)
		{
			st->acp(pl->mo, st, func);
			if(!psp->state)
			{
				// apply deferred state change
				state = psp->tics;
				extra = psp->extra;
				continue;
			}
		}

		if(psp->tics)
			break;
	}
}

//
// weapon logic

__attribute((regparm(2),no_caller_saved_registers))
void weapon_lower(player_t *pl)
{
	if(!pl->readyweapon)
		return;

	pl->weapon_ready = 0;
	weapon_set_state(pl, 0, pl->readyweapon, pl->readyweapon->st_weapon.lower, 0);
}

//
// weapon animation

__attribute((regparm(2),no_caller_saved_registers))
void weapon_move_pspr(player_t *pl)
{
	uint32_t angle;

	if(!pl->readyweapon)
		return;

	for(uint32_t i = 0; i < NUMPSPRITES; i++)
	{
		pspdef_t *psp = pl->psprites + i;

		if(!psp->state)
			continue;

		if(!psp->tics)
		{
			// special case for deferred gun flash
			weapon_set_state(pl, i, pl->readyweapon, psp->state - states, 0);
			if(psp->tics > 0)
				psp->tics++;
		}

		if(psp->tics > 0)
		{
			psp->tics--;
			if(!psp->tics)
				weapon_set_state(pl, i, pl->readyweapon, psp->state->nextstate, psp->state->next_extra);
		}
	}

	if(pl->weapon_ready || extra_config.center_weapon == 2)
	{
		if(pl->readyweapon->eflags & MFE_WEAPON_DONTBOB)
		{
			pl->psprites[0].sx = 0;
			pl->psprites[0].sy = 0;
		} else
		{
			// do weapon bob here for smooth chainsaw
			angle = (128 * leveltime) & FINEMASK;
			pl->psprites[0].sx = FixedMul(pl->bob, finecosine[angle]) >> FRACBITS;
			angle &= FINEANGLES / 2 - 1;
			pl->psprites[0].sy = FixedMul(pl->bob, finesine[angle]) >> FRACBITS;
		}
	} else
	if(extra_config.center_weapon)
	{
		pl->psprites[0].sx = 0;
		pl->psprites[0].sy = 0;
	}
}

//
// API

void weapon_setup(player_t *pl)
{
	for(uint32_t i = 0; i < NUMPSPRITES; i++)
		pl->psprites[i].state = NULL;

	if(is_title_map)
	{
		pl->readyweapon = NULL;
		pl->pendingweapon = NULL;
		return;
	}

	if(!pl->pendingweapon)
		pl->pendingweapon = pl->readyweapon;

	if(!pl->pendingweapon)
		return;

	pl->readyweapon = pl->pendingweapon;
	pl->pendingweapon = NULL;

	pl->psprites[0].sx = 0;
	pl->psprites[0].sy = 0;
	pl->psprites[1].sy = 1;

	S_StartSound(SOUND_CHAN_WEAPON(pl->mo), pl->readyweapon->weapon.sound_up);

	if(map_level_info->flags & MAP_FLAG_SPAWN_WITH_WEAPON_RAISED)
	{
		pl->psprites[1].sy = WEAPONTOP;
		weapon_set_state(pl, 0, pl->readyweapon, pl->readyweapon->st_weapon.ready, 0);
	} else
	{
		pl->psprites[1].sy = WEAPONBOTTOM;
		weapon_set_state(pl, 0, pl->readyweapon, pl->readyweapon->st_weapon.raise, 0);
	}
}

uint32_t weapon_fire(player_t *pl, uint32_t secondary, uint32_t refire)
{
	mobjinfo_t *info = pl->readyweapon;
	uint32_t state = 0;

	if(refire < 2)
	{
		if(secondary > 1)
		{
			if(refire)
				state = info->st_weapon.hold_alt;
			if(!state)
				state = info->st_weapon.fire_alt;
		} else
		{
			if(refire)
				state = info->st_weapon.hold;
			if(!state)
				state = info->st_weapon.fire;
		}

		if(!state)
			return 0;

		pl->attackdown = secondary;
	} else
		state = secondary;

	// ammo check
	if(!weapon_check_ammo(pl))
		return 1;

	// mobj to 'melee' animation
	if(pl->mo->animation != ANIM_MELEE && pl->mo->info->state_melee)
		mobj_set_animation(pl->mo, ANIM_MELEE);

	if(!(info->eflags & MFE_WEAPON_NOALERT))
		P_NoiseAlert(pl->mo, pl->mo);

	pl->weapon_ready = 0;

	if(!refire && extra_config.center_weapon == 1)
	{
		pl->psprites[1].sx = 1;
		pl->psprites[1].sy = WEAPONTOP;
	}

	// just hope that this was not called in 'flash PSPR'
	pl->psprites[0].state = NULL;
	pl->psprites[0].tics = state;
	pl->psprites[0].extra = 0;

	return 1;
}

uint32_t weapon_check_ammo(player_t *pl)
{
	uint32_t count;
	uint16_t type;
	uint16_t selection_order = 0xFFFF;
	mobjinfo_t *weap = pl->readyweapon;

	if(!pl->attackdown)
		return 0;

	if(weapon_has_ammo(pl->mo, weap, pl->attackdown))
		return 1;

	pl->attackdown = 0;

	// find next suitable weapon
	for(uint32_t i = 0; i < NUM_WPN_SLOTS; i++)
	{
		uint16_t *ptr;

		ptr = pl->mo->info->player.wpn_slot[i];
		if(!ptr)
			continue;

		while(*ptr)
		{
			uint16_t type = *ptr++;
			mobjinfo_t *info = mobjinfo + type;

			if(info == pl->readyweapon)
				continue;

			// priority
			if(info->weapon.selection_order > selection_order)
				continue;

			// check
			if(!inventory_check(pl->mo, type))
				continue;

			if(weapon_has_ammo(pl->mo, info, 3))
			{
				selection_order = info->weapon.selection_order;
				weap = info;
			}
		}
	}

	pl->pendingweapon = weap;

	// just hope that this was not called in 'flash PSPR'
	pl->psprites[0].state = NULL;
	pl->psprites[0].tics = pl->readyweapon->st_weapon.lower;
	pl->psprites[0].extra = 0;
	pl->weapon_ready = 0;

	return 0;
}

uint32_t weapon_has_ammo(mobj_t *mo, mobjinfo_t *info, uint32_t check)
{
	uint16_t ammo_have[2];

	// primary ammo
	if(!info->weapon.ammo_type[0] || !info->weapon.ammo_use[0] || info->eflags & MFE_WEAPON_AMMO_OPTIONAL)
		ammo_have[0] = 1;
	else
		ammo_have[0] = inventory_check(mo, info->weapon.ammo_type[0]) >= info->weapon.ammo_use[0];

	// secondary ammo
	if(!info->weapon.ammo_type[1] || !info->weapon.ammo_use[1] || info->eflags & MFE_WEAPON_ALT_AMMO_OPTIONAL)
		ammo_have[1] = 1;
	else
		ammo_have[1] = inventory_check(mo, info->weapon.ammo_type[1]) >= info->weapon.ammo_use[1];

	// check primary
	if(info->st_weapon.fire && (check & 1))
	{
		if(	ammo_have[0] &&
			(ammo_have[1] || !(info->eflags & MFE_WEAPON_PRIMARY_USES_BOTH))
		)
			return 1;
	}

	// check secondary
	if(info->st_weapon.fire_alt && (check & 2))
	{
		if(	ammo_have[1] &&
			(ammo_have[0] || !(info->eflags & MFE_WEAPON_ALT_USES_BOTH))
		)
			return 1;
	}

	return 0;
}

__attribute((regparm(2),no_caller_saved_registers))
void wpn_codeptr(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	// code pointer wrapper for original weapon actions
	void (*func)(player_t*,pspdef_t*) __attribute((regparm(2),no_caller_saved_registers));
	func = st->arg + doom_code_segment;
	func(mo->player, NULL);
}

__attribute((regparm(2),no_caller_saved_registers))
void wpn_sound(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	// code pointer hack for orignal weapon sounds
	uint16_t snd = (uint32_t)st->arg;
	if(!snd)
		snd = 6;
	S_StartSound(SOUND_CHAN_WEAPON(mo), snd);
	if(snd == 6)
		A_ReFire(mo, st, stfunc);
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'P_MovePsprites'
	{0x00033278, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)weapon_move_pspr},
	// change size of 'attackdown' in 'ST_updateFaceWidget'
	{0x0003A187, CODE_HOOK | HOOK_UINT8, 0x80},
};

