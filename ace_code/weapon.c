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
			state = sp->state - states;
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

	if(pl->pendingweapon == 0xFFFF)
		pl->pendingweapon = pl->readyweapon;

	wp = decorate_extra_info[DECORATE_EXTRA_WEAPON];
	wp += pl->pendingweapon;

	pl->pendingweapon = 0xFFFF;

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

