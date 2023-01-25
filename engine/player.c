// kgsws' ACE Engine
////
// New player stuff.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "decorate.h"
#include "inventory.h"
#include "saveload.h"
#include "mobj.h"
#include "render.h"
#include "weapon.h"
#include "stbar.h"
#include "sound.h"
#include "cheat.h"
#include "config.h"
#include "controls.h"
#include "demo.h"
#include "map.h"
#include "player.h"

typedef struct
{
	const uint8_t *name;
	int32_t direction;
	void (*start)(mobj_t*,mobjinfo_t*);
	void (*stop)(mobj_t*);
} powerup_t;

//

uint_fast8_t player_flags_changed = 1; // force update

player_info_t player_info[MAXPLAYERS] =
{
	{.flags = PLF_AUTO_SWITCH | PLF_AUTO_AIM},
	{.flags = PLF_AUTO_SWITCH | PLF_AUTO_AIM},
	{.flags = PLF_AUTO_SWITCH | PLF_AUTO_AIM},
	{.flags = PLF_AUTO_SWITCH | PLF_AUTO_AIM},
};

// powerups
static void invul_start(mobj_t*,mobjinfo_t*);
static void invul_stop(mobj_t*);
static void invis_start(mobj_t*,mobjinfo_t*);
static void invis_stop(mobj_t*);
static void buddha_start(mobj_t*,mobjinfo_t*);
static void buddha_stop(mobj_t*);
static void flight_start(mobj_t*,mobjinfo_t*);
static void flight_stop(mobj_t*);
static const powerup_t powerup[] =
{
	[pw_invulnerability] = {"invulnerable", -1, invul_start, invul_stop},
	[pw_strength] = {"strength", 1},
	[pw_invisibility] = {"invisibility", -1, invis_start, invis_stop},
	[pw_ironfeet] = {"ironfeet", -1},
	[pw_allmap] = {NULL},
	[pw_infrared] = {"lightamp", -1},
	[pw_buddha] = {"buddha", -1, buddha_start, buddha_stop},
	[pw_attack_speed] = {"doublefiringspeed", -1},
	[pw_flight] = {"flight", -1, flight_start, flight_stop},
	[pw_reserved0] = {NULL},
	[pw_reserved1] = {NULL},
	[pw_reserved2] = {NULL},
};

//
// POWER: invulnerability

static void invul_start(mobj_t *mo, mobjinfo_t *info)
{
	mo->flags1 |= MF1_INVULNERABLE;
	if(info->powerup.mode)
		mo->flags1 |= MF1_REFLECTIVE;
}

static void invul_stop(mobj_t *mo)
{
	mo->flags1 &= ~(MF1_INVULNERABLE | MF1_REFLECTIVE);
	mo->flags1 |= mo->info->flags1 & (MF1_INVULNERABLE | MF1_REFLECTIVE);
}

//
// POWER: invisibility

static void invis_start(mobj_t *mo, mobjinfo_t *info)
{
	mo->flags |= MF_SHADOW;

	if(!info->powerup.mode)
	{
		mo->render_style = RS_FUZZ;
		mo->render_alpha = 255;
		return;
	}

	mo->render_alpha = 255 - ((uint32_t)info->powerup.strength * 255) / 100;

	if(!mo->render_alpha)
		mo->render_style = RS_INVISIBLE;
	else
		mo->render_style = RS_TRANSLUCENT;
}

static void invis_stop(mobj_t *mo)
{
	mo->flags &= ~MF_SHADOW;
	mo->flags |= mo->info->flags & MF_SHADOW;
	mo->render_style = mo->info->render_style;
	mo->render_alpha = mo->info->render_alpha;
}

//
// POWER: buddha

static void buddha_start(mobj_t *mo, mobjinfo_t *info)
{
	mo->flags1 |= MF1_BUDDHA;
}

static void buddha_stop(mobj_t *mo)
{
	mo->flags1 &= ~MF1_BUDDHA;
	mo->flags1 |= mo->info->flags1 & MF1_BUDDHA;
}

//
// POWER: flight

static void flight_start(mobj_t *mo, mobjinfo_t *info)
{
	mo->flags1 |= MF_NOGRAVITY;
}

static void flight_stop(mobj_t *mo)
{
	mo->flags1 &= ~MF_NOGRAVITY;
	mo->flags1 |= mo->info->flags1 & MF_NOGRAVITY;
}

//
// powerup giver

void powerup_give(player_t *pl, mobjinfo_t *info)
{
	const powerup_t *pw = powerup + info->powerup.type;

	pl->power_color[info->powerup.type] = info->powerup.colorstuff;

	if(!pw->start)
		return;

	pw->start(pl->mo, info);
}

//
// buttons in ticcmd

static inline void check_buttons(player_t *pl, ticcmd_t *cmd)
{
	uint32_t slot;
	uint16_t *ptr;
	uint16_t pick, last;
	uint16_t now;
	mobjinfo_t *select;

	slot = (cmd->buttons & BT_ACTIONMASK) >> BT_ACTIONSHIFT;

	if(!slot)
		return;

	if(slot == BT_ACT_INV_PREV)
	{
		pl->inv_tick = PLAYER_INVBAR_TICS;
		inv_player_prev(pl->mo);
		return;
	}

	if(slot == BT_ACT_INV_NEXT)
	{
		pl->inv_tick = PLAYER_INVBAR_TICS;
		inv_player_next(pl->mo);
		return;
	}

	if(slot == BT_ACT_INV_USE)
	{
		if(pl->inv_sel >= 0)
			mobj_use_item(pl->mo, pl->mo->inventory->slot + pl->inv_sel);
		return;
	}

	if(slot == BT_ACT_JUMP)
	{
		if(pl->mo->flags & MF_NOGRAVITY)
		{
			// i don't have a good solution for this
		} else
		if(pl->mo->info->player.jump_z && pl->mo->z <= pl->mo->floorz)
		{
			pl->mo->momz += pl->mo->info->player.jump_z;
			S_StartSound(pl->mo, pl->mo->info->player.sound.jump);
		}
	}

	slot--;
	if(slot >= NUM_WPN_SLOTS)
		return;

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
void P_CalcHeight(player_t *player)
{
	uint32_t angle;
	fixed_t bob;
	fixed_t viewheight = player->mo->info->player.view_height;

	if(onground || !(player->mo->flags & MF_NOGRAVITY))
	{
		player->bob = FixedMul(player->mo->momx, player->mo->momx) + FixedMul(player->mo->momy,player->mo->momy);
		player->bob >>= 2;

		if(player->bob > MAXBOB)
			player->bob = MAXBOB;
	} else
		player->bob = 0;

	if(player->cheats & CF_NOMOMENTUM || (!onground && !(player->mo->flags & MF_NOGRAVITY)))
	{
		player->viewz = player->mo->z + viewheight;

		if(player->viewz > player->mo->ceilingz - 4 * FRACUNIT)
			player->viewz = player->mo->ceilingz - 4 * FRACUNIT;

		player->viewz = player->mo->z + player->viewheight;
		return;
	}

	angle = (FINEANGLES / 20 * leveltime) & FINEMASK;
	bob = FixedMul(player->bob / 2, finesine[angle]);

	if(player->state == PST_LIVE)
	{
		player->viewheight += player->deltaviewheight;

		if(player->viewheight > viewheight)
		{
			player->viewheight = viewheight;
			player->deltaviewheight = 0;
		}

		if(player->viewheight < viewheight / 2)
		{
			player->viewheight = viewheight / 2;
			if(player->deltaviewheight <= 0)
				player->deltaviewheight = 1;
		}

		if(player->deltaviewheight)
		{
			player->deltaviewheight += FRACUNIT / 4;
			if(!player->deltaviewheight)
				player->deltaviewheight = 1;
		}
	}

	player->viewz = player->mo->z + player->viewheight + bob;

	if(player->viewz > player->mo->ceilingz - 4 * FRACUNIT)
		player->viewz = player->mo->ceilingz - 4 * FRACUNIT;
}

static void player_sector_damage(player_t *pl, sector_extra_t *se)
{
	uint32_t damage;

	if(se->damage.tics > 1 && leveltime % se->damage.tics)
		return;

	if(se->damage.amount < 0)
	{
		uint32_t health = pl->mo->health;
		health -= se->damage.amount;
		if(health > pl->mo->info->spawnhealth)
			health = pl->mo->info->spawnhealth;
		pl->mo->health = health;
		pl->health = health;
		return;
	}

	if(	!(se->damage.type & 0x80) &&
		pl->powers[pw_ironfeet] &&
		(!se->damage.leak || P_Random() >= se->damage.leak)
	)
		return;

	if((se->damage.type & 0x7F) == DAMAGE_INSTANT)
		damage = 1000000;

	damage = DAMAGE_WITH_TYPE(damage, se->damage.type & 0x7F);

	mobj_damage(pl->mo, NULL, NULL, damage, NULL);
}

static __attribute((regparm(2),no_caller_saved_registers))
void player_think(player_t *pl)
{
	uint32_t idx = pl - players;
	ticcmd_t *cmd = &pl->cmd;

	if(pl->text_data)
	{
		pl->text_tics++;
		if(pl->text_tics >= pl->text_data->tics)
			pl->text_data = NULL;
	}

	if(pl->stbar_update)
	{
		if(idx == consoleplayer)
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

	cheat_char(idx, cmd->chatchar);

	if(pl->prop & ((1 << PROP_FROZEN) | (1 << PROP_TOTALLYFROZEN)))
	{
		cmd->forwardmove = 0;
		cmd->sidemove = 0;
	}
	if(pl->prop & (1 << PROP_TOTALLYFROZEN))
	{
		cmd->angleturn = 0;
		cmd->pitchturn = 0;
		if(!(cmd->buttons & BT_SPECIAL))
			cmd->buttons &= ~(BT_ATTACK|BT_ALTACK|BT_ACTIONMASK);
	}

	if(pl->camera != pl->mo && pl->prop & (1 << PROP_CAMERA_MOVE) && (cmd->forwardmove || cmd->sidemove))
	{
		pl->prop &= ~(1 << PROP_CAMERA_MOVE);
		pl->camera = pl->mo;
	}

	if(pl->damagecount < 0)
		pl->damagecount = 0;
	if(pl->bonuscount < 0)
		pl->bonuscount = 0;

	if(pl->bonuscount) // this is NOT done in 'P_DeathThink'
		pl->bonuscount--;

	if(pl->state == PST_DEAD)
	{
		if(	pl->info_flags & PLF_MOUSE_LOOK &&
			!(map_level_info->flags & MAP_FLAG_NO_FREELOOK) &&
			!(pl->flags & PF_IS_FROZEN)
		){
			int32_t pitch = pl->mo->pitch;
			if(pitch > PLAYER_LOOK_DEAD)
			{
				pitch -= PLAYER_LOOK_STEP;
				if(pitch < PLAYER_LOOK_DEAD)
					pitch = PLAYER_LOOK_DEAD;
			} else
			if(pitch < PLAYER_LOOK_DEAD)
			{
				pitch += PLAYER_LOOK_STEP;
				if(pitch > PLAYER_LOOK_DEAD)
					pitch = PLAYER_LOOK_DEAD;
			}
			pl->mo->pitch = (angle_t)pitch;
		} else
			pl->mo->pitch = 0;
		pl->weapon_ready = 0;
		pl->inv_tick = 0;
		pl->extralight = 0;

		P_DeathThink(pl);

		if(pl->flags & PF_IS_FROZEN)
		{
			pl->viewz = pl->mo->z + pl->mo->info->player.view_height;
			pl->fixedpalette = 1; // you can freely recolor this one
		}

		return;
	}

	if(!pl->mo->reactiontime)
	{
		ticcmd_t *cmd = &pl->cmd;
		int32_t scale;
		angle_t angle;

		pl->mo->angle += (cmd->angleturn << 16);

		angle = pl->mo->angle >> ANGLETOFINESHIFT;

		onground = (pl->mo->z <= pl->mo->floorz);

		if(pl->mo->flags & MF_NOGRAVITY && pl->mo->pitch)
		{
			scale = 2048;

			if(cmd->forwardmove)
			{
				fixed_t power = cmd->forwardmove * scale;

				if(pl->mo->pitch)
				{
					angle_t pitch = pl->mo->pitch >> ANGLETOFINESHIFT;
					pl->mo->momz += FixedMul(power, finesine[pitch]);
					power = FixedMul(power, finecosine[pitch]);
				}

				pl->mo->momx += FixedMul(power, finecosine[angle]);
				pl->mo->momy += FixedMul(power, finesine[angle]);
			}
		} else
		{
			scale = (onground || pl->mo->flags & MF_NOGRAVITY) ? 2048 : 8;

			if(cmd->forwardmove)
			{
				fixed_t power = cmd->forwardmove * scale;
				pl->mo->momx += FixedMul(power, finecosine[angle]);
				pl->mo->momy += FixedMul(power, finesine[angle]);
			}
		}

		if(cmd->sidemove)
		{
			fixed_t power = cmd->sidemove * scale;
			pl->mo->momx += FixedMul(power, finesine[angle]);
			pl->mo->momy -= FixedMul(power, finecosine[angle]);
		}

		if((cmd->forwardmove || cmd->sidemove) && pl->mo->animation == ANIM_SPAWN)
			mobj_set_animation(pl->mo, ANIM_SEE);

		if(pl->info_flags & PLF_MOUSE_LOOK && !(map_level_info->flags & MAP_FLAG_NO_FREELOOK))
		{
			if(cmd->pitchturn)
			{
				int32_t pitch = pl->mo->pitch + (int32_t)(cmd->pitchturn << 16);
				if(pitch > PLAYER_LOOK_TOP)
					pitch = PLAYER_LOOK_TOP;
				if(pitch < PLAYER_LOOK_BOT)
					pitch = PLAYER_LOOK_BOT;
				pl->mo->pitch = (angle_t)pitch;
			}
		} else
			pl->mo->pitch = 0;
	} else
		pl->mo->reactiontime--;

	P_CalcHeight(pl);

	if(pl->mo->subsector->sector->special)
	{
		if(map_format == MAP_FORMAT_DOOM)
		{
			uint32_t special = pl->mo->subsector->sector->special;
			P_PlayerInSpecialSector(pl);
			if(special == 9 && !pl->mo->subsector->sector->special)
			{
				pl->message = "Secret!";
				if(idx == consoleplayer)
					S_StartSound(NULL, SFX_SECRET);
			}
		} else
		{
			sector_t *sec = pl->mo->subsector->sector;
			if(pl->mo->z == sec->floorheight)
			{
				if(sec->special & 1024)
				{
					pl->message = "Secret!";
					if(idx == consoleplayer)
						S_StartSound(NULL, SFX_SECRET);
					sec->special &= ~1024;
				}
				if(sec->extra->damage.amount)
					player_sector_damage(pl, sec->extra);
			}
		}
	}

	if(pl->inv_tick)
		pl->inv_tick--;

	if(cmd->buttons & BT_SPECIAL)
	{
		if(cmd->buttons & BTS_PLAYER_FLAG)
		{
			// change player flag
			uint32_t flag = 1 << (cmd->buttons & 31);
			player_info_t *pli = player_info + idx;

			if(cmd->buttons & BTS_FLAG_SET)
				pli->flags |= flag;
			else
				pli->flags &= ~flag;

			pl->info_flags = pli->flags;
		}
		cmd->buttons = 0;
	}

	check_buttons(pl, cmd);

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

	// powers
	pl->fixedcolormap = 0;
	pl->fixedpalette = 0;
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
			{
				uint8_t color = pl->power_color[i];

				// TODO: MFE_INVENTORY_NOSCREENBLINK
				if(color && (pl->powers[i] > 128 || pl->powers[i] & 8))
				{
					if(color & 0x80)
					{
						if(!pl->fixedpalette)
							pl->fixedpalette = color & 63;
					} else
					{
						if(!pl->fixedcolormap)
							pl->fixedcolormap = color & 63;
					}
				}
			}
		}
	}

	if(pl->damagecount) // this IS done in 'P_DeathThink'
		pl->damagecount--;
}

//
// input

__attribute((regparm(2),no_caller_saved_registers))
static void build_ticcmd(ticcmd_t *cmd)
{
	static uint8_t mouse_inv_use;

	// do nothing in titlemap
	if(is_title_map)
	{
		memset(cmd, 0, sizeof(ticcmd_t));
		return;
	}

	// first, use the original function
	G_BuildTiccmd(cmd);

	// second, modify stuff
	cmd->chatchar = 0;

	// cheat char
	if(!paused)
		cmd->chatchar = HU_dequeueChatChar();

	// do nothing on special request
	if(cmd->buttons & BT_SPECIAL)
		return;

	// check for menu changes
	if(player_flags_changed)
	{
		// send info change over ticcmd
		// can send only one change at the time
		player_info_t *pli = player_info + consoleplayer;

		if((pli->flags & PLF_AUTO_SWITCH) != (extra_config.auto_switch << plf_auto_switch))
		{
			cmd->buttons = BT_SPECIAL | BTS_PLAYER_FLAG;
			if(extra_config.auto_switch)
				cmd->buttons |= BTS_FLAG_SET;
			cmd->buttons |= plf_auto_switch;
			return;
		} else
		if((pli->flags & PLF_AUTO_AIM) != (extra_config.auto_aim << plf_auto_aim))
		{
			cmd->buttons = BT_SPECIAL | BTS_PLAYER_FLAG;
			if(extra_config.auto_aim)
				cmd->buttons |= BTS_FLAG_SET;
			cmd->buttons |= plf_auto_aim;
			return;
		} else
		if((pli->flags & PLF_MOUSE_LOOK) != (!!(extra_config.mouse_look) << plf_mouse_look))
		{
			cmd->buttons = BT_SPECIAL | BTS_PLAYER_FLAG;
			if(extra_config.mouse_look)
				cmd->buttons |= BTS_FLAG_SET;
			cmd->buttons |= plf_mouse_look;
			return;
		}

		// clear after all flags are updated
		player_flags_changed = 0;
	}

	// mouse look
	cmd->pitchturn = mousey * 8;
	mousey = 0;

	// use (mouse)
	if(mousebuttons[mouseb_use])
		cmd->buttons |= BT_USE;

	// secondary attack
	if(mousebuttons[mouseb_fire_alt] || gamekeydown[key_fire_alt])
		cmd->buttons |= BT_ALTACK;

	// mask off weapon chagnes
	cmd->buttons &= BT_ATTACK | BT_USE | BT_ALTACK;

	// clear mouse use
	if(!gamekeydown[key_inv_use] && !mousebuttons[mouseb_inv_use])
		mouse_inv_use = 0;

	// action keys can not be combined

	// new weapon changes
	for(uint32_t i = 0; i < NUM_WPN_SLOTS; i++)
	{
		if(gamekeydown['0' + i])
		{
			gamekeydown['0' + i] = 0;
			cmd->buttons |= (i + 1) << BT_ACTIONSHIFT;
			// no inventory input
			return;
		}
	}

	if(gamekeydown[key_jump])
	{
		gamekeydown[key_jump] = 0;
		cmd->buttons |= BT_ACT_JUMP << BT_ACTIONSHIFT;
	} else
	if(gamekeydown[key_inv_use] || (!mouse_inv_use && mousebuttons[mouseb_inv_use]))
	{
		mouse_inv_use = 1;
		gamekeydown[key_inv_use] = 0;
		cmd->buttons |= BT_ACT_INV_USE << BT_ACTIONSHIFT;
	} else
	if(gamekeydown[key_inv_prev])
	{
		gamekeydown[key_inv_prev] = 0;
		cmd->buttons |= BT_ACT_INV_PREV << BT_ACTIONSHIFT;
	} else
	if(gamekeydown[key_inv_next])
	{
		gamekeydown[key_inv_next] = 0;
		cmd->buttons |= BT_ACT_INV_NEXT << BT_ACTIONSHIFT;
	}
}

//
// level transition

void player_finish(player_t *pl)
{
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

	if(pl->mo && pl->mo->inventory)
	{
		Z_ChangeTag2(pl->mo->inventory, PU_STATIC);
		inventory_hubstrip(pl->mo);
		pl->inventory = pl->mo->inventory;
		pl->mo->inventory = NULL;
		pl->angle = pl->mo->angle;
		pl->pitch = pl->mo->pitch;
	} else
		pl->inventory = NULL;
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t spawn_player(mapthing_t *mt)
{
	uint32_t angle;
	angle = (ANG45 / 45) * mt->angle;
	mobj_spawn_player(mt->type - 1, mt->x * FRACUNIT, mt->y * FRACUNIT, angle);
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t respawn_check(uint32_t idx)
{
	if(map_level_info->flags & MAP_FLAG_ALLOW_RESPAWN)
	{
		player_t *pl = players + idx;
		pl->inventory = pl->mo->inventory;
		pl->mo->inventory = NULL;
		reborn_inventory_hack = 1;
		return 1;
	}
	if(netgame)
		return 1;

	gameaction = ga_loadlevel;
	load_auto();

	return 0;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace 'P_SpawnPlayer'
	{0x000317F0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)spawn_player},
	// replace call to 'P_PlayerThink' in 'P_Ticker'
	{0x00032FBE, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)player_think},
	// replace call to 'P_CalcHeight' in 'P_DeathThink'
	{0x000332BF, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_CalcHeight},
	// replace netgame check in 'G_DoReborn'
	{0x00020C57, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)respawn_check},
	{0x00020C5C, CODE_HOOK | HOOK_UINT16, 0xC085},
	{0x00020C60, CODE_HOOK | HOOK_UINT16, 0x08EB},
	{0x00020C71, CODE_HOOK | HOOK_UINT8, 0xF5},
	// replace call to 'G_BuildTiccmd'
	{0x0001D5B2, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)build_ticcmd},
	{0x0001F220, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)build_ticcmd},
	// disable 'mousey' in 'G_BuildTiccmd'
	{0x0001FF60, CODE_HOOK | HOOK_SET_NOPS, 7},
	{0x0001FF94, CODE_HOOK | HOOK_SET_NOPS, 5},
	// disable 'consistancy' check in 'G_Ticker'
	{0x000206BB, CODE_HOOK | HOOK_UINT16, 0x7CEB},
	// remove call to 'HU_dequeueChatChar' in 'G_BuildTiccmd'
	{0x0001FD81, CODE_HOOK | HOOK_SET_NOPS, 8},
	// change 'BT_SPECIAL' check in 'G_Ticker'
	{0x0002079A, CODE_HOOK | HOOK_UINT8, BT_SPECIALMASK},
	// use 'fixedpalette' in 'ST_doPaletteStuff'
	{0x0003A475, CODE_HOOK | HOOK_UINT16, 0x9B8B},
	{0x0003A477, CODE_HOOK | HOOK_UINT32, offsetof(player_t,fixedpalette)},
	{0x0003A47B, CODE_HOOK | HOOK_UINT16, 0x10EB},
};

