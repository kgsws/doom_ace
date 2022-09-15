// kgsws' ACE Engine
////
// New player stuff.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "decorate.h"
#include "inventory.h"
#include "mobj.h"
#include "weapon.h"
#include "stbar.h"
#include "cheat.h"

typedef struct
{
	const uint8_t *name;
	int32_t direction;
	void (*start)(mobj_t*,mobjinfo_t*);
	void (*stop)(mobj_t*);
	void (*tick)(mobj_t*);
} powerup_t;

//

uint32_t *playeringame;
player_t *players;

// view height patches
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
// cheat buffer

static inline void cheat_char(uint32_t pidx, uint8_t cc)
{
	cheat_buf_t *cb;

	if(!cc)
		return;

	cb = cheat_buf + pidx;

	if(cc == CHAT_CHEAT_MARKER)
	{
		cb->len = 0;
		return;
	}

	if(cb->len < 0)
		return;

	if(cc == 0x0D)
	{
		cheat_check(pidx);
		return;
	}

	if(cc == 0x7F)
	{
		if(cb->len)
			cb->len--;
		return;
	}

	if(cb->len >= MAX_CHEAT_BUFFER)
		return;

	cb->text[cb->len] = cc;
	cb->len++;
}

//
// player think

static __attribute((regparm(2),no_caller_saved_registers))
void player_think(player_t *pl)
{
	ticcmd_t *cmd = &pl->cmd;

	if(pl->stbar_update)
	{
		if(pl - players == *consoleplayer)
			stbar_update(pl);
		else
			pl->stbar_update = 0;
	}

	if(pl->mo->flags & MF_JUSTATTACKED)
	{
		// chainsaw
		cmd->angleturn = 0;
		cmd->forwardmove = 64;
		cmd->sidemove = 0;
		pl->mo->flags &= ~MF_JUSTATTACKED;
	}

	cheat_char(pl - players, cmd->chatchar);

	if(pl->damagecount < 0)
		pl->damagecount = 0;
	if(pl->bonuscount < 0)
		pl->bonuscount = 0;

	if(pl->bonuscount) // this is NOT done in 'P_DeathThink'
		pl->bonuscount--;

	if(pl->playerstate == PST_DEAD)
	{
		pl->weapon_ready = 0;
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

	weapon_move_pspr(pl);

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
				{
					pw->stop(pl->mo);
					cheat_player_flags(pl);
				}
			} else
			if(pw->tick)
				pw->tick(pl->mo);
		}
	}

	if(pl->damagecount) // this IS done in 'P_DeathThink'
		pl->damagecount--;
}

//
// level transition

__attribute((regparm(2),no_caller_saved_registers))
static void player_finish(uint32_t pidx)
{
	player_t *pl = players + pidx;

	for(uint32_t i = 0; i < NUMPOWERS; i++)
	{
		const powerup_t *pw = powerup + i;
		if(pl->powers[i])
		{
			pl->powers[i] = 0;
			if(pw->stop && pl->mo)
				pw->stop(pl->mo);
		}
	}

	pl->fixedcolormap = 0;
	pl->damagecount = 0;
	pl->bonuscount = 0;
	pl->extralight = 0;

	if(pl->mo)
	{
		inventory_hubstrip(pl->mo);
		pl->inventory = pl->mo->inventory;
		pl->mo->inventory = NULL;
	}
}

//
// API

void player_viewheight(fixed_t wh)
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

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'P_PlayerThink' in 'P_Ticker'
	{0x00032FBE, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)player_think},
	// replace call to 'G_PlayerFinishLevel' in 'G_DoCompleted'
	{0x00020DBE, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)player_finish},
	// import variables
	{0x0002B2D8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&playeringame},
	{0x0002AE78, DATA_HOOK | HOOK_IMPORT, (uint32_t)&players},
};
