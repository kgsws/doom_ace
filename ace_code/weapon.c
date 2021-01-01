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
	player_t *pl = mo->player;
	dextra_weapon_t *wpbase = decorate_extra_info[DECORATE_EXTRA_WEAPON];
	dextra_weapon_t *wp = &item->info->extra->weapon;
	uint32_t idx = wp - wpbase;
	uint32_t sub = 1 << (idx & 31);

	// TODO: ammo check

	// select this weapon
	if(!(pl->weaponowned[idx >> 5] & sub))
		pl->pendingweapon = idx;

	// owned now
	pl->weaponowned[idx >> 5] |= sub;

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

__attribute((regparm(2),no_caller_saved_registers))
uint8_t weapon_ticcmd_set()
{
	for(uint32_t i = 0; i < WPN_NUMSLOTS; i++)
	{
		if(gamekeydown[i + '0'])
		{
			// check this slot
			player_t *pl = players + *consoleplayer;
			uint8_t *slot = pl->class->weaponslot[i];
			uint8_t pick = 0;
			uint8_t first = 0;

			for(uint32_t j = 0; j < WPN_PERSLOT; j++)
			{
				uint8_t idx = slot[j];
				if(!idx)
					break;
				if(!(pl->weaponowned[idx >> 5] & (1 << (idx & 31))))
					continue;
				if(!first && idx != pl->readyweapon)
					first = idx;
				if(pick)
				{
					if(idx == pl->readyweapon)
						pick = 0;
				} else
				if(idx != pl->readyweapon)
					pick = idx;
			}
			if(!pick)
				pick = first;
			if(pick && pick != pl->readyweapon)
				return pick;
		}
	}
	return 0;
}

__attribute((regparm(2),no_caller_saved_registers))
void weapon_ticcmd_parse()
{
	register ticcmd_t *cmd asm("ebx");
	player_t *pl;
	uint32_t idx;
	uint32_t sub;

	pl = (player_t*)((uint8_t*)cmd - offsetof(player_t, cmd));

	// TODO: do not change when morphed

	if(*demoplayback) // TODO: check version
	{
		if(!(cmd->buttons & 4))
			return;

		idx = (cmd->buttons >> 3) & 0b111;
		idx++; // skip 'no weapon'

		if(	idx == 1 &&
			pl->weaponowned[0] & (1 << 8) &&
			!(pl->readyweapon != 8 && pl->powers[pw_strength])
		)
			idx = 8;

		if(	idx == 3 &&
			pl->weaponowned[0] & (1 << 9) &&
			pl->readyweapon != 9
		)
			idx = 9;
	} else
	{
		if(!cmd->weaponchange)
			return;
		idx = cmd->weaponchange;
		// TODO: do not change to weapon not present in player slots
	}

	sub = 1 << (idx & 31);

	if(!(pl->weaponowned[idx >> 5] & sub))
		return;

	if(idx == pl->readyweapon)
		return;

	pl->pendingweapon = idx;
}

