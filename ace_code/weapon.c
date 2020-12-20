// kgsws' Doom ACE
// New weapon code.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "mobj.h"
#include "decorate.h"
#include "weapon.h"

dextra_weapon_t *weapon_now;
pspdef_t *weapon_ps;
uint32_t weapon_refire_state;
uint32_t weapon_flash_state;

__attribute((regparm(2),no_caller_saved_registers))
uint32_t weapon_pickup(mobj_t *item, mobj_t *mo)
{
	dextra_weapon_t *wpbase = decorate_extra_info[DECORATE_EXTRA_WEAPON];
	dextra_weapon_t *wp = &item->info->extra->weapon;

	// select this weapon
	mo->player->pendingweapon = wp - wpbase;

	return 1; // OK
}

__attribute((regparm(2),no_caller_saved_registers))
void weapon_set_sprite(player_t *pl, uint32_t slot, uint32_t state)
{
	pspdef_t *sp = &pl->psprites[slot];
	weapon_ps = sp;

	while(1)
	{
		state_t *st = states + (state & 0xFFFFFF);

		sp->state = st;
		sp->tics = st->tics;
		if(st->misc1)
		{
			sp->sx = st->misc1;
			sp->sy = st->misc2;
		}

		if(st->acp)
			st->acp(pl->mo);

		// next
		if(sp->tics == -2)
		{
			// state was chagned in codepointer
			// passed state is not a pointer
			state = (uint32_t)sp->state;
		} else
		{
			// check for ending
			if(sp->tics)
				return;
			state = st->nextstate;
		}
	}
}

__attribute((regparm(2),no_caller_saved_registers))
void weapon_setup(player_t *pl)
{
	dextra_weapon_t *wp;

	if(!pl->mo)
		return;

	if(pl->pendingweapon != 0xFFFF)
	{
		pl->readyweapon = pl->pendingweapon;
		pl->pendingweapon = 0xFFFF;
	}

	wp = decorate_extra_info[DECORATE_EXTRA_WEAPON];
	wp += pl->readyweapon;

	if(wp->upsound)
		S_StartSound(pl->mo, wp->upsound);

	pl->psprites[0].sy = WEAPONBOTTOM;

	weapon_now = wp;
	weapon_set_sprite(pl, 0, wp->select_state);
	weapon_set_sprite(pl, 1, 0);
	weapon_now = NULL;
}

__attribute((regparm(2),no_caller_saved_registers))
void weapon_tick(player_t *pl)
{
	weapon_now = decorate_extra_info[DECORATE_EXTRA_WEAPON];
	weapon_now += pl->readyweapon;

	for(uint32_t i = 0; i < NUMPSPRITES; i++)
	{
		pspdef_t *sp = &pl->psprites[i];
		if(sp->tics < 0)
			continue;
		sp->tics--;
		if(sp->tics)
			continue;
		weapon_set_sprite(pl, i, sp->state->nextstate);
	}

	pl->psprites[1].sx = pl->psprites[0].sx;
	pl->psprites[1].sy = pl->psprites[0].sy;

	weapon_now = NULL;
}

__attribute((regparm(2),no_caller_saved_registers))
void weapon_drop(player_t *pl)
{
	// called on death
	weapon_now = decorate_extra_info[DECORATE_EXTRA_WEAPON];
	weapon_now += pl->readyweapon;
	weapon_set_sprite(pl, 0, weapon_now->deselect_state);
	weapon_set_sprite(pl, 1, 0);
	weapon_now = NULL;
}

__attribute((regparm(2),no_caller_saved_registers))
void weapon_fire(player_t *pl, int32_t type)
{
	uint32_t fire_state;

	// TODO: check ammo

	if(type < 0)
		fire_state = weapon_refire_state;
	else
	{
		if(type)
		{
			fire_state = weapon_now->fire2_state;
			weapon_flash_state = weapon_now->flash2_state;
			if(weapon_now->hold2_state)
				weapon_refire_state = weapon_now->hold2_state;
			else
				weapon_refire_state = fire_state;
		} else
		{
			fire_state = weapon_now->fire1_state;
			weapon_flash_state = weapon_now->flash1_state;
			if(weapon_now->hold1_state)
				weapon_refire_state = weapon_now->hold1_state;
			else
				weapon_refire_state = fire_state;
		}
	}

	P_SetMobjAnimation(pl->mo, MOANIM_MISSILE);

	weapon_change_state(fire_state);

	if(!(weapon_now->flags & WPNFLAG_NOALERT))
		P_NoiseAlert(pl->mo, pl->mo);
}

