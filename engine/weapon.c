// kgsws' ACE Engine
////
// New weapon code.
// New powerup code.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "dehacked.h"
#include "decorate.h"
#include "inventory.h"
#include "weapon.h"
#include "mobj.h"
#include "map.h"

typedef struct
{
	const uint8_t *name;
	int32_t direction;
	void (*start)(mobj_t*,mobjinfo_t*);
	void (*stop)(mobj_t*);
	void (*tick)(mobj_t*);
} powerup_t;

//

// powerups
static void invul_start(mobj_t*,mobjinfo_t*);
static void invul_stop(mobj_t*);
static void invis_start(mobj_t*,mobjinfo_t*);
static void invis_stop(mobj_t*);
static const powerup_t powerup[] =
{
	[pw_invulnerability] = {"invulnerable", -1, invul_start, invul_stop},
	[pw_strength] = {"strength", 1},
	[pw_invisibility] = {"invisibility", -1, invis_start, invis_stop},
	[pw_ironfeet] = {"ironfeet", -1},
	[pw_allmap] = {NULL},
	[pw_infrared] = {"lightamp", -1},
};

// from mobj.c
extern const uint16_t base_anim_offs[NUM_MOBJ_ANIMS];

//
// POWER: invulnerability

static void invul_start(mobj_t *mo, mobjinfo_t *info)
{
	mo->flags1 |= MF1_INVULNERABLE;
	// TODO: reflective
}

static void invul_stop(mobj_t *mo)
{
	mo->flags1 &= ~(MF1_INVULNERABLE | MF1_REFLECTIVE);
}

//
// POWER: invisibility

static void invis_start(mobj_t *mo, mobjinfo_t *info)
{
	mo->flags |= MF_SHADOW;
}

static void invis_stop(mobj_t *mo)
{
	mo->flags &= ~MF_SHADOW;
}

//
// powerup giver

void powerup_give(player_t *pl, mobjinfo_t *info)
{
	const powerup_t *pw = powerup + info->powerup.type;
	if(!pw->start)
		return;
	pw->start(pl->mo, info);
}

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

//
// weapon animation

__attribute((regparm(2),no_caller_saved_registers))
static void hook_move_pspr(player_t *pl)
{
	uint32_t angle;

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

	if(pl->playerstate != PST_LIVE)
		return;

	if(!pl->weapon_ready)
		return;

	// do weapon bob here for smooth chainsaw
	angle = (128 * *leveltime) & FINEMASK;
	pl->psprites[0].sx = FRACUNIT + FixedMul(pl->bob, finecosine[angle]);
	angle &= FINEANGLES / 2 - 1;
	pl->psprites[0].sy = WEAPONTOP + FixedMul(pl->bob, finesine[angle]);
}

//
// weapon change by ticcmd

static inline void weapon_change(player_t *pl, ticcmd_t *cmd)
{
	uint32_t slot;
	uint16_t *ptr;
	uint16_t pick, last;
	uint16_t now;
	mobjinfo_t *select;

	if(!(cmd->buttons & 4))
		return;

	slot = ((cmd->buttons >> 3) & 7) + 1; // old handling, will be replaced soon

	ptr = pl->mo->info->player.wpn_slot[slot];
	if(!ptr)
		return;

	if(pl->readyweapon)
		now = pl->readyweapon - mobjinfo;
	else
		now = 0;

	// 3rd, 2nd, 1st
	pick = 0;
	last = 0;
	while(*ptr)
	{
		if(now == *ptr)
			now = 0;
		if(inventory_check(pl->mo, *ptr))
		{
			if(now)
				pick = *ptr;
			last = *ptr;
		}
		ptr++;
	}

	if(!pick)
		pick = last;

	if(!pick)
		return;

	select = mobjinfo + pick;
	if(select == pl->readyweapon)
		return;

	pl->pendingweapon = select;
}

//
// player think

static __attribute((regparm(2),no_caller_saved_registers))
void player_think(player_t *pl)
{
	ticcmd_t *cmd = &pl->cmd;

	if(pl->cheats & CF_NOCLIP)
		pl->mo->flags |= MF_NOCLIP;
	else
		pl->mo->flags &= ~MF_NOCLIP;

	if(pl->mo->flags & MF_JUSTATTACKED)
	{
		// chainsaw
		cmd->angleturn = 0;
		cmd->forwardmove = 64;
		cmd->sidemove = 0;
		pl->mo->flags &= ~MF_JUSTATTACKED;
	}

	if(pl->playerstate == PST_DEAD)
	{
		P_DeathThink(pl);
		return;
	}

	if(pl->mo->reactiontime)
		pl->mo->reactiontime--;
	else
		P_MovePlayer(pl);

	P_CalcHeight(pl);

	if(pl->mo->subsector->sector->special)
		P_PlayerInSpecialSector(pl);

	if(cmd->buttons & BT_SPECIAL)
		cmd->buttons = 0;

	weapon_change(pl, cmd);

	if(cmd->buttons & BT_USE)
	{
		if(!pl->usedown)
		{
			P_UseLines(pl);
			pl->usedown = 1;
		}
	} else
		pl->usedown = 0;

	hook_move_pspr(pl);

	// TODO: colormaps
	for(uint32_t i = 0; i < NUMPOWERS; i++)
	{
		if(pl->powers[i])
		{
			const powerup_t *pw = powerup + i;

			pl->powers[i] += pw->direction;
			if(!pl->powers[i])
			{
				if(pw->stop)
					pw->stop(pl->mo);
			} else
			if(pw->tick)
				pw->tick(pl->mo);
		}
	}

	if(pl->damagecount)
		pl->damagecount--;

	if(pl->bonuscount)
		pl->bonuscount--;
}

//
// API

void weapon_setup(player_t *pl)
{
	for(uint32_t i = 0; i < NUMPSPRITES; i++)
		pl->psprites[i].state = NULL;

	if(!pl->pendingweapon)
		pl->pendingweapon = pl->readyweapon;

	if(!pl->pendingweapon)
		return;

	pl->readyweapon = pl->pendingweapon;
	pl->pendingweapon = NULL;

	S_StartSound(pl->mo, pl->readyweapon->weapon.sound_up);

	pl->psprites[0].sy = WEAPONBOTTOM;
	weapon_set_state(pl, 0, pl->readyweapon, pl->readyweapon->st_weapon.raise);
}

uint32_t weapon_fire(player_t *pl, uint32_t secondary, uint32_t refire)
{
	mobjinfo_t *info = pl->readyweapon;
	uint32_t state = 0;

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

	// TODO: ammo check

	pl->attackdown = secondary;
	pl->weapon_ready = 0;
	pl->psprites[0].sx = FRACUNIT;
	pl->psprites[0].sy = WEAPONTOP;

	// just hope that 'A_WeaponReady' was not called in 'flash PSPR'
	pl->psprites[0].state = NULL;
	pl->psprites[0].tics = state;

	return 1;
}

__attribute((regparm(2),no_caller_saved_registers))
void wpn_codeptr(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	// code pointer wrapper for original weapon actions
	void (*func)(player_t*,pspdef_t*) __attribute((regparm(2),no_caller_saved_registers));
	func = st->arg + doom_code_segment;
	func(mo->player, NULL);
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'P_DropWeapon' in 'P_KillMobj'
	{0x0002A387, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_lower_weapon},
	// replace call to '' in ''
	{0x00032FBE, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)player_think},
	// replace call to 'P_MovePsprites'
	{0x00033278, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_move_pspr},
	// change size of 'attackdown' in 'ST_updateFaceWidget'
	{0x0003A187, CODE_HOOK | HOOK_UINT8, 0x80},
};

