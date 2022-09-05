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
#include "mobj.h"

// from mobj.c
extern const uint16_t base_anim_offs[NUM_MOBJ_ANIMS];

//
// weapon states

__attribute((regparm(2),no_caller_saved_registers))
static uint32_t weapon_change_state0(mobj_t *mo, uint32_t state)
{
	// defer state change for later
	mo->player->psprites[0].state = NULL;
	mo->player->psprites[0].tics = state;
}

__attribute((regparm(2),no_caller_saved_registers))
static uint32_t weapon_change_state1(mobj_t *mo, uint32_t state)
{
	// defer state change for later
	mo->player->psprites[1].state = NULL;
	mo->player->psprites[1].tics = state;
}

static void weapon_set_state(player_t *pl, uint32_t idx, mobjinfo_t *info, uint32_t state)
{
	// normal state changes
	state_t *st;
	pspdef_t *psp;
	void *func;

	psp = pl->psprites + idx;
	if(idx)
		func = weapon_change_state1;
	else
		func = weapon_change_state0;

	while(1)
	{
		if(state & 0x80000000)
		{
			// change animation
			uint16_t offset;
			uint8_t anim;

			offset = state & 0xFFFF;
			anim = (state >> 16) & 0xFF;

			if(anim < NUM_MOBJ_ANIMS)
				state = *((uint16_t*)((void*)info + base_anim_offs[anim]));
			else
				state = info->extra_states[anim - NUM_MOBJ_ANIMS];

			if(state)
				state += offset;

			if(state >= info->state_idx_limit)
				I_Error("[WEAPON] State jump '+%u' is invalid!", offset);
		}

		if(!state)
		{
			psp->state = NULL;
			return;
		}

		st = states + state;
		psp->state = st;
		psp->tics = st->tics;
		state = st->nextstate;

		if(st->misc1)
		{
			psp->sx = st->misc1 << FRACBITS;
			psp->sy = st->misc2 << FRACBITS;
		}

		if(st->acp)
		{
			st->acp(pl->mo, st, func);
			if(!psp->state)
			{
				// apply deferred state change
				state = psp->tics;
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
static void hook_lower_weapon(player_t *pl)
{
	if(!pl->readyweapon)
		return;

	weapon_set_state(pl, 0, pl->readyweapon, pl->readyweapon->st_weapon.lower);
}

static void weapon_raise(player_t *pl)
{
	if(!pl->pendingweapon)
		pl->pendingweapon = pl->readyweapon;

	if(!pl->pendingweapon)
		return;

	pl->readyweapon = pl->pendingweapon;
	pl->pendingweapon = NULL;

	// TODO: start sound

	pl->psprites[0].sy = WEAPONBOTTOM;
	weapon_set_state(pl, 0, pl->readyweapon, pl->readyweapon->st_weapon.raise);
}

//
// weapon animation

__attribute((regparm(2),no_caller_saved_registers))
static void hook_move_pspr(player_t *pl)
{
	if(!pl->readyweapon)
		return;

	for(uint32_t i = 0; i < NUMPSPRITES; i++)
	{
		pspdef_t *psp = pl->psprites + i;

		if(!psp->state)
			continue;

		if(psp->tics >= 0)
		{
			psp->tics--;
			if(psp->tics <= 0)
				weapon_set_state(pl, i, pl->readyweapon, psp->state->nextstate);
		}
	}
}

//
// API

void weapon_setup(player_t *pl)
{
	for(uint32_t i = 0; i < NUMPSPRITES; i++)
		pl->psprites[i].state = NULL;

	weapon_raise(pl);
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'P_DropWeapon' in 'P_KillMobj'
	{0x0002A387, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_lower_weapon},
	// replace call to 'P_MovePsprites'
	{0x00033278, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_move_pspr},
	{0x000334A4, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_move_pspr},
};

