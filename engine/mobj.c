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
#include "ldr_flat.h"
#include "mobj.h"
#include "sight.h"
#include "action.h"
#include "player.h"
#include "weapon.h"
#include "hitscan.h"
#include "special.h"
#include "terrain.h"
#include "map.h"
#include "stbar.h"
#include "sound.h"
#include "render.h"
#include "mover.h"
#include "demo.h"
#include "cheat.h"
#include "extra3d.h"

#define STOPSPEED	0x1000
#define FRICTION	0xE800
#define FRICTION_WATER	0xD000
#define FLOATSPEED	(FRACUNIT * 4)
#define SINK_SPEED	(FRACUNIT / 2)

static line_t *ceilingline;
static line_t *floorline;

static line_t *specbump[MAXSPECIALBUMP];
static uint32_t numspecbump;

uint32_t mo_puff_type = 37;
uint32_t mo_puff_flags;

uint_fast8_t reborn_inventory_hack;

uint32_t mobj_netid;

static uint_fast8_t teleblock;

static fixed_t oldfloorz;
mobj_t *mobj_hit_thing;

//
// state changes

static __attribute((regparm(3),no_caller_saved_registers)) // three!
uint32_t mobj_change_state(mobj_t *mo, uint32_t state, uint16_t extra)
{
	// defer state change for later
	mo->next_state = state;
	mo->next_extra = extra;
}

static __attribute((regparm(3),no_caller_saved_registers)) // three!
uint32_t P_SetMobjState(mobj_t *mo, uint32_t state, uint16_t extra)
{
	// normal state changes
	state_t *st;

	while(1)
	{
		switch(extra & STATE_CHECK_MASK)
		{
			case STATE_SET_OFFSET:
				state += mo->state - states;
				if(state >= num_states)
					state = 0;
			break;
			case STATE_SET_CUSTOM:
				state = dec_mobj_custom_state(mo->info, state);
				state += extra & STATE_EXTRA_MASK;
				if(state >= num_states)
					state = 0;
			break;
			case STATE_SET_ANIMATION:
				mo->animation = extra & STATE_EXTRA_MASK;
				if(state == 0xFFFFFFFF)
					state = dec_resolve_animation(mo->info, 0, mo->animation, num_states);
			break;
		}

		if(!state)
		{
			mobj_remove(mo);
			return 0;
		}

		st = states + state;
		mo->state = st;
		if(st->sprite != 0xFFFF)
		{
			mo->sprite = st->sprite;
			mo->frame = st->frame & ~FF_NODELAY;
		}

		if(st->tics != 0xFFFF)
		{
			if(st->tics > 1 && (fastparm || gameskill == sk_nightmare) && st->frame & FF_FAST)
				mo->tics = (st->tics + 1) >> 1;
			else
				mo->tics = st->tics;
		} else
			mo->tics = -1;

		extra = st->next_extra;
		state = st->nextstate;

		if(st->acp)
		{
			st->acp(mo, st, mobj_change_state);
			if(mo->thinker.function == (void*)-1)
				return 0;
			if(mo->next_state || mo->next_extra)
			{
				// apply deferred state change
				state = mo->next_state;
				extra = mo->next_extra;
				mo->next_state = 0;
				mo->next_extra = 0;
				continue;
			}
		}

		if(mo->tics)
			break;
	}

	return 1;
}

__attribute((regparm(2),no_caller_saved_registers))
void mobj_set_animation(mobj_t *mo, uint8_t anim)
{
	P_SetMobjState(mo, 0xFFFFFFFF, anim | STATE_SET_ANIMATION);
}

__attribute((regparm(3),no_caller_saved_registers))
static uint32_t mobj_inv_change(mobj_t *mo, uint32_t state, uint16_t extra)
{
	// defer state change for later
	mo->custom_extra = extra;
	mo->custom_state = state;
}

static uint32_t mobj_inv_loop(mobj_t *mo, uint32_t state)
{
	// state set by custom inventory
	state_t *st;
	uint32_t oldstate = 0;
	uint16_t extra = 0;

	mo->custom_state = 0;

	while(1)
	{
		state = dec_reslove_state(mo->custom_inventory, oldstate, state, extra);

		if(state <= 1)
		{
			mo->custom_inventory = NULL;
			return !state;
		}

		oldstate = state;
		st = states + state;

		extra = st->next_extra;
		state = st->nextstate;

		if(st->acp)
		{
			st->acp(mo, st, mobj_inv_change);
			if(mo->custom_state)
			{
				// apply deferred state change
				extra = mo->custom_extra;
				state = mo->custom_state;
				mo->custom_state = 0;
			}
		}
	}
}

//
// nightmare

static inline void check_nightmare(mobj_t *mo)
{
	if(!respawnmonsters)
		return;

	if(mo->spawnpoint.options != mo->type)
		return;

	if(!(mo->flags1 & MF1_ISMONSTER))
		return;

	if(!(mo->flags & MF_CORPSE))
		return;

	mo->movecount++;
	if(mo->movecount < 12*35)
		return;

	if(leveltime & 31)
		return;

	if(P_Random() > 4)
		return;

	P_NightmareRespawn(mo);
}

//
// inventory handling

static uint32_t give_ammo(mobj_t *mo, uint16_t type, uint16_t count, uint32_t dropped)
{
	uint32_t left;

	if(!type)
		return 0;

	if(dropped)
	{
		count /= 2;
		if(!count)
			count = 1;
	}

	if(	!(mobjinfo[type].eflags & MFE_INVENTORY_IGNORESKILL) &&
		(gameskill == sk_baby || gameskill == sk_nightmare)
	)
		count *= 2;

	left = inventory_give(mo, type, count);

	return left < count;
}

uint32_t mobj_give_health(mobj_t *mo, uint32_t count, uint32_t maxhp)
{
	if(maxhp)
		maxhp = maxhp;
	else
		maxhp = mo->info->spawnhealth;

	if(mo->player)
	{
		// Voodoo Doll ...
		if(mo->player->health >= maxhp)
			return 0;

		mo->player->health += count;
		if(mo->player->health > maxhp)
			mo->player->health = maxhp;
		mo->health = mo->player->health;

		return 1;
	}

	if(mo->health >= maxhp)
		return 0;

	mo->health += count;
	if(mo->health > maxhp)
		mo->health = maxhp;

	return 1;
}

static uint32_t give_armor(mobj_t *mo, mobjinfo_t *info)
{
	player_t *pl = mo->player;

	if(!pl)
		return 0;

	if(info->extra_type == ETYPE_ARMOR)
	{
		if(pl->armorpoints >= info->armor.count)
			return 0;

		pl->armorpoints = info->armor.count;
		pl->armortype = info - mobjinfo;

		return 1;
	} else
	if(info->extra_type == ETYPE_ARMOR_BONUS)
	{
		if(pl->armorpoints >= info->armor.max_count)
			return 0;

		pl->armorpoints += info->armor.count;
		if(pl->armorpoints > info->armor.max_count)
			pl->armorpoints = info->armor.max_count;

		if(!pl->armortype)
			pl->armortype = info - mobjinfo;

		return 1;
	}

	return 0;
}

static uint32_t give_power(mobj_t *mo, mobjinfo_t *info)
{
	uint32_t duration;

	if(!mo->player)
		return 1;

	if(info->powerup.type >= NUMPOWERS)
		return 1;

	if(info->powerup.duration < 0)
		duration = info->powerup.duration * -35;
	else
		duration = info->powerup.duration;

	if(!duration)
		return 1;

	if(!(info->eflags & MFE_INVENTORY_ALWAYSPICKUP) && mo->player->powers[info->powerup.type])
		return 0;

	powerup_give(mo->player, info);

	if(info->eflags & MFE_INVENTORY_ADDITIVETIME)
		mo->player->powers[info->powerup.type] += duration;
	else
		mo->player->powers[info->powerup.type] = duration;

	return 1;
}

static uint32_t give_special(mobj_t *mo, mobjinfo_t *info)
{
	if(!mo->player)
		return 0;

	switch(info->inventory.special)
	{
		case 0:
			// backpack
			if(!mo->player->backpack)
			{
				mo->player->backpack = 1;
				mo->player->stbar_update |= STU_BACKPACK;
			}
			// give all existing ammo
			for(uint32_t idx = 0; idx < num_mobj_types; idx++)
			{
				mobjinfo_t *info = mobjinfo + idx;
				if(info->extra_type == ETYPE_AMMO)
					inventory_give(mo, idx, info->ammo.count);
			}
		break;
		case 1:
			// map
			if(mo->player->powers[pw_allmap])
				// original behaviour - different than ZDoom
				return 0;
			mo->player->powers[pw_allmap] = 1;
		break;
		case 2:
			// megasphere
			mo->health = dehacked.hp_megasphere;
			mo->player->health = mo->health;
			if(mo->player->armorpoints < 200)
			{
				mo->player->armorpoints = 200;
				mo->player->armortype = 44;
			}
		break;
		case 3:
			// berserk
			mobj_give_health(mo, 100, 0);
			mo->player->powers[pw_strength] = 1;
			if(mo->player->readyweapon != mobjinfo + MOBJ_IDX_FIST && inventory_check(mo, MOBJ_IDX_FIST))
				mo->player->pendingweapon = mobjinfo + MOBJ_IDX_FIST;
		break;
	}

	return 1;
}

static uint32_t pick_custom_inv(mobj_t *mo, mobjinfo_t *info)
{
	uint32_t ret;

	if(!info->st_custinv.pickup)
		return 1;

	if(mo->custom_inventory)
		engine_error("MOBJ", "Nested CustomInventory is not supported!");

	mo->custom_inventory = info;
	ret = mobj_inv_loop(mo, info->st_custinv.pickup);

	if(info->eflags & MFE_INVENTORY_ALWAYSPICKUP)
		return 1;

	return ret;
}

static uint32_t use_custom_inv(mobj_t *mo, mobjinfo_t *info)
{
	if(!info->st_custinv.use)
		return 1;

	if(mo->custom_inventory)
		engine_error("MOBJ", "Nested CustomInventory is not supported!");

	mo->custom_inventory = info;
	return mobj_inv_loop(mo, info->st_custinv.use);
}

//
// thing pickup

static void touch_mobj(mobj_t *mo, mobj_t *toucher)
{
	mobjinfo_t *info;
	player_t *pl;
	uint32_t left, given;
	fixed_t diff;
	uint32_t do_remove = 1;

	if(!toucher->player || toucher->player->state != PST_LIVE)
		return;

	diff = mo->z - toucher->z;
	if(diff > toucher->height || diff < -mo->height)
		return;

	pl = toucher->player;
	info = mo->info;

	if(mo->type < NUMMOBJTYPES)
	{
		// old items - type is based on current sprite
		uint16_t type;

		// forced height
		if(diff < 8 * -FRACUNIT)
			return;

		// DEHACKED workaround
		if(mo->sprite < sizeof(deh_pickup_type))
			type = deh_pickup_type[mo->sprite];
		else
			type = 0;

		if(!type)
		{
			// original game would throw an error
			mo->flags &= ~MF_SPECIAL;
			// heheh
			if(!(mo->flags & MF_NOGRAVITY))
				mo->momz += 8 * FRACUNIT;
			return;
		}

		info = mobjinfo + type;
	}

	// new inventory stuff

	switch(info->extra_type)
	{
		case ETYPE_HEALTH:
			// health pickup
			if(!mobj_give_health(toucher, info->inventory.count, info->inventory.max_count) && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				// can't pickup
				return;
		break;
		case ETYPE_INV_SPECIAL:
			// netgame inventory drop (hack)
			if(mo->iflags & MFI_PLAYER_DROP)
			{
				if(!toucher->player)
					return;
				if(!mo->inventory)
					return;
				if(	net_inventory > 2 &&
					mo->threshold >= 0 &&
					toucher->player - players != mo->threshold
				)
					return;
				// backpack
				toucher->player->backpack |= mo->reactiontime & 1;
				// move items
				for(uint32_t i = 0; i < mo->inventory->numslots; i++)
				{
					invitem_t *item = mo->inventory->slot + i;
					if(item->type)
						inventory_give(toucher, item->type, item->count);
				}
				// update status bar
				toucher->player->stbar_update |= STU_EVERYTHING;
			} else
			// special pickup type
			if(!give_special(toucher, info))
				// can't pickup
				return;
		break;
		case ETYPE_INVENTORY:
			// add to inventory
			if(inventory_give(toucher, mo->type, info->inventory.count) >= info->inventory.count)
				// can't pickup
				return;
		break;
		case ETYPE_INVENTORY_CUSTOM:
			// pickup
			if(!pick_custom_inv(toucher, info))
				// can't pickup
				return;
			// check for 'use'
			if(info->st_custinv.use)
			{
				given = 0;
				// autoactivate
				if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
					given = use_custom_inv(toucher, info);
				// give as item
				if(!given)
					given = inventory_give(toucher, mo->type, info->inventory.count) < info->inventory.count;
				// check
				if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
					return;
			}
		break;
		case ETYPE_WEAPON:
			// weapon
			if(	weapons_stay &&
				mo->spawnpoint.options == mo->type
			){
				// TODO: this does not work well for multi-weapons
				if(inventory_check(toucher, mo->type) >= mobjinfo[mo->type].inventory.max_count)
					return;
				do_remove = 0;
			}
			given = inventory_give(toucher, mo->type, info->inventory.count);
			if(!given)
			{
				pl->stbar_update |= STU_WEAPON_NEW; // evil grin
				// auto-weapon-switch optional
				if(	pl->readyweapon != info &&
					player_info[pl - players].flags & PLF_AUTO_SWITCH
				)
					pl->pendingweapon = info;
				if(!pl->psprites[0].state)
					// fix 'no weapon' state
					weapon_setup(pl);
			}
			given = given < info->inventory.count;
			// primary ammo
			given |= give_ammo(toucher, info->weapon.ammo_type[0], info->weapon.ammo_give[0], mo->flags & MF_DROPPED);
			// secondary ammo
			given |= give_ammo(toucher, info->weapon.ammo_type[1], info->weapon.ammo_give[1], mo->flags & MF_DROPPED);
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return;
		break;
		case ETYPE_AMMO:
		case ETYPE_AMMO_LINK:
			// add ammo to inventory
			if(	!give_ammo(toucher, mo->type, info->inventory.count, mo->flags & MF_DROPPED) &&
				!(info->eflags & MFE_INVENTORY_ALWAYSPICKUP)
			)
				// can't pickup
				return;
		break;
		case ETYPE_KEY:
			if(	netgame &&
				mo->spawnpoint.options == mo->type
			){
				if(inventory_check(toucher, mo->type))
					return;
				do_remove = 0;
			}
			// add to inventory
			inventory_give(toucher, mo->type, 1);
		break;
		case ETYPE_ARMOR:
		case ETYPE_ARMOR_BONUS:
			given = 0;
			// autoactivate
			if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
				given = give_armor(toucher, info);
			// give as item
			if(!given)
				given = inventory_give(toucher, mo->type, info->inventory.count) < info->inventory.count;
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return;
		break;
		case ETYPE_POWERUP:
			given = 0;
			// autoactivate
			if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
				given = give_power(toucher, info);
			// give as item
			if(!given)
				given = inventory_give(toucher, mo->type, info->inventory.count) < info->inventory.count;
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return;
		break;
		case ETYPE_HEALTH_PICKUP:
			given = 0;
			// autoactivate
			if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
				given = mobj_give_health(toucher, info->spawnhealth, toucher->info->spawnhealth);
			// give as item
			if(!given)
				given = inventory_give(toucher, mo->type, info->inventory.count) < info->inventory.count;
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return;
		break;
		default:
			// this should not be set
			mo->flags &= ~MF_SPECIAL;
		return;
	}

	// count
	if(mo->flags & MF_COUNTITEM)
	{
		pl->itemcount++;
		mo->flags &= ~MF_COUNTITEM;
	}

	if(do_remove)
	{
		// activate special
		if(mo->special.special)
		{
			spec_special = mo->special.special;
			spec_arg[0] = mo->special.arg[0];
			spec_arg[1] = mo->special.arg[1];
			spec_arg[2] = mo->special.arg[2];
			spec_arg[3] = mo->special.arg[3];
			spec_arg[4] = mo->special.arg[4];
			spec_activate(NULL, toucher, 0);
			mo->special.special = 0;
		}

		// remove / hide
		P_SetMobjState(mo, STATE_SPECIAL_HIDE, 0);
	}

	if(info->eflags & MFE_INVENTORY_QUIET)
		return;

	// flash
	if(!(info->eflags & MFE_INVENTORY_NOSCREENFLASH))
	{
		pl->bonuscount += 6;
		if(pl->bonuscount > 24)
			pl->bonuscount = 24; // new limit
	}

	// sound
	S_StartSound(SOUND_CONSOLEPLAYER(pl), info->inventory.sound_pickup);

	// message
	if(info->inventory.message)
		pl->message = info->inventory.message;
}

//
// player spawn

mobj_t *mobj_spawn_player(uint32_t idx, fixed_t x, fixed_t y, angle_t angle)
{
	player_t *pl;
	mobj_t *mo;
	mobjinfo_t *info;

	if(!playeringame[idx])
		return NULL;

	pl = players + idx;
	if(!is_title_map)
	{
		info = mobjinfo + player_class[player_info[idx].playerclass];
		if(pl->cheats & CF_CHANGE_CLASS)
		{
			pl->cheats &= ~CF_CHANGE_CLASS;
			pl->state = PST_REBORN;
			reborn_inventory_hack = 0;
		}
	} else
		info = mobjinfo; // default to 'DoomPlayer'

	// create body
	mo = P_SpawnMobj(x, y, 0x80000000, player_class[player_info[idx].playerclass]);
	mo->angle = angle;

	if(pl->inventory)
		Z_ChangeTag2(pl->inventory, PU_LEVEL_INV);

	// check for reset
	if(	pl->state == PST_REBORN ||
		pl->state == PST_SPECTATE ||
		map_level_info->flags & MAP_FLAG_RESET_INVENTORY
	){
		// cleanup
		uint32_t killcount;
		uint32_t itemcount;
		uint32_t secretcount;
		uint32_t is_cheater;
		uint32_t extra_inv;
		int32_t inv_sel;
		inventory_t *inventory;
		mobjinfo_t *weapon;

		if(reborn_inventory_hack)
		{
			weapon = pl->readyweapon;
			inventory = pl->inventory;
			inv_sel = pl->inv_sel;
			extra_inv = pl->backpack;
			extra_inv |= !!pl->powers[pw_allmap] << 1;
		} else
		{
			inventory = NULL;
			if(pl->inventory)
				Z_Free(pl->inventory);
		}

		killcount = pl->killcount;
		itemcount = pl->itemcount;
		secretcount = pl->secretcount;
		is_cheater = pl->cheats & CF_IS_CHEATER;

		memset(pl, 0, sizeof(player_t));

		pl->killcount = killcount;
		pl->itemcount = itemcount;
		pl->secretcount = secretcount;
		pl->cheats = is_cheater;

		pl->usedown = 1;
		pl->attackdown = 1;
		pl->state = PST_LIVE;

		if(info == mobjinfo)
			pl->health = dehacked.start_health;
		else
			pl->health = info->spawnhealth;

		pl->pendingweapon = NULL;
		pl->readyweapon = NULL;
		mo->inventory = inventory;

		if(!inventory || reborn_inventory_hack > 1)
		{
			pl->inv_sel = -1;

			pl->readyweapon = NULL;

			// required for inventory allocation
			pl->mo = mo;
			mo->player = pl;

			// default inventory
			for(plrp_start_item_t *si = info->start_item.start; si < (plrp_start_item_t*)info->start_item.end; si++)
			{
				mobj_give_inventory(mo, si->type, si->count);
				if(!pl->pendingweapon)
				{
					if(mobjinfo[si->type].extra_type == ETYPE_WEAPON)
						pl->pendingweapon = mobjinfo + si->type;
				}
			}

			pl->readyweapon = pl->pendingweapon;

			if(deathmatch)
			{
				for(uint32_t i = 0; i < num_mobj_types; i++)
				{
					mobjinfo_t *info = mobjinfo + i;
					if(info->extra_type == ETYPE_KEY)
						inventory_give(mo, i, INV_MAX_COUNT);
				}
			}
		} else
		{
			pl->inv_sel = inv_sel;
			pl->readyweapon = weapon;
			pl->pendingweapon = weapon;
			pl->backpack = extra_inv & 1;
			pl->powers[pw_allmap] = (extra_inv >> 1) & 1;
		}

		reborn_inventory_hack = 0;
	} else
	{
		if(pl->inventory)
			// use existing inventory
			mo->inventory = pl->inventory;
		else
		if(pl->mo)
		{
			// voodoo doll ...
			mo->inventory = pl->mo->inventory;
			pl->mo->inventory = NULL;
		}
		if(map_start_facing)
		{
			mo->angle = pl->angle;
			mo->pitch = pl->pitch;
		}
	}

	mo->player = pl;
	mo->health = pl->health;

	pl->mo = mo;
	pl->camera = mo;
	pl->state = PST_LIVE;
	pl->refire = 0;
	pl->message = NULL;
	pl->damagecount = 0;
	pl->bonuscount = 0;
	pl->extralight = 0;
	pl->fixedcolormap = 0;
	pl->inventory = NULL;
	pl->stbar_update = 0;
	pl->inv_tick = 0;
	pl->text.text = NULL;
	pl->viewheight = mo->info->player.view_height;
	pl->airsupply = PLAYER_AIRSUPPLY;

	memset(&pl->cmd, 0, sizeof(ticcmd_t));

	if(!(pl->mo->flags2 & MF2_DONTTRANSLATE))
		pl->mo->translation = r_generate_player_color(idx);

	cheat_player_flags(pl);

	weapon_setup(pl);

	if(idx == consoleplayer)
	{
		stbar_start(pl);
		HU_Start();
	}

	return mo;
}

//
// stuff

static __attribute((regparm(2),no_caller_saved_registers))
void set_mobj_state(mobj_t *mo, uint32_t state)
{
	P_SetMobjState(mo, state, 0);
}

static __attribute((regparm(2),no_caller_saved_registers))
void set_mobj_animation(mobj_t *mo, uint8_t anim)
{
	if(anim == ANIM_HEAL)
	{
		if(!mo->info->state_heal)
			return;
	}
	if(anim == ANIM_RAISE)
	{
		if(mo->state - states == 895)
			mo->translation = mo->info->translation;
	}
	mobj_set_animation(mo, anim);
}

static __attribute((regparm(2),no_caller_saved_registers))
mobjinfo_t *prepare_mobj(mobj_t *mo, uint32_t type)
{
	mobjinfo_t *info = mobjinfo + type;
	uint32_t hack = 0;
	uint32_t rng = 0;

	// check for replacement
	if(info->replacement)
	{
		type = info->replacement;
		info = mobjinfo + type;
	}

	// check for random
	while(info->extra_type == ETYPE_RANDOMSPAWN)
	{
		uint32_t weight = 0;
		int32_t type = MOBJ_IDX_UNKNOWN;
		uint32_t rnd = P_Random() % info->random_weight;
		uint32_t chance;

		if(rng >= 16)
			engine_error("MOBJ", "Possible recursive RandomSpawner!");

		for(mobj_dropitem_t *drop = info->dropitem.start; drop < (mobj_dropitem_t*)info->dropitem.end; drop++)
		{
			if(drop->amount)
				weight += drop->amount;
			else
				weight += 1;

			type = drop->type;
			chance = drop->chance;

			if(weight > rnd)
				break;
		}

		if(chance < 255 && chance > P_Random())
			type = MOBJ_IDX_UNKNOWN;

		if(type == MOBJ_IDX_UNKNOWN)
		{
			// spawn invisible item that will remove itself instantly
			type = MOBJ_IDX_ICE_CHUNK_HEAD;
			hack = 1;
		}

		info = mobjinfo + type;

		// one extra round of replacement
		if(info->replacement)
		{
			type = info->replacement;
			info = mobjinfo + type;
		}

		rng++;
	}

	// clear memory
	memset(mo, 0, sizeof(mobj_t));

	// fill in new stuff
	mobj_netid++;
	mo->type = type;
	mo->render_style = info->render_style;
	mo->render_alpha = info->render_alpha;
	mo->flags1 = info->flags1;
	mo->flags2 = info->flags2;
	mo->translation = info->translation;
	mo->gravity = info->gravity;
	mo->scale = info->scale;
	mo->damage_type = info->damage_type;
	mo->bounce_count = info->bounce_count;
	mo->netid = mobj_netid;

	for(uint32_t i = 0; i < info->args[5]; i++)
		mo->special.arg[i] = info->args[i];

	// vertical speed
	mo->momz = info->vspeed;

	// random item hack
	if(hack)
		mo->render_style = RS_INVISIBLE;

	// return offset
	return info;
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t finish_mobj(mobj_t *mo)
{
	// add thinker
	P_AddThinker(&mo->thinker);

	// spawn inactive
	if(mo->flags1 & MF1_DORMANT)
		mo->tics = -1;

	// check for extra floors
	if(mo->subsector->sector->exfloor)
	{
		tmfloorz = mo->subsector->sector->floorheight;
		tmceilingz = mo->subsector->sector->ceilingheight;
		e3d_check_heights(mo, mo->subsector->sector, 1);
		mo->floorz = tmextrafloor;
		mo->ceilingz = tmextraceiling;
		e3d_check_water(mo);
	}

	// save current sector
	// in ZDoom, spawned things do not trigger 'enter' sector action
	mo->old_sector = mo->subsector->sector;

	// stealth
	if(mo->flags2 & MF2_STEALTH)
	{
		if(mo->render_alpha < mo->info->stealth_alpha)
			mo->alpha_dir = 14;
		else
		if(mo->render_alpha > mo->info->stealth_alpha)
			mo->alpha_dir = -10;
	}

	// counters
	if(mo->flags & MF_COUNTKILL)
		totalkills++;
	if(mo->flags & MF_COUNTITEM)
		totalitems++;

	// fix tics
	mo->tics = mo->state->tics == 0xFFFF ? -1 : mo->state->tics;

	// HACK - other sound slots
	mo->sound_body.x = mo->x;
	mo->sound_body.y = mo->y;
	mo->sound_weapon.x = mo->x;
	mo->sound_weapon.y = mo->y;

	// ZDoom compatibility
	// teleport fog starts teleport sound
	if(mo->type == 39)
		S_StartSound(mo, 35);

	// fix type - RandomSpawner
	mo->type = mo->info - mobjinfo;
}

__attribute((regparm(2),no_caller_saved_registers))
static void mobj_kill(mobj_t *mo, mobj_t *source)
{
	uint_fast8_t new_damage_type = DAMAGE_NORMAL;
	uint32_t state = 0;

	// TODO: frags

	if(mo->flags2 & MF2_STEALTH)
	{
		mo->render_alpha = 255;
		mo->alpha_dir = 0;
	}

	if(mo->flags & MF_COUNTKILL)
	{
		if(!netgame)
			players[0].killcount++;
		else
		if(source && source->player)
			source->player->killcount++;
	}

	mo->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY|MF_COUNTKILL);

	if(mo->flags & MF_MISSILE)
	{
		mo->momx = 0;
		mo->momy = 0;
		mo->momz = 0;
	} else
	{
		if(source)
			mo->target = source;

		if(!(mo->flags1 & MF1_DONTFALL))
			mo->flags &= ~MF_NOGRAVITY;

		if(mo->special.special && (!(mo->flags & MF_SPECIAL) || mo->flags1 & MF1_ISMONSTER))
		{
			spec_special = mo->special.special;
			spec_arg[0] = mo->special.arg[0];
			spec_arg[1] = mo->special.arg[1];
			spec_arg[2] = mo->special.arg[2];
			spec_arg[3] = mo->special.arg[3];
			spec_arg[4] = mo->special.arg[4];
			spec_activate(NULL, source, 0);
		}

		if(!(mo->flags1 & MF1_DONTFALL))
			mo->flags &= ~MF_NOGRAVITY;

		if(!(mo->flags2 & MF2_DONTCORPSE))
			mo->flags |= MF_CORPSE;

		mo->flags |= MF_DROPOFF;
		mo->height >>= 2;
	}

	// player stuff
	if(mo->player)
	{
		mo->flags &= ~MF_SOLID;
		mo->player->extralight = 0;
		mo->player->state = PST_DEAD;
		weapon_lower(mo->player);
		if(mo->player == &players[consoleplayer] && automapactive)
		    AM_Stop();

		mo->player->extralight = 0;

		if(net_inventory > 1 && mo->inventory)
		{
			inventory_t *inv = mo->inventory;
			mobj_t *thing;
			uint32_t i;

			// check inventory for droppable items
			for(i = 0; i < inv->numslots; i++)
			{
				invitem_t *item = inv->slot + i;
				mobjinfo_t *info = mobjinfo + item->type;

				if(!item->type)
					continue;

				if(!item->count)
					continue;

				if(info->eflags & MFE_INVENTORY_UNTOSSABLE)
					continue;

				if(keep_keys && info->extra_type == ETYPE_KEY)
					continue;

				break;
			}

			if(i < inv->numslots)
			{
				// spawn normal backpack
				thing = P_SpawnMobj(mo->x, mo->y, mo->z + (8 << FRACBITS), 71); // MT_MISC24 (backpack)
				thing->angle = P_Random() << 24;
				thing->momx = FRACUNIT - (P_Random() << 9);
				thing->momy = FRACUNIT - (P_Random() << 9);
				thing->momz = (4 << 16) + (P_Random() << 10);
				thing->iflags |= MFI_PLAYER_DROP; // override function
				thing->threshold = mo->player - players;
				thing->reactiontime = mo->player->backpack;
				if(!(thing->flags2 & MF2_DONTTRANSLATE))
					thing->translation = mo->translation;

				// move inventory pointer
				thing->inventory = mo->inventory;
				mo->inventory = NULL;
				mo->player->inv_sel = -1;
				mo->player->inv_tick = 0;
				mo->player->backpack = 0;
				mo->player->readyweapon = NULL;
				mo->player->pendingweapon = NULL;

				// move special items back
				// this is keeps 'resurrect' cheat working
				// this is also required for 'keep keys' flag
				for(uint32_t i = 0; i < inv->numslots; i++)
				{
					invitem_t *item = inv->slot + i;
					mobjinfo_t *info = mobjinfo + item->type;

					if(	info->eflags & MFE_INVENTORY_UNTOSSABLE ||
						(keep_keys && info->extra_type == ETYPE_KEY)
					)
					{
						// give
						inventory_give(mo, item->type, item->count);
						if(info->extra_type == ETYPE_WEAPON)
							mo->player->readyweapon = info;
						// clear original
						item->type = 0;
						item->count = 0;
					}
				}

				// update status bar
				mo->player->stbar_update |= STU_EVERYTHING;
			}
		}
	}

	// look for custom damage first
	if(mo->damage_type)
	{
		if(mo->health < -mo->info->spawnhealth)
			state = dec_mobj_custom_state(mo->info, damage_type_config[mo->damage_type].xdeath);
		if(!state)
			state = dec_mobj_custom_state(mo->info, damage_type_config[mo->damage_type].death);
		if(state)
			new_damage_type = mo->damage_type;
	}

	// check generic ice death
	if(	!state &&
		mo->damage_type == DAMAGE_ICE &&
		(mo->flags1 & MF1_ISMONSTER || mo->player) &&
		!(mo->flags2 & MF2_NOICEDEATH)
	){
		state = STATE_ICE_DEATH_0;
		new_damage_type = DAMAGE_ICE;
	}

	// look for normal damage now
	if(!state)
	{
		if(mo->health < -mo->info->spawnhealth)
			state = mo->info->state_xdeath;
		if(!state)
			state = mo->info->state_death;
	}

	mo->damage_type = new_damage_type;
	mo->animation = ANIM_DEATH; // or XDEATH? meh
	P_SetMobjState(mo, state, 0);

	if(mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	// spawn original drops
	P_KillMobj(source, mo); // this function was modified
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t pit_check_thing(mobj_t *thing, mobj_t *tmthing)
{
	uint32_t damage;
	uint32_t thsolid = 0;
	uint32_t tmsolid = 0;

	if(tmthing->flags2 & MF2_THRUACTORS)
		// NOTE: This should just skip PIT_CheckThing completely.
		return 1;

	if(tmthing->inside == thing)
		return 1;

	if(tmthing->player)
	{
		// players walk trough corpses in ZDoom
		// this is not the exact emulation ...
		if(!(thing->flags & MF_CORPSE) || !(thing->flags1 & MF1_ISMONSTER) || thing->flags2 & MF2_ICECORPSE)
			thsolid = thing->flags & MF_SOLID;
		if(!(tmthing->flags & MF_CORPSE) || !(tmthing->flags1 & MF1_ISMONSTER) || tmthing->flags2 & MF2_ICECORPSE)
			tmsolid = tmthing->flags & MF_SOLID;
	} else
	{
		thsolid = thing->flags & MF_SOLID;
		tmsolid = tmthing->flags & MF_SOLID;
	}

	if(/*map_format != MAP_FORMAT_DOOM && */thsolid && (!(tmthing->flags & MF_MISSILE) || tmthing->flags & MF_TELEPORT))
	{
		// thing-over-thing
		tmthing->iflags |= MFI_MOBJONMOBJ;
		thing->iflags |= MFI_MOBJONMOBJ;

		if(	(tmthing->z >= thing->z + thing->height) ||
			(tmthing->player && tmthing->z + tmthing->info->step_height >= thing->z + thing->height)
		){
			if(tmfloorz < thing->z + thing->height)
				tmfloorz = thing->z + thing->height;
			return 1;
		}

		if(tmthing->z + tmthing->height <= thing->z)
		{
			if(tmceilingz > thing->z)
				tmceilingz = thing->z;
			return 1;
		}
	}

	// ignore when teleporting
	if(tmthing->flags & MF_TELEPORT)
	{
		teleblock |= !!(thing->flags & MF_SOLID);
		teleblock |= thsolid;
		return 1;
	}

	if(tmthing->flags & MF_SKULLFLY)
	{
		damage = tmthing->info->damage;
		if(!(damage & DAMAGE_IS_MATH_FUNC))
			damage |= DAMAGE_IS_PROJECTILE;

		mobj_damage(thing, tmthing, tmthing, damage, NULL);

		tmthing->flags &= ~MF_SKULLFLY;
		tmthing->momx = 0;
		tmthing->momy = 0;
		tmthing->momz = 0;

		mobj_set_animation(tmthing, ANIM_SPAWN);

		return 0;
	}

	mobj_hit_thing = thing->flags & MF_SHOOTABLE ? thing : NULL;

	if(tmthing->flags & MF_MISSILE)
	{
		uint32_t is_ripper;
		uint32_t damage;

		if(tmthing->target == thing)
			return 1;

		if(tmthing->z >= thing->z + thing->height)
			return 1;

		if(tmthing->z + tmthing->height <= thing->z)
			return 1;

		if(thing->flags1 & MF1_SPECTRAL && !(tmthing->flags1 & MF1_SPECTRAL))
			return 1;

		if(thing->flags1 & MF1_GHOST && tmthing->flags1 & MF1_THRUGHOST)
			return 1;

		if(!(thing->flags & MF_SHOOTABLE))
			return !thsolid;

		if(	!dehacked.no_species &&
			(map_level_info->flags & (MAP_FLAG_TOTAL_INFIGHTING | MAP_FLAG_NO_INFIGHTING)) != MAP_FLAG_TOTAL_INFIGHTING &&
			tmthing->target && thing->info->extra_type != ETYPE_PLAYERPAWN
		){
			if(thing->info->species == tmthing->target->info->species)
				return 0;
		}

		if(!(thing->flags1 & MF1_DONTRIP) && tmthing->flags1 & MF1_RIPPER)
		{
			if(tmthing->rip_thing == thing && tmthing->rip_tick == leveltime)
				return 1;
			tmthing->rip_thing = thing;
			tmthing->rip_tick = leveltime;
			is_ripper = 1;
			damage = tmthing->info->damage;
			if(!(damage & DAMAGE_IS_MATH_FUNC))
				damage |= DAMAGE_IS_RIPPER;
		} else
		{
			is_ripper = 0;
			damage = tmthing->info->damage;
			if(!(damage & DAMAGE_IS_MATH_FUNC))
				damage |= DAMAGE_IS_PROJECTILE;
		}

		mobj_damage(thing, tmthing, tmthing->target, damage, NULL);

		if(is_ripper)
		{
			if(!(thing->flags & MF_NOBLOOD) && !(tmthing->flags2 & MF2_BLOODLESSIMPACT))
			{
				mobj_t *mo;
				mo = P_SpawnMobj(thing->x, thing->y, thing->z + thing->height / 2, thing->info->blood_type);
				mo->inside = thing;
				mo->momx = (P_Random() - P_Random()) << 12;
				mo->momy = (P_Random() - P_Random()) << 12;
				if(!(mo->flags2 & MF2_DONTTRANSLATE))
					mo->translation = thing->info->blood_trns;
			}
			if(thing->flags1 & MF1_PUSHABLE && !(tmthing->flags1 & MF1_CANNOTPUSH))
			{
				thing->momx += tmthing->momx / 2;
				thing->momy += tmthing->momy / 2;
			}
			return 1;
		}

		if(thing->flags1 & MF1_REFLECTIVE)
		{
			angle_t angle;
			fixed_t speed;

			if(tmthing->flags1 & MF1_SEEKERMISSILE)
				tmthing->tracer = tmthing->target;
			else
				tmthing->tracer = NULL;
			tmthing->target = thing;

			angle = R_PointToAngle2(thing->x, thing->y, tmthing->x, tmthing->y);

			if(tmthing->info->fast_speed && (fastparm || gameskill == sk_nightmare))
				speed = tmthing->info->fast_speed;
			else
				speed = tmthing->info->speed;

			tmthing->angle = angle;

			if(speed > 0x0800)
			{
				if(tmthing->momz)
				{
					angle_t pitch = slope_to_angle(FixedDiv(tmthing->momz, speed));
					pitch >>= ANGLETOFINESHIFT;
					speed /= 2;
					tmthing->momz = FixedMul(speed, finesine[pitch]);
					speed = FixedMul(speed, finecosine[pitch]);
				} else
					speed /= 2;

				angle >>= ANGLETOFINESHIFT;
				tmthing->momx = FixedMul(speed, finecosine[angle]);
				tmthing->momy = FixedMul(speed, finesine[angle]);
			}

			return 1;
		}

		return 0;
	}

	if(!tmsolid)
		// ZDoom: non-solid things are not blocked
		return 1;

	if(thing->flags1 & MF1_PUSHABLE && !(tmthing->flags1 & MF1_CANNOTPUSH))
	{
		thing->momx += tmthing->momx / 2;
		thing->momy += tmthing->momy / 2;
		if(thsolid && !(tmthing->flags & MF_SLIDE))
		{
			tmthing->momx = 0;
			tmthing->momy = 0;
		}
	}

	if(thing->flags & MF_SPECIAL)
	{
		uint32_t solid = thsolid;
		if(tmthing->flags & MF_PICKUP)
			touch_mobj(thing, tmthing);
		return !solid;
	}

	return !thsolid;
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t pit_check_line(mobj_t *tmthing, line_t *ld)
{
	uint32_t is_safe = 0;
	fixed_t z;

	mobj_hit_thing = NULL; // ideally, this would be outside

	if(!ld->backsector)
		goto blocked;

	if(ld->flags & ML_BLOCK_ALL)
		goto blocked;

	if(!(tmthing->flags & MF_MISSILE))
	{
		if(ld->flags & ML_BLOCKING)
			goto blocked;

		if(tmthing->player)
		{
			if(ld->flags & ML_BLOCK_PLAYER)
				goto blocked;
		} else
		{
			if(ld->flags & ML_BLOCKMONSTERS && !(tmthing->flags2 & MF2_NOBLOCKMONST))
				goto blocked;
		}
	}

	e3d_check_heights(tmthing, ld->frontsector, tmthing->flags & MF_MISSILE);

	if(tmceilingz > tmextraceiling)
		tmceilingz = tmextraceiling;
	if(tmfloorz < tmextrafloor)
		tmfloorz = tmextrafloor;

	if(	tmextradrop <= tmthing->info->dropoff ||
		ld->frontsector->floorheight >= tmthing->z - tmthing->info->dropoff
	)
		is_safe |= 1;

	e3d_check_heights(tmthing, ld->backsector, tmthing->flags & MF_MISSILE);

	if(tmceilingz > tmextraceiling)
		tmceilingz = tmextraceiling;
	if(tmfloorz < tmextrafloor)
		tmfloorz = tmextrafloor;

	if(	tmextradrop <= tmthing->info->dropoff ||
		ld->backsector->floorheight >= tmthing->z - tmthing->info->dropoff
	)
		is_safe |= 2;

	e3d_check_midtex(tmthing, ld, tmthing->flags & MF_MISSILE);

	if(tmceilingz > tmextraceiling)
		tmceilingz = tmextraceiling;
	if(tmfloorz < tmextrafloor)
		tmfloorz = tmextrafloor;

	P_LineOpening(ld);

	if(opentop < tmceilingz)
	{
		tmceilingz = opentop;
		ceilingline = ld;
	}

	if(openbottom > tmfloorz)
	{
		tmfloorz = openbottom;
		floorline = ld;
	}

	if(tmthing->z < tmfloorz)
		z = tmfloorz;
	else
		z = tmthing->z;

	if(e3d_check_inside(ld->frontsector, z, E3D_SOLID))
		goto blocked;

	if(e3d_check_inside(ld->backsector, z, E3D_SOLID))
		goto blocked;

	if(is_safe != 3 && lowfloor < tmdropoffz)
		tmdropoffz = lowfloor;

	if(	!(tmthing->flags & MF_TELEPORT) &&
		ld->special &&
		numspechit < MAXSPECIALCROSS
	){
		spechit[numspechit] = ld;
		numspechit++;
	}

	return 1;

blocked:
	// ignore when teleporting
	if(tmthing->flags & MF_TELEPORT)
		return 1;

	if(ld->special && numspecbump < MAXSPECIALBUMP)
	{
		specbump[numspecbump] = ld;
		numspecbump++;
	}

	return 0;
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t PIT_ChangeSector(mobj_t *thing)
{
	uint32_t onfloor;

	onfloor = (thing->z <= thing->floorz) && !(thing->flags2 & MF2_NOLIFTDROP);

	P_CheckPosition(thing, thing->x, thing->y);

	thing->floorz = tmfloorz;
	thing->ceilingz = tmceilingz;

	if(!onfloor)
	{
		if(thing->z + thing->height > thing->ceilingz)
			thing->z = thing->ceilingz - thing->height;
	} else
		thing->z = thing->floorz;

	if(thing->ceilingz - thing->floorz >= thing->height)
		return 1;

	if(!(thing->flags1 & MF1_DONTGIB))
	{
		if(thing->flags2 & MF2_ICECORPSE)
		{
			thing->tics = 1;
			thing->iflags |= MFI_SHATTERING;
			return 1;
		}

		if(thing->health <= 0)
		{
			thing->flags1 |= MF1_DONTGIB;

			if(!(thing->flags & MF_NOBLOOD))
			{
				uint32_t state;

				if(!thing->info->state_crush)
				{
					state = 895;
					thing->translation = thing->info->blood_trns;
				} else
					state = thing->info->state_crush;

				thing->flags &= ~MF_SOLID;

				P_SetMobjState(thing, state, 0);
			}

			return 1;
		}

		if(thing->flags & MF_DROPPED)
		{
			mobj_remove(thing);
			return 1;
		}
	}

	if(!(thing->flags & MF_SHOOTABLE))
		return 1;

	nofit = 1;

	if(crushchange && !(leveltime & 3))
	{
		uint32_t damage;

		if(crushchange & 0x8000) // TODO: 'crushchange' sould contain damage value directly
			damage = crushchange & 0x7FFF;
		else
			damage = 10;

		damage |= DAMAGE_SET_TYPE(DAMAGE_CRUSH);

		mobj_damage(thing, NULL, NULL, damage, NULL);

		if(!(thing->flags & MF_NOBLOOD))
		{
			mobj_t *mo;

			mo = P_SpawnMobj(thing->x, thing->y, thing->z + thing->height / 2, thing->info->blood_type);
			mo->inside = thing;
			mo->momx = (P_Random() - P_Random()) << 12;
			mo->momy = (P_Random() - P_Random()) << 12;
			if(!(mo->flags2 & MF2_DONTTRANSLATE))
				mo->translation = thing->info->blood_trns;
		}
	}

	return 1;
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t try_move_check(mobj_t *mo, fixed_t x)
{
	fixed_t y;
	uint32_t ret;

	{
		register fixed_t tmp asm("ebx"); // hack to extract 3rd argument
		y = tmp;
	}

	ret = P_CheckPosition(mo, x, y);

	while(numspecbump--)
		spec_activate(specbump[numspecbump], mo, SPEC_ACT_BUMP);
	numspecbump = 0;

	return ret;
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t check_position_extra(sector_t *sec)
{
	ceilingline = NULL;
	floorline = NULL;
	numspecbump = 0;

	tmthing->iflags &= ~MFI_MOBJONMOBJ;

	tmceilingz = sec->ceilingheight;
	tmfloorz = sec->floorheight;
	tmdropoffz = tmfloorz;

	tmflags = tmthing->flags;

	if(tmflags & MF_NOCLIP)
		return 1;

	e3d_check_heights(tmthing, sec, tmthing->flags & MF_MISSILE);

	if(tmextraceiling < tmceilingz)
		tmceilingz = tmextraceiling;

	if(tmextrafloor > tmfloorz)
	{
		tmfloorz = tmextrafloor;
		tmdropoffz = tmextrafloor;
	}

	if(e3d_check_inside(sec, tmfloorz, E3D_SOLID))
		tmceilingz = tmfloorz;

	return 0;
}

//
// API

uint8_t *mobj_check_keylock(mobj_t *mo, uint32_t lockdef, uint32_t is_remote)
{
	uint32_t have_key = 1;
	uint32_t did_check = 0;
	uint16_t *data = NULL;
	void *ptr = lockdefs;
	uint8_t *msg = NULL;
	uint8_t *rsg = NULL;

	while(ptr < lockdefs + lockdefs_size)
	{
		lockdef_t *ld = ptr;

		if(ld->id == lockdef)
			data = ld->data;

		ptr += ld->size;
	}

	if(!data)
		return "That doesn't seem to work";

	while(*data)
	{
		switch(*data & 0xF000)
		{
			case KEYLOCK_MESSAGE:
				msg = (uint8_t*)(data + 1);
			break;
			case KEYLOCK_REMTMSG:
				rsg = (uint8_t*)(data + 1);
			break;
			case KEYLOCK_KEYLIST:
				if(have_key)
				{
					uint32_t count = *data & 0x0FFF;
					uint32_t i;

					for(i = 0; i < count; i++)
					{
						did_check = 1;
						if(inventory_check(mo, data[1+i]))
							break;
					}

					if(i >= count)
						have_key = 0;
				}
			break;
		}
		data += 1 + (*data & 0x0FFF);
	}

	if(!did_check)
	{
		// any key
		if(mo->inventory)
		{
			for(uint32_t i = 0; i < mo->inventory->numslots; i++)
			{
				invitem_t *item = mo->inventory->slot + i;

				if(!item->type)
					continue;

				if(item->count && mobjinfo[item->type].extra_type == ETYPE_KEY)
				{
					have_key = 1;
					break;
				}
			}
		} else
			have_key = 0;
	}

	if(have_key)
		// NULL = unlock
		return NULL;

	if(is_remote)
	{
		if(rsg)
			msg = rsg;
	} else
	{
		if(!msg)
			msg = rsg;
	}

	if(msg)
		return msg;

	return "";
}

void mobj_use_item(mobj_t *mo, invitem_t *item)
{
	mobjinfo_t *info = mobjinfo + item->type;
	switch(info->extra_type)
	{
		case ETYPE_INVENTORY_CUSTOM:
			if(!use_custom_inv(mo, info))
				return;
		break;
		case ETYPE_ARMOR:
		case ETYPE_ARMOR_BONUS:
			if(!give_armor(mo, info))
				return;
		break;
		case ETYPE_POWERUP:
			if(!give_power(mo, info))
				return;
		break;
		case ETYPE_HEALTH_PICKUP:
			if(!mobj_give_health(mo, info->spawnhealth, mo->info->spawnhealth))
				return;
		break;
		default:
		return;
	}

	inventory_take(mo, item->type, 1);
}

uint32_t mobj_give_inventory(mobj_t *mo, uint16_t type, uint16_t count)
{
	uint32_t given;
	mobjinfo_t *info = mobjinfo + type;

	if(!count)
		return 0;

	switch(info->extra_type)
	{
		case ETYPE_HEALTH:
			return mobj_give_health(mo, (uint32_t)count * (uint32_t)info->inventory.count, info->inventory.max_count);
		case ETYPE_INV_SPECIAL:
			return give_special(mo, info);
		case ETYPE_INVENTORY:
		case ETYPE_WEAPON:
		case ETYPE_AMMO:
		case ETYPE_AMMO_LINK:
		case ETYPE_KEY: // can this fail in ZDoom?
			return inventory_give(mo, type, count) < count;
		case ETYPE_INVENTORY_CUSTOM:
			// pickup
			if(!pick_custom_inv(mo, info))
				return 0;
			// check for 'use'
			if(info->st_custinv.use)
			{
				given = 0;
				// autoactivate
				if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
				{
					given = use_custom_inv(mo, info);
					if(given)
						count--;
				}
				// give as item(s)
				if(!given || count)
					given |= inventory_give(mo, type, count) < count;
				// check
				if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
					return 0;
			}
			return 1;
		case ETYPE_ARMOR:
		case ETYPE_ARMOR_BONUS:
			given = 0;
			// autoactivate
			if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
			{
				given = give_armor(mo, info);
				if(given)
					count--;
			}
			// give as item(s)
			if(!given || count)
				given |= inventory_give(mo, type, count) < count;
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return 0;
			return 1;
		case ETYPE_POWERUP:
			given = 0;
			// autoactivate
			if(	info->eflags & MFE_INVENTORY_AUTOACTIVATE ||
				!info->inventory.max_count
			)
			{
				given = give_power(mo, info);
				if(given)
					count--;
			}
			// give as item(s)
			if(!given || count)
				given |= inventory_give(mo, type, count) < count;
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return 0;
			return 1;
		case ETYPE_HEALTH_PICKUP:
			given = 0;
			// autoactivate
			if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
			{
				given = mobj_give_health(mo, info->spawnhealth, mo->info->spawnhealth);
				if(given)
					count--;
			}
			// give as item(s)
			if(!given || count)
				given |= inventory_give(mo, type, count) < count;
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return 0;
			return 1;
	}

	return 0;
}

uint32_t mobj_for_each(uint32_t (*cb)(mobj_t*))
{
	for(thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		uint32_t ret;

		if(th->function != (void*)P_MobjThinker)
			continue;

		ret = cb((mobj_t*)th);
		if(ret)
			return ret;
	}

	return 0;
}

mobj_t *mobj_by_tid_first(uint32_t tid)
{
	for(thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		mobj_t *mo;

		if(th->function != (void*)P_MobjThinker)
			continue;

		mo = (mobj_t*)th;

		if(mo->special.tid == tid)
			return mo;
	}

	return NULL;
}

mobj_t *mobj_by_netid(uint32_t netid)
{
	if(!netid)
		return NULL;

	if(!thinkercap.next)
		return NULL;

	for(thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		mobj_t *mo;

		if(th->function != (void*)P_MobjThinker)
			continue;

		mo = (mobj_t*)th;
		if(mo->netid == netid)
			return mo;
	}

	return NULL;
}

__attribute((regparm(2),no_caller_saved_registers))
void mobj_remove(mobj_t *mo)
{
	for(uint32_t i = 0; i < numsectors; i++)
	{
		sector_t *sec = sectors + i;
		if(sec->soundtarget == mo)
			sec->soundtarget = NULL;
		if(sec->extra->action.enter == mo)
			sec->extra->action.enter = NULL;
		if(sec->extra->action.leave == mo)
			sec->extra->action.leave = NULL;
	}

	for(thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		mobj_t *om;

		if(th->function != (void*)P_MobjThinker)
			continue;

		om = (mobj_t*)th;

		if(om->target == mo)
			om->target = NULL;
		if(om->tracer == mo)
			om->tracer = NULL;
		if(om->master == mo)
			om->master = NULL;
		if(om->inside == mo)
			om->inside = NULL;
	}

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		player_t *pl;

		if(!playeringame[i])
			continue;

		pl = players + i;

		if(pl->mo == mo)
			pl->mo = NULL;

		if(pl->attacker == mo)
			pl->attacker = NULL;

		if(pl->camera == mo)
			pl->camera = pl->mo;
	}

	if(mo->flags & MF_COUNTKILL)
		totalkills--;
	if(mo->flags & MF_COUNTITEM)
		totalitems--;

	inventory_clear(mo);

	P_UnsetThingPosition(mo);
	S_StopSound(mo);
	P_RemoveThinker((thinker_t*)mo);
}

static __attribute((regparm(2),no_caller_saved_registers))
void mobj_plane_bounce(mobj_t *mo, fixed_t momz)
{
	if(mo->bounce_count && !--mo->bounce_count)
	{
		mobj_explode_missile(mo);
		return;
	}

	mo->angle = R_PointToAngle2(mo->x, mo->y, mo->x + mo->momx, mo->y + mo->momy);

	mo->momx = FixedMul(mo->momx, mo->info->bounce_factor);
	mo->momy = FixedMul(mo->momy, mo->info->bounce_factor);
	mo->momz = FixedMul(-momz, mo->info->bounce_factor);

	S_StartSound(mo, mo->info->bouncesound);
}

__attribute((regparm(2),no_caller_saved_registers))
void mobj_explode_missile(mobj_t *mo)
{
	uint32_t animation = ANIM_DEATH;

	mo->momx = 0;
	mo->momy = 0;
	if(mo->flags & MF_NOGRAVITY)
		mo->momz = 0;
	else
		mo->momz = -mo->gravity; // workaround

	mo->flags &= ~MF_SHOOTABLE;

	if(mobj_hit_thing)
	{
		if(mobj_hit_thing->flags & MF_NOBLOOD)
		{
			if(mo->info->state_crash)
				animation = ANIM_CRASH;
		} else
		{
			if(mo->info->state_xdeath)
				animation = ANIM_XDEATH;
		}

		if(mo->flags2 & MF2_HITTARGET)
			mo->target = mobj_hit_thing;
		if(mo->flags2 & MF2_HITMASTER)
			mo->master = mobj_hit_thing;
		if(mo->flags2 & MF2_HITTRACER)
			mo->tracer = mobj_hit_thing;
	}

	mobj_set_animation(mo, animation);

	if(mo->flags1 & MF1_RANDOMIZE && mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	mo->flags &= ~MF_MISSILE;

	S_StartSound(mo->flags2 & MF2_FULLVOLDEATH ? NULL : mo, mo->info->deathsound);
}

uint32_t mobj_range_check(mobj_t *mo, mobj_t *target, fixed_t range, uint32_t check_z)
{
	fixed_t dist;

	if(!target)
		return 0;

	if(check_z)
	{
		if(target->z >= mo->z + mo->height)
			return 0;

		if(target->z + target->height <= mo->z)
			return 0;
	}

	dist = P_AproxDistance(target->x - mo->x, target->y - mo->y);

	if(dist >= range)
		return 0;

	return 1;
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t mobj_check_melee_range(mobj_t *mo)
{
	mobj_t *target;
	fixed_t dist;

	if(!mo->target)
		return 0;

	target = mo->target;

	if(target->render_style >= RS_INVISIBLE)
		return 0;

	if(target->flags1 & MF1_NOTARGET)
		return 0;

	if(!mobj_range_check(mo, target, mo->info->range_melee + target->info->radius, !(mo->flags2 & MF2_NOVERTICALMELEERANGE)))
		return 0;

	if(!P_CheckSight(mo, mo->target))
		return 0;

	return 1;
}

void mobj_damage(mobj_t *target, mobj_t *inflictor, mobj_t *source, uint32_t damage, mobjinfo_t *pufftype)
{
	// target = what is damaged
	// inflictor = damage source (projectile or ...)
	// source = what is responsible
	player_t *player;
	int32_t kickback;
	uint32_t if_flags1;
	uint_fast8_t forced;
	uint_fast8_t damage_type;
	uint_fast8_t skip_armor;

	if(!(target->flags & MF_SHOOTABLE))
		return;

	if(target->flags1 & MF1_DORMANT)
		return;

	if(target->health <= 0 && !(target->flags2 & MF2_ICECORPSE))
			return;

	if(target->flags & MF_SKULLFLY)
	{
		target->momx = 0;
		target->momy = 0;
		target->momz = 0;
	}

	if(pufftype)
	{
		if_flags1 = pufftype->flags1;
		damage_type = pufftype->damage_type;
	} else
	if(inflictor)
	{
		if_flags1 = inflictor->flags1;
		damage_type = inflictor->damage_type;
	} else
	{
		if_flags1 = 0;
		damage_type = DAMAGE_NORMAL;
	}

	skip_armor = !!(damage & DAMAGE_SKIP_ARMOR);
	damage &= ~DAMAGE_SKIP_ARMOR;

	if(damage & (DAMAGE_TYPE_MASK << DAMAGE_TYPE_SHIFT))
	{
		damage_type = (damage >> DAMAGE_TYPE_SHIFT) & DAMAGE_TYPE_MASK;
		damage_type--;
		damage &= ~(DAMAGE_TYPE_MASK << DAMAGE_TYPE_SHIFT);
	}

	if(target->flags2 & MF2_ICECORPSE)
	{
		if(damage_type != DAMAGE_ICE || if_flags1 & MF1_ICESHATTER)
		{
			target->tics = 1;
			target->iflags |= MFI_SHATTERING;
		}
		return;
	}

	switch(damage & DAMAGE_TYPE_CHECK)
	{
		case DAMAGE_IS_MATH_FUNC:
		{
			int32_t temp = damage & 0xFFFF;
			temp = actarg_integer(inflictor, mobjinfo[temp].damage_func, 0, 0);
			if(temp < 0)
				damage = 0;
			else
				damage = temp;
		}
		break;
		case DAMAGE_IS_PROJECTILE:
			damage = ((P_Random() & 7) + 1) * (damage & 0x003FFFFF);
		break;
		case DAMAGE_IS_RIPPER:
			damage = ((P_Random() & 3) + 2) * (damage & 0x003FFFFF);
		break;
	}

	if(damage < 1000000)
	{
		if(target->flags1 & MF1_SPECTRAL && !(if_flags1 & MF1_SPECTRAL))
			return;

		// friendly fire
		if(	no_friendly_fire &&
			source &&
			source != target &&
			source->player &&
			target->player
		)
			return;

		if(target->info->damage_factor[damage_type] != 8)
			damage = (damage * target->info->damage_factor[damage_type]) / 8;
	}

	if(!damage && !(target->flags1 & MF1_NODAMAGE))
		return;

	if(damage >= 1000000)
		forced = damage - 999999;
	else
		forced = 0;
	if(damage > 1000000)
		damage = target->health;

	if(source && source->player && source->player->readyweapon)
		kickback = source->player->readyweapon->weapon.kickback;
	else
		kickback = 100;

	player = target->player;

	if(player && gameskill == sk_baby)
	{
		damage /= 2;
		if(!damage)
			damage = 1;
	}

	if(	target->flags1 & MF1_INVULNERABLE &&
		!forced
	)
		return;

	if(	inflictor &&
		kickback &&
		!(target->flags1 & MF1_DONTTHRUST) &&
		!(target->flags & MF_NOCLIP) &&
		!(if_flags1 & MF1_NODAMAGETHRUST)
	) {
		angle_t angle;
		int32_t thrust;

		angle = R_PointToAngle2(inflictor->x, inflictor->y, target->x, target->y);
		thrust = damage > 10000 ? 10000 : damage;

		thrust = (thrust * (FRACUNIT >> 5) * 100 / target->info->mass);
		if(thrust > (30 * FRACUNIT) >> 2)
			thrust = 30 * FRACUNIT;
		else
			thrust <<= 2;

		if(kickback != 100)
			thrust = (thrust * kickback) / 100;

		if(	!(target->flags1 & (MF1_NOFORWARDFALL | MF1_INVULNERABLE | MF1_BUDDHA | MF1_NODAMAGE)) &&
			!(if_flags1 & MF1_NOFORWARDFALL) &&
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

	if(target->flags2 & MF2_STEALTH)
	{
		target->render_alpha = 255;
		target->alpha_dir = -10;
	}

	if(player)
	{
		if(target->subsector->sector->special == 11 && damage >= target->health)
			damage = target->health - 1;

		if(player->armortype && !forced && !skip_armor)
		{
			uint32_t saved;
			saved = ((damage * mobjinfo[player->armortype].armor.percent) + 25) / 100;
			if(player->armorpoints <= saved)
			{
				saved = player->armorpoints;
				player->armortype = 0;
			}

			player->armorpoints -= saved;
			damage -= saved;
		}

		if(!(target->flags1 & MF1_NODAMAGE) || forced > 2)
		{
			player->health -= damage;
			if(player->health < 0)
				player->health = 0;

			player->damagecount += damage;
			if(player->damagecount > 65)
				player->damagecount = 65; // this is a bit less
		}

		player->attacker = source;

		if(source && source != target && player->cheats & CF_REVENGE)
			mobj_damage(source, NULL, target, 1000000, NULL);

		// I_Tactile ...
	}

	if(!(target->flags1 & MF1_NODAMAGE) || forced > 2)
	{
		target->health -= damage;
		if(target->health <= 0)
		{
			if(	target->flags1 & MF1_BUDDHA &&
				!forced
			)
			{
				target->health = 1;
				if(player)
					player->health = 1;
			} else
			{
				if(if_flags1 & MF1_NOEXTREMEDEATH)
				{
					if(target->health < -target->info->spawnhealth)
						target->health = -target->info->spawnhealth;
				} else
				if(if_flags1 & MF1_EXTREMEDEATH)
				{
					if(target->health >= -target->info->spawnhealth)
						target->health = -target->info->spawnhealth - 1;
				}

				target->damage_type = damage_type; // seems like ZDoom does this

				mobj_kill(target, source); // replaces P_KillMobj; swapped arguments!

				return;
			}
		}
	}

	if(	!(if_flags1 & MF1_PAINLESS) &&
		!(target->flags2 & MF2_NOPAIN) &&
		P_Random() < target->info->painchance[damage_type] &&
		!(target->flags & MF_SKULLFLY)
	) {
		uint32_t state = 0;

		target->flags |= MF_JUSTHIT;

		if(damage_type)
			state = dec_mobj_custom_state(target->info, damage_type_config[damage_type].pain);

		if(!state)
			state = target->info->state_pain;

		if(state)
		{
			target->animation = ANIM_PAIN;
			P_SetMobjState(target, state, 0);
		}
	}

	target->reactiontime = 0;

	if(	(
			(source && source->player) ||
			(!dehacked.no_infight && !(map_level_info->flags & MAP_FLAG_NO_INFIGHTING))
		) &&
		(!target->threshold || target->flags1 & MF1_QUICKTORETALIATE) &&
		source && source != target &&
		!(source->flags1 & MF1_NOTARGET)
	) {
		target->target = source;
		target->threshold = 100;
		if(target->info->state_see && target->state == states + target->info->state_spawn)
			mobj_set_animation(target, ANIM_SEE);
	}
}

static void mobj_fall_damage(mobj_t *mo)
{
	int32_t damage;
	int32_t mom;
	int32_t dist;

	if(	mo->momz <= -63 * FRACUNIT ||
		(
			!mo->player &&
			!(map_level_info->flags & MAP_FLAG_MONSTER_FALL_DMG)
		)
	){
		mobj_damage(mo, NULL, NULL, DAMAGE_WITH_TYPE(1000000, DAMAGE_FALLING), NULL);
		return;
	}

	dist = FixedMul(-mo->momz, 16 * FRACUNIT / 23);
	damage = ((FixedMul(dist, dist) / 10) >> FRACBITS) - 24;

	if(	mo->momz > -39 * FRACUNIT &&
		damage > mo->health &&
		mo->health != 1
	)
		damage = mo->health - 1;

	mobj_damage(mo, NULL, NULL, DAMAGE_WITH_TYPE(damage, DAMAGE_FALLING), NULL);
}

static __attribute((regparm(2),no_caller_saved_registers))
void P_XYMovement(mobj_t *mo)
{
	uint32_t dropoff;
	player_t *pl = mo->player;

	oldfloorz = mo->floorz;

	// allow pushing monsters off ledges (and +PUSHABLE)
	dropoff = (mo->flags ^ MF_DROPOFF) & MF_DROPOFF;
	mo->flags |= MF_DROPOFF;

	{
		// new, better movement code
		// split movement based on half of radius
		fixed_t ox, oy; // original location
		fixed_t sx, sy; // step move
		int32_t count; // step count

		ox = mo->radius >> (FRACBITS + 1);
		if(ox < 4)
			ox = 4;

		sx = abs(mo->momx);
		sy = abs(mo->momy);

		if(sx > sy)
			count = 1 + ((sx / ox) >> FRACBITS);
		else
			count = 1 + ((sy / ox) >> FRACBITS);

		sx = mo->momx / count;
		sy = mo->momy / count;

		ox = mo->x;
		oy = mo->y;

		do
		{
			fixed_t nx, ny; // new location
			fixed_t xx, yy; // previous location

			xx = mo->x;
			nx = xx + sx;
			yy = mo->y;
			ny = yy + sy;

			// move
			if(P_TryMove(mo, nx, ny))
			{
				// check for teleport
				if(mo->x != nx || mo->y != ny)
					break;
			} else
			{
				// check for teleport
				if(mo->x != xx || mo->y != yy)
					break;

				// blocked
				if(mo->flags & MF_SLIDE)
				{
					// throw away any existing result
					if(mo->x != ox || mo->y != oy)
					{
						P_UnsetThingPosition(mo);
						mo->x = ox;
						mo->y = oy;
						P_SetThingPosition(mo);
					}
					// and do a (single!) slide move
					P_SlideMove(mo);
					break;
				} else
				if(mo->flags & MF_MISSILE)
				{
					if(!(mo->flags1 & MF1_SKYEXPLODE) && ceilingline && ceilingline->backsector)
					{
						if(	(ceilingline->backsector->ceilingpic == skyflatnum && mo->z + mo->height >= ceilingline->backsector->ceilingheight) ||
							(ceilingline->frontsector->ceilingpic == skyflatnum && mo->z + mo->height >= ceilingline->frontsector->ceilingheight)
						)
							mobj_remove(mo);
					}
					// TODO: floorline check

					if(mo->thinker.function != (void*)-1)
						mobj_explode_missile(mo);
				} else
				{
					mo->momx = 0;
					mo->momy = 0;
				}
				break;
			}
		} while(--count);
	}

	// restore +DROPOFF
	mo->flags ^= dropoff;

	// check water
	if(!mo->momz)
		e3d_check_water(mo);

	// HACK - move other sound slots
	mo->sound_body.x = mo->x;
	mo->sound_body.y = mo->y;
	mo->sound_weapon.x = mo->x;
	mo->sound_weapon.y = mo->y;

	if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
		// no friction for projectiles
		return;

	if(mo->z > mo->floorz && mo->waterlevel <= 1 && (!pl || !(mo->flags & MF_NOGRAVITY)))
		// no friction in air
		return;

	if(mo->flags & MF_CORPSE)
	{
		// sliding corpses, yay
		if(	mo->momx > FRACUNIT/4 ||
			mo->momx < -FRACUNIT/4 ||
			mo->momy > FRACUNIT/4 ||
			mo->momy < -FRACUNIT/4
		) {
			if(mo->floorz != mo->subsector->sector->floorheight && !mo->subsector->sector->exfloor)
				return;
		}
	}

	if(	mo->momx > -STOPSPEED &&
		mo->momx < STOPSPEED &&
		mo->momy > -STOPSPEED &&
		mo->momy < STOPSPEED &&
		(!pl || (pl->cmd.forwardmove == 0 && pl->cmd.sidemove == 0))
	) {
		if(pl && mo->animation == ANIM_SEE)
			mobj_set_animation(mo, ANIM_SPAWN);
		mo->momx = 0;
		mo->momy = 0;
	} else
	{
		// friction
		fixed_t friction = mo->waterlevel <= 1 ? FRICTION : FRICTION_WATER;
		mo->momx = FixedMul(mo->momx, friction);
		mo->momy = FixedMul(mo->momy, friction);
	}
}

__attribute((regparm(2),no_caller_saved_registers))
static void P_ZMovement(mobj_t *mo)
{
	fixed_t oldz = mo->z;
	fixed_t momz = mo->momz;
	uint32_t splash = 0;

	// check for smooth step up
	if(mo->player && mo->player->mo == mo && mo->z < mo->floorz)
	{
		mo->player->viewheight -= mo->floorz - mo->z;
		mo->player->deltaviewheight = (mo->info->player.view_height - mo->player->viewheight) >> 3;
	}

	// adjust height
	mo->z += mo->momz;

	// water level
	if(momz)
		e3d_check_water(mo);

	// Z movement
	if(mo->flags & MF_NOGRAVITY)
	{
		if(mo->player)
		{
			// flight friction
			if(mo->momz > -STOPSPEED && mo->momz < STOPSPEED)
				mo->momz = 0;
			else
				mo->momz = FixedMul(mo->momz, FRICTION);
		}
	} else
	if(mo->waterlevel > 1)
	{
		// water friction
		fixed_t speed = SINK_SPEED;
		fixed_t diff;

		if(!mo->player)
		{
			fixed_t mass = mo->info->mass;
			if(mass > 4000)
				mass = 4000;
			else
			if(mass < 1)
				mass = 1;

			speed *= mass;
			speed /= 100;
		}

		diff = mo->momz + speed;

		if(diff > -STOPSPEED && diff < STOPSPEED)
			mo->momz = -speed;
		else
			mo->momz += FixedMul(diff, -8192);
	}

	// float down towards target if too close
	if(mo->flags & MF_FLOAT && mo->target)
	{
		fixed_t dist;
		fixed_t delta;

		if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
		{
			dist = P_AproxDistance(mo->x - mo->target->x, mo->y - mo->target->y);
			delta = (mo->target->z + (mo->height >> 1)) - mo->z;
			if(delta < 0 && dist < delta * -3)
				mo->z -= FLOATSPEED;
			else
			if(delta > 0 && dist < delta * 3)
				mo->z += FLOATSPEED;
		}
	}

	// splash
	if(flatterrain && !(mo->flags2 & MF2_DONTSPLASH))
	{
		extraplane_t *pl;

		if(mo->momz < 0)
		{
			uint32_t remove = 0;

			pl = mo->subsector->sector->exfloor;
			while(pl)
			{
				if(	!(pl->flags & E3D_SWAP_PLANES) &&
					*pl->height > mo->floorz &&
					mo->z <= *pl->height && oldz > *pl->height &&
					pl->source->ceilingpic < numflats + num_texture_flats &&
					flatterrain[pl->source->ceilingpic] != 255 &&
					terrain[flatterrain[pl->source->ceilingpic]].splash != 255
				){
					int32_t flat = pl->source->ceilingpic;
					if(mo->info->mass < TERRAIN_LOW_MASS)
						flat = -flat;
					if(terrain_hit_splash(mo, mo->x, mo->y, *pl->height, flat))
					{
						splash |= 1;
						remove = !!(pl->flags & E3D_SOLID);
					}
				}
				pl = pl->next;
			}

			if(	!(mo->iflags & MFI_MOBJONMOBJ) &&
				mo->z <= mo->floorz &&
				mo->floorz == mo->subsector->sector->floorheight
			){
				if(terrain_hit_splash(mo, mo->x, mo->y, mo->floorz, mo->subsector->sector->floorpic))
				{
					splash |= 1;
					remove = 1;
				}
			}

			if(remove && mo->flags2 & MF2_BOUNCEONFLOORS && mo->flags & MF_MISSILE)
			{
				if(mo->flags2 & MF2_EXPLODEONWATER)
				{
					mobj_explode_missile(mo);
					return;
				} else
				if(!(mo->flags2 & MF2_CANBOUNCEWATER))
				{
					mobj_remove(mo);
					return;
				}
			}
		} else
		if(mo->momz > 0 && mo->flags & MF_MISSILE)
		{
			// this is extra - ZDoom does splash only when submerging
			fixed_t t0 = oldz + mo->height;
			fixed_t t1 = mo->z + mo->height;

			pl = mo->subsector->sector->exfloor;
			while(pl)
			{
				if(	!(pl->flags & E3D_SWAP_PLANES) &&
					t0 < *pl->height && t1 >= *pl->height &&
					pl->source->ceilingpic < numflats + num_texture_flats &&
					flatterrain[pl->source->ceilingpic] != 255 &&
					terrain[flatterrain[pl->source->ceilingpic]].splash != 255
				){
					int32_t flat = pl->source->ceilingpic;
					if(mo->info->mass < TERRAIN_LOW_MASS)
						flat = -flat;
					terrain_hit_splash(mo, mo->x, mo->y, *pl->height, flat);
				}
				pl = pl->next;
			}
		}
	}

	// clip movement
	if(mo->z <= mo->floorz)
	{
		// hit the floor
		if(mo->momz < 0)
		{
			if(mo->momz < mo->gravity * -8)
			{
				if(mo->player && mo->health > 0)
				{
					mo->player->deltaviewheight = mo->momz >> 3;
					if(!splash)
						S_StartSound(SOUND_CHAN_BODY(mo), mo->info->player.sound.land);
				}

				if(mo->flags2 & MF2_ICECORPSE)
				{
					mo->tics = 1;
					mo->iflags |= MFI_SHATTERING;
				}
			}

			if(mo->momz < -23 * FRACUNIT)
			{
				if(mo->player)
				{
					if(map_level_info->flags & MAP_FLAG_FALLING_DAMAGE)
					{
						mobj_fall_damage(mo);
						P_NoiseAlert(mo, mo);
					}
				} else
				{
					if(map_level_info->flags & (MAP_FLAG_MONSTER_FALL_DMG_KILL | MAP_FLAG_MONSTER_FALL_DMG))
						mobj_fall_damage(mo);
				}
			}

			mo->momz = 0;
		}

		mo->z = mo->floorz;

		if(mo->flags & MF_MISSILE && !(mo->flags & MF_NOCLIP))
		{
			if(	mo->flags1 & MF1_SKYEXPLODE ||
				!mo->subsector ||
				!mo->subsector->sector ||
				mo->subsector->sector->floorheight < mo->z ||
				mo->subsector->sector->floorpic != skyflatnum
			){
				mobj_hit_thing = NULL;
				if(mo->flags2 & MF2_BOUNCEONFLOORS)
					mobj_plane_bounce(mo, momz);
				else
					mobj_explode_missile(mo);
			} else
				mobj_remove(mo);
			return;
		}

		if(mo->flags & MF_CORPSE && !(mo->iflags & MFI_CRASHED))
		{
			uint32_t state = 0;
			uint32_t animation;

			mo->iflags |= MFI_CRASHED;

			// look for custom damage first
			if(mo->damage_type)
			{
				if(mo->health < -mo->info->spawnhealth)
					state = dec_mobj_custom_state(mo->info, damage_type_config[mo->damage_type].xcrash);
				if(!state)
					state = dec_mobj_custom_state(mo->info, damage_type_config[mo->damage_type].crash);
			}

			// look for normal damage now
			if(!state)
			{
				if(mo->health < -mo->info->spawnhealth)
					state = mo->info->state_xcrash;
				if(!state)
					state = mo->info->state_crash;
			}

			if(state)
			{
				mo->animation = ANIM_CRASH; // or xcrash? meh
				P_SetMobjState(mo, state, 0);
			}
		}
	} else
	if(!(mo->flags & MF_NOGRAVITY) && mo->waterlevel <= 1)
	{
		if(mo->momz == 0 && (oldfloorz > mo->floorz && mo->z == oldfloorz))
			mo->momz = mo->gravity * -2;
		else
			mo->momz -= mo->gravity;
	}

	if(mo->z + mo->height > mo->ceilingz)
	{
		// hit the ceiling
		if(mo->momz > 0)
			mo->momz = 0;

		mo->z = mo->ceilingz - mo->height;

		if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
		{
			if(	mo->flags1 & MF1_SKYEXPLODE ||
				!mo->subsector ||
				!mo->subsector->sector ||
				mo->subsector->sector->ceilingheight > mo->z + mo->height ||
				mo->subsector->sector->ceilingpic != skyflatnum
			){
				mobj_hit_thing = NULL;
				if(mo->flags2 & MF2_BOUNCEONCEILINGS)
					mobj_plane_bounce(mo, momz);
				else
					mobj_explode_missile(mo);
			} else
				mobj_remove(mo);
			return;
		}
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t PIT_StompThing(mobj_t *thing)
{
	fixed_t blockdist;

	if(thing == tmthing)
		return 1;

	if(!(thing->flags & MF_SHOOTABLE))
		return 1;

	if(thing->z >= tmthing->z + tmthing->height)
		return 1;

	if(tmthing->z >= thing->z + thing->height)
		return 1;

	blockdist = thing->radius + tmthing->radius;

	if(abs(thing->x - tmthing->x) >= blockdist)
		return 1;

	if(abs(thing->y - tmthing->y) >= blockdist)
		return 1;

	mobj_damage(thing, tmthing, tmthing, DAMAGE_WITH_TYPE(1000000, DAMAGE_TELEFRAG), NULL);

	return 1;
}

void mobj_telestomp(mobj_t *mo, fixed_t x, fixed_t y)
{
	int32_t xl, xh, yl, yh;

	tmbbox[BOXTOP] = y + tmthing->radius;
	tmbbox[BOXBOTTOM] = y - tmthing->radius;
	tmbbox[BOXRIGHT] = x + tmthing->radius;
	tmbbox[BOXLEFT] = x - tmthing->radius;

	xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

	tmthing = mo;

	for(int32_t bx = xl; bx <= xh; bx++)
		for(int32_t by = yl; by <= yh; by++)
			P_BlockThingsIterator(bx, by, PIT_StompThing);
}

uint32_t mobj_teleport(mobj_t *mo, fixed_t x, fixed_t y, fixed_t z, angle_t angle, uint32_t flags)
{
	fixed_t xx, yy, zz, fz, cz, ff;
	uint32_t blocked;

	xx = mo->x;
	yy = mo->y;
	zz = mo->z;
	cz = mo->ceilingz;
	fz = mo->floorz;

	if(mo->flags & MF_MISSILE)
		ff = mo->subsector->sector->floorheight;

	P_UnsetThingPosition(mo);

	mo->x = x;
	mo->y = y;
	if(!(flags & TELEF_USE_Z))
	{
		subsector_t *ss;
		ss = R_PointInSubsector(x, y);
		mo->z = ss->sector->floorheight;
		if(mo->flags & MF_MISSILE)
			mo->z += zz - ff;
	} else
		mo->z = z;

	P_SetThingPosition(mo);

	mo->flags |= MF_TELEPORT;
	teleblock = 0;
	blocked = !P_TryMove(mo, mo->x, mo->y);
	mo->flags &= ~MF_TELEPORT;
	if(blocked || teleblock)
	{
		if(flags & TELEF_NO_KILL)
			goto revert;

		if(!(mo->flags1 & MF1_TELESTOMP))
			goto revert;

		mobj_telestomp(mo, x, y);
	}

	if(flags & TELEF_USE_ANGLE)
		mo->angle = angle;

	angle = mo->angle >> ANGLETOFINESHIFT;

	if(mo->flags & MF_MISSILE)
	{
		fixed_t speed, pitch;

		if(mo->info->fast_speed && (fastparm || gameskill == sk_nightmare))
			speed = mo->info->fast_speed;
		else
			speed = mo->info->speed;

		if(mo->momz)
		{
			pitch = slope_to_angle(mo->momz);
			pitch >>= ANGLETOFINESHIFT;
			speed = FixedMul(speed, finecosine[pitch]);
		}

		mo->momx = FixedMul(speed, finecosine[angle]);
		mo->momy = FixedMul(speed, finesine[angle]);
	}

	if(!(flags & TELEF_NOSTOP) && !(mo->flags & MF_MISSILE))
	{
		mo->momx = 0;
		mo->momy = 0;
		mo->momz = 0;
		if(mo->player)
			mo->reactiontime = 18;
	}

	if(flags & TELEF_FOG)
	{
		if(mo->info->telefog[0])
			P_SpawnMobj(xx, yy, zz, mo->info->telefog[0]);

		if(mo->info->telefog[1])
		{
			x = mo->x + 20 * finecosine[angle];
			y = mo->y + 20 * finesine[angle];
			z = mo->z;
			P_SpawnMobj(x, y, z, mo->info->telefog[1]);
		}
	}

	if(mo->player)
	{
		fixed_t viewheight = mo->info->player.view_height;
		mo->player->viewz = mo->z + viewheight;
		mo->player->viewheight = viewheight;
		mo->player->deltaviewheight = 0;
	}

	e3d_check_water(mo);

	// HACK - move other sound slots
	mo->sound_body.x = mo->x;
	mo->sound_body.y = mo->y;
	mo->sound_weapon.x = mo->x;
	mo->sound_weapon.y = mo->y;

	return 1;

revert:
	P_UnsetThingPosition(mo);
	mo->x = xx;
	mo->y = yy;
	mo->z = zz;
	mo->ceilingz = cz;
	mo->floorz = fz;
	P_SetThingPosition(mo);

	return 0;
}

void mobj_spawn_puff(divline_t *trace, mobj_t *target, uint32_t puff_type)
{
	// hitscan only!
	mobj_t *mo;
	uint32_t state = 0;

	if(	target &&
		!(target->flags & MF_NOBLOOD) &&
		!(target->flags1 & MF1_DORMANT) &&
		!(mobjinfo[mo_puff_type].flags1 & MF1_PUFFONACTORS)
	)
		return;

	mo = P_SpawnMobj(trace->x, trace->y, trace->dx, puff_type);

	if(mo->flags2 & MF2_HITTARGET)
		mo->target = target;
	if(mo->flags2 & MF2_HITMASTER)
		mo->master = target;
	if(mo->flags2 & MF2_HITTRACER)
		mo->tracer = target;

	if(mo->flags2 & MF2_PUFFGETSOWNER)
		mo->target = shootthing;

	if(!(mo_puff_flags & 1))
	{
		mo->z += ((P_Random() - P_Random()) << 10);
		if(mo->z > mo->ceilingz - mo->height)
			mo->z = mo->ceilingz - mo->height;
		if(mo->z < mo->floorz)
			mo->z = mo->floorz;
	}

	if(target)
	{
		if(!(target->flags & MF_NOBLOOD) && mo->info->state_xdeath)
			state = mo->info->state_xdeath;
	} else
	{
		if(mo->info->state_crash)
			state = mo->info->state_crash;
		else
		if(attackrange <= 64 * FRACUNIT && mo->info->state_melee)
			state = mo->info->state_melee;
	}

	if(state)
		P_SetMobjState(mo, state, 0);

	if(mo->flags1 & MF1_RANDOMIZE && mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	mo->angle = shootthing->angle + ANG180;
}

void mobj_spawn_blood(divline_t *trace, mobj_t *target, uint32_t damage, uint32_t puff_type)
{
	// hitscan only!
	mobj_t *mo;
	uint32_t state;

	if(target->flags & MF_NOBLOOD)
		return;

	if(target->flags1 & MF1_DORMANT)
		return;

	if(puff_type && mobjinfo[puff_type].flags2 & MF2_BLOODLESSIMPACT)
		return;

	mo = P_SpawnMobj(trace->x, trace->y, trace->dx, target->info->blood_type);
	mo->inside = target;
	mo->angle = shootthing->angle;
	mo->momz = FRACUNIT * 2;

	if(!(mo->flags2 & MF2_DONTTRANSLATE))
		mo->translation = target->info->blood_trns;

	if(mo->flags2 & MF2_PUFFGETSOWNER)
		mo->target = target;

	if(damage < 9)
		state = 2;
	else
	if(damage <= 12)
		state = 1;
	else
		return;

	state += mo->info->state_spawn;

	if(state >= num_states) // TODO: this is not correct
		state = num_states - 1;

	P_SetMobjState(mo, state, 0);
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t mobj_change_sector(sector_t *sec, uint32_t crush)
{
	// sector based
	for(mobj_t *mo = sec->thinglist; mo; mo = mo->snext)
	{
		uint32_t of, oc;

		if(!(mo->flags2 & MF2_MOVEWITHSECTOR))
			continue;

		if(!(mo->flags & MF_NOBLOCKMAP))
			continue;

		of = mo->z <= mo->floorz;
		oc = mo->z + mo->height >= mo->ceilingz;

		tmfloorz = mo->subsector->sector->floorheight;
		tmceilingz = mo->subsector->sector->ceilingheight;

		if(mo->subsector && mo->subsector->sector->exfloor)
		{
			e3d_check_heights(mo, mo->subsector->sector, 1);
			tmfloorz = tmextrafloor;
			tmceilingz = tmextraceiling;
		}

		mo->floorz = tmfloorz;
		mo->ceilingz = tmceilingz;

		if(of)
			mo->z = tmfloorz;
		else
		if(oc)
			mo->z = tmceilingz - mo->height;

		e3d_check_water(mo);
	}

	// blockmap
	nofit = 0;
	crushchange = crush;

	for(int32_t x = sec->blockbox[BOXLEFT]; x <= sec->blockbox[BOXRIGHT]; x++)
		for(int32_t y = sec->blockbox[BOXBOTTOM]; y <= sec->blockbox[BOXTOP]; y++)
			P_BlockThingsIterator(x, y, PIT_ChangeSector);

	return nofit;
}

static __attribute((regparm(2),no_caller_saved_registers))
mobj_t *spawn_missile(mobj_t *source, mobj_t *target)
{
	mobj_t *th;
	uint32_t type;
	angle_t angle;
	fixed_t z, dist, speed;

	{
		register uint32_t tmp asm("ebx"); // hack to extract 3rd argument
		type = tmp;
	}

	z = source->z + 32 * FRACUNIT;

	angle = R_PointToAngle2(source->x, source->y, target->x, target->y);
	dist = P_AproxDistance(target->x - source->x, target->y - source->y);

	th = P_SpawnMobj(source->x, source->y, z, type);

	speed = projectile_speed(th->info);

	if(speed)
	{
		dist /= speed;
		if(dist <= 0)
			dist = 1;
		dist = ((target->z + 32 * FRACUNIT) - z) / dist;
		th->momz = FixedDiv(dist, speed);
	}

	missile_stuff(th, source, target, speed, angle, 0, th->momz);

	return th;
}

//
// sector action

static void mobj_sector_action(mobj_t *mo, mobj_t **actptr)
{
	mobj_t *action = *actptr;

	if(mo->player)
	{
		if(action->iflags & MFI_FRIENDLY)
			return;
	} else
	if(mo->flags1 & MF1_ISMONSTER)
	{
		if(!(action->flags & MF_AMBUSH))
			return;
	} else
	if(mo->flags & MF_MISSILE)
	{
		if(!(action->flags1 & MF1_DORMANT))
			return;
	} else
		return;

	spec_arg[0] = action->special.arg[0];
	spec_arg[1] = action->special.arg[1];
	spec_arg[2] = action->special.arg[2];
	spec_arg[3] = action->special.arg[3];
	spec_arg[4] = action->special.arg[4];
	spec_special = action->special.special;

	spec_activate(NULL, mo, 0);

	if(!spec_success)
		return;

	if(!(action->iflags & MFI_STANDSTILL))
		return;

	*actptr = NULL;
}

//
// new thinker

__attribute((regparm(2),no_caller_saved_registers))
void P_MobjThinker(mobj_t *mo)
{
	angle_t angle;

	if(mo->iflags & MFI_CRASHED && !(mo->flags & MF_CORPSE))
		// archvile ressurection or something
		mo->iflags &= ~MFI_CRASHED;

	if(!mo->momz && !(mo->flags & MF_NOGRAVITY) && mo->z <= mo->floorz && mo->flags & MF_CORPSE && !(mo->iflags & MFI_CRASHED))
		// fake some Z movement for crash states
		mo->momz = -1;

	// path follower
	if(mo->iflags & MFI_FOLLOW_PATH)
	{
		mover_tick(mo);
		goto skip_move;
	}

	if(mo->iflags & MFI_FOLLOW_MOVE)
	{
		angle = mo->angle; // workaround
		goto skip_move;
	}

	// XY movement
	if(	mo->momx ||
		mo->momy ||
		mo->flags & MF_MISSILE ||
		mo->iflags & MFI_MOBJONMOBJ
	){
		P_XYMovement(mo);
		if(mo->thinker.function == (void*)-1)
			return;
	} else
	if(mo->flags & MF_SKULLFLY)
	{
		mo->flags &= ~MF_SKULLFLY;
		mo->momz = 0;
		mobj_set_animation(mo, ANIM_SPAWN);
	}

	// Z movement
	if((mo->z != mo->floorz) || mo->momz)
	{
		P_ZMovement(mo);
		if(mo->thinker.function == (void*)-1)
			return;
	}

skip_move:

	// sector actions
	if(mo->old_sector != mo->subsector->sector)
	{
		sector_t *old = mo->old_sector;
		sector_t *now = mo->subsector->sector;

		mo->old_sector = mo->subsector->sector;

		if(old->extra->action.leave)
			mobj_sector_action(mo, &old->extra->action.leave);

		if(now->extra->action.enter)
			mobj_sector_action(mo, &now->extra->action.enter);
	}

	// stealth
	if(mo->flags2 & MF2_STEALTH && mo->alpha_dir)
	{
		int32_t alpha;
		alpha = mo->render_alpha;
		alpha += mo->alpha_dir;
		if(alpha >= 255)
		{
			alpha = 255;
			mo->alpha_dir = 0;
		} else
		if(alpha <= mo->info->stealth_alpha)
		{
			alpha = mo->info->stealth_alpha;
			mo->alpha_dir = 0;
		}
		mo->render_alpha = alpha;
	}

	// first state
	if(mo->frame & FF_NODELAY)
	{
		mo->frame &= ~FF_NODELAY;
		if(mo->state->acp)
			mo->state->acp(mo, mo->state, P_SetMobjState);
	}

	// animate
	if(mo->tics >= 0)
	{
		mo->tics--;
		if(mo->tics <= 0)
			if(!P_SetMobjState(mo, mo->state->nextstate, mo->state->next_extra))
				return;
	} else
		check_nightmare(mo);

	if(mo->iflags & MFI_FOLLOW_MOVE)
		mo->angle = angle; // workaround
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
	// replace 'P_CheckMeleeRange'
	{0x00027040, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)mobj_check_melee_range},
	// replace 'P_SetMobjState'
	{0x00030EA0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)set_mobj_state},
	// replace 'P_RemoveMobj'
	{0x00031660, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)mobj_remove},
	// replace 'P_DamageMobj' - use trampoline
	{0x0002A460, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)hook_mobj_damage},
	// hollow out 'P_KillMobj' - keep only item drops
	{0x0002A2B9, CODE_HOOK | HOOK_JMP_DOOM, 0x0002A40D},
	// replace 'P_ChangeSector'
	{0x0002BF90, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)mobj_change_sector},
	// replace 'P_SpawnMissile'
	{0x00031C60, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)spawn_missile},
	// change 'mobj_t' size
	{0x00031552, CODE_HOOK | HOOK_UINT32, sizeof(mobj_t)},
	// fix 'P_SpawnMobj'; disable old 'frame'
	{0x000315F9, CODE_HOOK | HOOK_SET_NOPS, 3},
	// replace most of 'PIT_CheckThing'
	{0x0002AEDA, CODE_HOOK | HOOK_UINT16, 0xD889},
	{0x0002AEDC, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)pit_check_thing},
	{0x0002AEE1, CODE_HOOK | HOOK_UINT16, 0x56EB},
	// replace most of 'PIT_CheckLine'
	{0x0002ADC2, CODE_HOOK | HOOK_UINT16, 0x0AEB},
	{0x0002ADD3, CODE_HOOK | HOOK_UINT16, 0xDA89},
	{0x0002ADD5, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)pit_check_line},
	{0x0002ADDA, CODE_HOOK | HOOK_UINT16, 0x05EB},
	// replace call to 'P_CheckPosition' in 'P_TryMove'
	{0x0002B217, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)try_move_check},
	// add extra floor check into 'P_CheckPosition'
	{0x0002B0D7, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)check_position_extra},
	{0x0002B0DC, CODE_HOOK | HOOK_UINT32, 0x16EBC085},
	// replace pointers to 'P_MobjThinker'
	{0x0002767F, CODE_HOOK | HOOK_UINT32, (uint32_t)P_MobjThinker},
	{0x000286FA, CODE_HOOK | HOOK_UINT32, (uint32_t)P_MobjThinker},
	{0x00028979, CODE_HOOK | HOOK_UINT32, (uint32_t)P_MobjThinker},
	{0x00028ADB, CODE_HOOK | HOOK_UINT32, (uint32_t)P_MobjThinker},
	{0x00031643, CODE_HOOK | HOOK_UINT32, (uint32_t)P_MobjThinker},
	{0x00031EA6, CODE_HOOK | HOOK_UINT32, (uint32_t)P_MobjThinker},
	{0x0002767F, CODE_HOOK | HOOK_UINT32, (uint32_t)P_MobjThinker},
	// replace 'P_SetMobjState' with new animation system
	{0x00027776, CODE_HOOK | HOOK_UINT32, 0x909000B2 | (ANIM_SEE << 8)}, // A_Look
	{0x00027779, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Look
	{0x0002782A, CODE_HOOK | HOOK_UINT32, 0x909000B2 | (ANIM_SPAWN << 8)}, // A_Chase
	{0x0002782D, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	{0x0002789D, CODE_HOOK | HOOK_UINT32, 0x909000B2 | (ANIM_MELEE << 8)}, // A_Chase
	{0x000278A0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	{0x000278DD, CODE_HOOK | HOOK_UINT32, 0x909000B2 | (ANIM_MISSILE << 8)}, // A_Chase
	{0x000278E0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	{0x000281DF, CODE_HOOK | HOOK_UINT32, ANIM_HEAL}, // A_VileChase
	{0x000281E3, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_VileChase
	{0x000281FF, CODE_HOOK | HOOK_UINT32, 0x909000B2 | (ANIM_RAISE << 8)}, // A_VileChase
	{0x00028202, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_VileChase
};

