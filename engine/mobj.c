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
#include "action.h"
#include "player.h"
#include "weapon.h"
#include "hitscan.h"
#include "special.h"
#include "map.h"
#include "stbar.h"
#include "sound.h"
#include "render.h"
#include "demo.h"
#include "cheat.h"
#include "extra3d.h"

#define STOPSPEED	0x1000
#define FRICTION	0xe800
#define FLOATSPEED	(FRACUNIT * 4)

static line_t *ceilingline;
static line_t *floorline;

static line_t *specbump[MAXSPECIALBUMP];
static uint32_t numspecbump;

uint32_t mo_puff_type = 37;
uint32_t mo_puff_flags;

uint_fast8_t mo_dmg_skip_armor;

uint_fast8_t reborn_inventory_hack;

uint32_t mobj_netid;

static fixed_t oldfloorz;
static mobj_t *hit_thing;
static uint_fast8_t teleblock;

//

// this only exists because original animations are all over the place in 'mobjinfo_t'
const uint16_t base_anim_offs[LAST_MOBJ_STATE_HACK] =
{
	[ANIM_SPAWN] = offsetof(mobjinfo_t, state_spawn),
	[ANIM_SEE] = offsetof(mobjinfo_t, state_see),
	[ANIM_PAIN] = offsetof(mobjinfo_t, state_pain),
	[ANIM_MELEE] = offsetof(mobjinfo_t, state_melee),
	[ANIM_MISSILE] = offsetof(mobjinfo_t, state_missile),
	[ANIM_DEATH] = offsetof(mobjinfo_t, state_death),
	[ANIM_XDEATH] = offsetof(mobjinfo_t, state_xdeath),
	[ANIM_RAISE] = offsetof(mobjinfo_t, state_raise),
};

static const hook_t hook_fast_missile[];
static const hook_t hook_slow_missile[];

//
// state changes

__attribute((regparm(2),no_caller_saved_registers))
uint32_t mobj_set_state(mobj_t *mo, uint32_t state)
{
	// normal state changes
	state_t *st;

	do
	{
		if(state & 0x80000000)
		{
			// change animation
			uint16_t offset;

			offset = state & 0xFFFF;
			if(!(state & 0x40000000))
			{
				mo->animation = (state >> 16) & 0xFF;

				if(mo->animation < LAST_MOBJ_STATE_HACK)
					state = *((uint16_t*)((void*)mo->info + base_anim_offs[mo->animation]));
				else
				if(mo->animation < NUM_MOBJ_ANIMS)
					state = mo->info->new_states[mo->animation - LAST_MOBJ_STATE_HACK];
				else
					state = mo->info->extra_states[mo->animation - NUM_MOBJ_ANIMS];
			} else
				state = mo->state - states;

			if(state)
				state += offset;

			if(state >= mo->info->state_idx_limit)
				engine_error("MOBJ", "State jump '+%u' is invalid!", offset);
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
			mo->frame = st->frame;
		}

		if(st->tics > 1 && (fastparm || gameskill == sk_nightmare) && st->frame & FF_FAST)
			mo->tics = st->tics / 2;
		else
			mo->tics = st->tics;

		state = st->nextstate;

		if(st->acp)
			st->acp(mo, st, mobj_set_state);

	} while(!mo->tics);

	return 1;
}

__attribute((regparm(2),no_caller_saved_registers))
static uint32_t mobj_inv_change(mobj_t *mo, uint32_t state)
{
	// defer state change for later
	mo->custom_state = state;
}

static uint32_t mobj_inv_loop(mobj_t *mo, uint32_t state)
{
	// state set by custom inventory
	state_t *st;
	uint32_t oldstate = 0;

	mo->custom_state = 0;

	while(1)
	{
		if(state & 0x80000000)
		{
			// change animation
			mobjinfo_t *info = mo->custom_inventory;
			uint16_t offset;
			uint8_t anim;

			offset = state & 0xFFFF;
			if(!(state & 0x40000000))
			{
				anim = (state >> 16) & 0xFF;

				if(anim < NUM_MOBJ_ANIMS)
					state = *((uint16_t*)((void*)info + base_anim_offs[anim]));
				else
					state = info->extra_states[anim - NUM_MOBJ_ANIMS];
			} else
				state = oldstate;

			if(state)
				state += offset;

			if(state >= info->state_idx_limit)
				engine_error("MOBJ", "State jump '+%u' is invalid!", offset);
		}

		if(state <= 1)
		{
			mo->custom_inventory = NULL;
			return !state;
		}

		oldstate = state;
		st = states + state;
		state = st->nextstate;

		if(st->acp)
		{
			st->acp(mo, st, mobj_inv_change);
			if(mo->custom_state)
			{
				// apply deferred state change
				state = mo->custom_state;
				mo->custom_state = 0;
			}
		}
	}
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
}

static uint32_t give_power(mobj_t *mo, mobjinfo_t *info)
{
	uint32_t duration;

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

	if(!mo->player->powers[info->powerup.type])
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
			given = inventory_give(toucher, mo->type, info->inventory.count);
			if(!given)
			{
				pl->stbar_update |= STU_WEAPON_NEW; // evil grin
				// auto-weapon-switch optional
				if(pl->info_flags & PLF_AUTO_SWITCH)
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
	}

	// remove // TODO: handle respawn logic (like heretic does)
	mobj_remove(mo);

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
	if(pl == players + consoleplayer)
		S_StartSound(NULL, info->inventory.sound_pickup);

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
	info = mobjinfo + player_class[0]; // TODO

	// create body
	mo = P_SpawnMobj(x, y, 0x80000000, player_class[0]);
	mo->angle = angle;

	if(pl->inventory)
		Z_ChangeTag2(pl->inventory, PU_LEVEL_INV);

	// check for reset
	if(pl->state == PST_REBORN || map_level_info->flags & MAP_FLAG_RESET_INVENTORY)
	{
		// cleanup
		uint32_t killcount;
		uint32_t itemcount;
		uint32_t secretcount;
		uint32_t is_cheater;
		uint32_t extra_inv;
		inventory_t *inventory;
		mobjinfo_t *weapon;

		if(reborn_inventory_hack)
		{
			weapon = pl->readyweapon;
			inventory = pl->inventory;
			extra_inv = pl->backpack;
			extra_inv |= !!pl->powers[pw_allmap] << 1;
			reborn_inventory_hack = 0;
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

		if(!inventory)
		{
			pl->inv_sel = -1;

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
			pl->readyweapon = weapon;
			pl->pendingweapon = weapon;
			pl->backpack = extra_inv & 1;
			pl->powers[pw_allmap] = (extra_inv >> 1) & 1;
			mo->inventory = inventory;
		}
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

	// TODO: translation - player color; not if MF2_DONTTRANSLATE is set

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
	pl->viewheight = info->player.view_height;
	pl->inventory = NULL;
	pl->stbar_update = 0;
	pl->inv_tick = 0;
	pl->info_flags = player_info[idx].flags;

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
// hooks

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
	mobj_set_state(mo, STATE_SET_ANIMATION(anim, 0));
}

static __attribute((regparm(2),no_caller_saved_registers))
mobjinfo_t *prepare_mobj(mobj_t *mo, uint32_t type)
{
	mobjinfo_t *info = mobjinfo + type;
	uint32_t hack = 0;

	// check for replacement
	if(info->replacement)
	{
		type = info->replacement;
		info = mobjinfo + type;
	}

	// check for random
	if(info->extra_type == ETYPE_RANDOMSPAWN)
	{
		uint32_t weight = 0;
		int32_t type = MOBJ_IDX_UNKNOWN;
		uint32_t rnd = P_Random() % info->random_weight;
		uint32_t chance;

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
	mo->netid = mobj_netid;

	// vertical speed
	mo->momz = info->vspeed;

	// random item hack
	if(hack)
		mo->render_style = RS_INVISIBLE;

	// hack for fast projectiles in original attacks
	if(info->flags & MF_MISSILE)
	{
		// this modifies opcodes in 'P_SpawnMissile' even if called from anywhere else
		if(info->fast_speed && (fastparm || gameskill == sk_nightmare))
			utils_install_hooks(hook_fast_missile, 3);
		else
			utils_install_hooks(hook_slow_missile, 3);
	}

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
	if(mo->subsector && mo->subsector->sector->exfloor)
	{
		tmfloorz = mo->subsector->sector->floorheight;
		tmceilingz = mo->subsector->sector->ceilingheight;
		e3d_check_heights(mo, mo->subsector->sector, 1);
		mo->floorz = tmextrafloor;
		mo->ceilingz = tmextraceiling;
	}

	// counters
	if(mo->flags & MF_COUNTKILL)
		totalkills++;
	if(mo->flags & MF_COUNTITEM)
		totalitems++;

	// HACK - other sound slots
	mo->sound_body.x = mo->x;
	mo->sound_body.y = mo->y;
	mo->sound_weapon.x = mo->x;
	mo->sound_weapon.y = mo->y;

	// ZDoom compatibility
	// teleport fog starts teleport sound
	if(mo->type == 39)
		S_StartSound(mo, 35);
}

__attribute((regparm(2),no_caller_saved_registers))
static void kill_animation(mobj_t *mo)
{
	custom_damage_state_t *cst;
	uint32_t state = 0;
	uint32_t ice_death = mo->death_damage_type == DAMAGE_ICE && (mo->flags1 & MF1_ISMONSTER || mo->player) && !(mo->flags2 & MF2_NOICEDEATH);

	if(mo->death_damage_type != DAMAGE_NORMAL && mo->info->damage_states)
		cst = dec_get_damage_animation(mo->info->damage_states, mo->death_damage_type);
	else
		cst = NULL;

	if(mo->health < -mo->info->spawnhealth)
	{
		// find extreme death state
		if(cst && cst->xdeath)
		{
			state = cst->xdeath;
			ice_death = 0;
		} else
			state = mo->info->state_xdeath;
	}

	if(!state)
	{
		// find normal death state
		if(cst && cst->death)
		{
			state = cst->death;
			ice_death = 0;
		} else
			state = mo->info->state_death;
		mo->animation = ANIM_DEATH;
	} else
		mo->animation = ANIM_XDEATH;

	if(ice_death)
		state = STATE_ICE_DEATH_0;

	mobj_set_state(mo, state);

	if(mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	if(mo->player)
		mo->player->extralight = 0;
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t pit_check_thing(mobj_t *thing, mobj_t *tmthing)
{
	uint32_t damage;

	if(map_format != MAP_FORMAT_DOOM && thing->flags & MF_SOLID && (!(tmthing->flags & MF_MISSILE) || tmthing->iflags & MFI_TELEPORT))
	{
		// thing-over-thing
		tmthing->iflags |= MFI_MOBJONMOBJ;
		thing->iflags |= MFI_MOBJONMOBJ;

		if(tmthing->z >= thing->z + thing->height)
		{
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
	if(tmthing->iflags & MFI_TELEPORT)
	{
		teleblock = thing->flags & (MF_SOLID | MF_SHOOTABLE);
		return 1;
	}

	if(tmthing->flags & MF_SKULLFLY)
	{
		damage = tmthing->info->damage;
		if(!(damage & DAMAGE_IS_CUSTOM))
			damage |= DAMAGE_IS_PROJECTILE;

		mobj_damage(thing, tmthing, tmthing, damage, NULL);

		tmthing->flags &= ~MF_SKULLFLY;
		tmthing->momx = 0;
		tmthing->momy = 0;
		tmthing->momz = 0;

		mobj_set_animation(tmthing, ANIM_SPAWN);

		return 0;
	}

	hit_thing = thing->flags & MF_SHOOTABLE ? thing : NULL;

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

		if(thing->flags1 & MF1_GHOST & tmthing->flags1 & MF1_THRUGHOST)
			return 1;

		if(	!dehacked.no_species &&
			(map_level_info->flags & (MAP_FLAG_TOTAL_INFIGHTING | MAP_FLAG_NO_INFIGHTING)) != MAP_FLAG_TOTAL_INFIGHTING &&
			tmthing->target && thing->info->extra_type != ETYPE_PLAYERPAWN
		){
			mobj_t *target = tmthing->target;
			if(target->type == thing->type)
				return 0;
			if(thing->info->species && thing->info->species == target->type)
				return 0;
			if(target->info->species)
			{
				if(target->info->species == thing->type)
					return 0;
				if(target->info->species == thing->info->species)
					return 0;
			}
		}

		if(!(thing->flags & MF_SHOOTABLE))
			return !(thing->flags & MF_SOLID);

		if(!(thing->flags1 & MF1_DONTRIP) && tmthing->flags1 & MF1_RIPPER)
		{
			if(tmthing->rip_thing == thing && tmthing->rip_tick == leveltime)
				return 1;
			tmthing->rip_thing = thing;
			tmthing->rip_tick = leveltime;
			is_ripper = 1;
			damage = tmthing->info->damage;
			if(!(damage & DAMAGE_IS_CUSTOM))
				damage |= DAMAGE_IS_RIPPER;
		} else
		{
			is_ripper = 0;
			damage = tmthing->info->damage;
			if(!(damage & DAMAGE_IS_CUSTOM))
				damage |= DAMAGE_IS_PROJECTILE;
		}

		mobj_damage(thing, tmthing, tmthing->target, damage, NULL);

		if(is_ripper)
		{
			// TODO: ripper blood
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

	if(thing->flags1 & MF1_PUSHABLE && !(tmthing->flags1 & MF1_CANNOTPUSH))
	{
		thing->momx += tmthing->momx / 2;
		thing->momy += tmthing->momy / 2;
		if(thing->flags & MF_SOLID && !(tmthing->flags & MF_SLIDE))
		{
			tmthing->momx = 0;
			tmthing->momy = 0;
		}
	}

	if(thing->flags & MF_SPECIAL)
	{
		uint32_t solid = thing->flags & MF_SOLID;
		if(tmthing->flags & MF_PICKUP)
			touch_mobj(thing, tmthing);
		return !solid;
	}

	return !(thing->flags & MF_SOLID);
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t pit_check_line(mobj_t *tmthing, line_t *ld)
{
	uint32_t is_safe = 0;
	fixed_t z;

	hit_thing = NULL; // ideally, this would be outside

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
			if(ld->flags & ML_BLOCKMONSTERS)
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

	if(ld->special && numspechit < MAXSPECIALCROSS)
	{
		spechit[numspechit] = ld;
		numspechit++;
	}

	return 1;

blocked:
	// ignore when teleporting
	if(tmthing->iflags & MFI_TELEPORT)
		return 1;

	if(ld->special && numspecbump < MAXSPECIALBUMP)
	{
		specbump[numspecbump] = ld;
		numspecbump++;
	}

	return 0;
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t pit_change_sector(mobj_t *thing)
{
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
				if(map_format == MAP_FORMAT_DOOM)
				{
					thing->radius = 0;
					thing->height = 0;
				}

				mobj_set_state(thing, state);
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
		mobj_damage(thing, NULL, NULL, crushchange & 0x8000 ? crushchange & 0x7FFF : 10, NULL); // 'crushchange' sould contain damage value directly

		if(!(thing->flags & MF_NOBLOOD))
		{
			mobj_t *mo;

			mo = P_SpawnMobj(thing->x, thing->y, thing->z + thing->height / 2, thing->info->blood_type);
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
			if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
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

		if(th->function != (void*)0x00031490 + doom_code_segment)
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

		if(th->function != (void*)0x00031490 + doom_code_segment)
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

		if(th->function != (void*)0x00031490 + doom_code_segment)
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
	for(thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		mobj_t *om;

		if(th->function != (void*)0x00031490 + doom_code_segment)
			continue;

		om = (mobj_t*)th;

		if(om->target == mo)
			om->target = NULL;
		if(om->tracer == mo)
			om->tracer = NULL;
		if(om->master == mo)
			om->master = NULL;
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

__attribute((regparm(2),no_caller_saved_registers))
void explode_missile(mobj_t *mo)
{
	uint32_t animation = ANIM_DEATH;

	mo->momx = 0;
	mo->momy = 0;
	mo->momz = 0;

	if(hit_thing)
	{
		if(hit_thing->flags & MF_NOBLOOD)
		{
			if(mo->info->state_crash)
				animation = ANIM_CRASH;
		} else
		{
			if(mo->info->state_xdeath)
				animation = ANIM_XDEATH;
		}

		if(mo->flags2 & MF2_HITTARGET)
			mo->target = hit_thing;
		if(mo->flags2 & MF2_HITMASTER)
			mo->master = hit_thing;
		if(mo->flags2 & MF2_HITTRACER)
			mo->tracer = hit_thing;
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

uint32_t mobj_calc_damage(uint32_t damage)
{
	uint32_t lo = damage & 511;
	uint32_t hi = (damage >> 9) & 511;
	uint32_t add = (damage >> 18) & 511;
	uint32_t mul = ((damage >> 27) & 15) + 1;

	damage = lo;
	if(lo != hi)
		damage += P_Random() % ((hi - lo) + 1);
	damage *= mul;
	damage += add;

	return damage;
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
		damage_type = inflictor->info->damage_type;
	} else
	{
		if_flags1 = 0;
		damage_type = DAMAGE_NORMAL;
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

	if(damage & DAMAGE_IS_CUSTOM)
		damage = mobj_calc_damage(damage);
	else
	switch(damage & DAMAGE_TYPE_CHECK)
	{
		case DAMAGE_IS_PROJECTILE:
			damage = ((P_Random() & 7) + 1) * (damage & 0x00FFFFFF);
		break;
		case DAMAGE_IS_RIPPER:
			damage = ((P_Random() & 3) + 2) * (damage & 0x00FFFFFF);
		break;
	}

	if(damage < 1000000)
	{
		if(target->flags1 & MF1_SPECTRAL && !(if_flags1 & MF1_SPECTRAL))
			return;
		if(target->info->damage_factor[damage_type] != 4)
			damage = (damage * target->info->damage_factor[damage_type]) / 4;
	}

	if(!damage)
		return;

	forced = damage >= 1000000;
	if(damage > 1000000)
		damage = target->health;

	if(source && source->player && source->player->readyweapon)
		kickback = source->player->readyweapon->weapon.kickback;
	else
		kickback = 100;

	player = target->player;

	if(player && gameskill == sk_baby)
		damage /= 2;

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

	if(	target->flags1 & MF1_INVULNERABLE &&
		!forced
	)
		return;

	if(player)
	{
		if(target->subsector->sector->special == 11 && damage >= target->health)
			damage = target->health - 1;

		if(forced)
		{
			player->armortype = 0;
			player->armorpoints = 0;
		}

		if(player->armortype && !mo_dmg_skip_armor)
		{
			uint32_t saved;

			saved = (damage * mobjinfo[player->armortype].armor.percent) / 100;
			if(player->armorpoints <= saved)
			{
				saved = player->armorpoints;
				player->armortype = 0;
			}

			player->armorpoints -= saved;
			damage -= saved;
		}

		if(!(target->flags1 & MF1_NODAMAGE))
		{
			player->health -= damage;
			if(player->health < 0)
				player->health = 0;

			player->damagecount += damage;
			if(player->damagecount > 60)
				player->damagecount = 60; // this is a bit less
		}

		player->attacker = source;

		if(source && source != target && player->cheats & CF_REVENGE)
			mobj_damage(source, NULL, target, 1000000, NULL);

		// I_Tactile ...
	}

	if(!(target->flags1 & MF1_NODAMAGE))
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

				target->death_damage_type = damage_type;

				if(!(target->flags1 & MF1_DONTFALL))
					target->flags &= ~MF_NOGRAVITY;

				if(target->special.special)
				{
					spec_special = target->special.special;
					spec_arg[0] = target->special.arg[0];
					spec_arg[1] = target->special.arg[1];
					spec_arg[2] = target->special.arg[2];
					spec_arg[3] = target->special.arg[3];
					spec_arg[4] = target->special.arg[4];
					spec_activate(NULL, source, 0);
				}

				P_KillMobj(source, target);
				target->flags &= ~MF_COUNTKILL;

				if(player)
					player->extralight = 0;

				if(!target->momz)
					// fake some Z movement for crash states
					target->momz = -1;

				return;
			}
		}
	}

	if(	P_Random() < target->info->painchance[damage_type] &&
		!(target->flags & MF_SKULLFLY)
	) {
		uint32_t state = target->info->state_pain;

		target->flags |= MF_JUSTHIT;

		if(damage_type != DAMAGE_NORMAL && target->info->damage_states)
		{
			custom_damage_state_t *cst;
			cst = dec_get_damage_animation(target->info->damage_states, damage_type);
			if(cst && cst->pain)
				state = cst->pain;
		}

		target->animation = ANIM_PAIN;
		mobj_set_state(target, state);
	}

	target->reactiontime = 0;

	if(	!dehacked.no_infight &&
		!(map_level_info->flags & MAP_FLAG_NO_INFIGHTING) &&
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
		mobj_damage(mo, NULL, NULL, 1000000, NULL);
		return;
	}

	dist = FixedMul(-mo->momz, 16 * FRACUNIT / 23);
	damage = ((FixedMul(dist, dist) / 10) >> FRACBITS) - 24;

	if(	mo->momz > -39 * FRACUNIT &&
		damage > mo->health &&
		mo->health != 1
	)
		damage = mo->health - 1;

	mobj_damage(mo, NULL, NULL, damage, NULL);
}

__attribute((regparm(2),no_caller_saved_registers))
static void mobj_xy_move(mobj_t *mo)
{
	uint32_t dropoff;
	player_t *pl = mo->player;

	oldfloorz = mo->floorz;

	if(!mo->momx && !mo->momy)
	{
		if(mo->flags & MF_SKULLFLY)
		{
			mo->flags &= ~MF_SKULLFLY;
			mo->momz = 0;
			mobj_set_animation(mo, ANIM_SPAWN);
		}
		if(!(mo->flags & MF_MISSILE) && !(mo->iflags & MFI_MOBJONMOBJ))
			return;
	}

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
						explode_missile(mo);
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

	// HACK - move other sound slots
	mo->sound_body.x = mo->x;
	mo->sound_body.y = mo->y;
	mo->sound_weapon.x = mo->x;
	mo->sound_weapon.y = mo->y;

	if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
		// no friction for projectiles
		return;

	if(mo->z > mo->floorz && (!pl || !(mo->flags & MF_NOGRAVITY)))
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
		mo->momx = FixedMul(mo->momx, FRICTION);
		mo->momy = FixedMul(mo->momy, FRICTION);
	}

	// flight friction
	if(pl && mo->flags & MF_NOGRAVITY)
	{
		if(mo->momz > -STOPSPEED && mo->momz < STOPSPEED)
			mo->momz = 0;
		else
			mo->momz = FixedMul(mo->momz, FRICTION);
	}
}

__attribute((regparm(2),no_caller_saved_registers))
static void mobj_z_move(mobj_t *mo)
{
	// check for smooth step up
	if(mo->player && mo->z < mo->floorz)
	{
		mo->player->viewheight -= mo->floorz-mo->z;
		mo->player->deltaviewheight = (mo->info->player.view_height - mo->player->viewheight) >> 3;
	}

	// adjust height
	mo->z += mo->momz;

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
		if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
		{
			if(	mo->flags1 & MF1_SKYEXPLODE ||
				!mo->subsector ||
				!mo->subsector->sector ||
				mo->subsector->sector->floorheight < mo->z ||
				mo->subsector->sector->floorpic != skyflatnum
			){
				hit_thing = NULL;
				explode_missile(mo);
			} else
				mobj_remove(mo);
			return;
		}
		if(mo->flags & MF_CORPSE && !(mo->iflags & MFI_CRASHED))
		{
			uint32_t state = 0;
			uint32_t animation;
			custom_damage_state_t *cst;

			mo->iflags |= MFI_CRASHED;

			if(mo->death_damage_type != DAMAGE_NORMAL && mo->info->damage_states)
				cst = dec_get_damage_animation(mo->info->damage_states, mo->death_damage_type);
			else
				cst = NULL;

			if(mo->health < -mo->info->spawnhealth)
			{
				// find extreme crash state
				if(cst && cst->xcrash)
					state = cst->xcrash;
				else
					state = mo->info->state_xcrash;
			}

			if(!state)
			{
				// find normal crash state
				if(cst && cst->crash)
					state = cst->crash;
				else
					state = mo->info->state_crash;
				animation = ANIM_CRASH;
			} else
				animation = ANIM_XCRASH;

			if(state)
			{
				mo->animation = animation;
				mobj_set_state(mo, state);
			}
		}
	} else
	if(!(mo->flags & MF_NOGRAVITY))
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
				hit_thing = NULL;
				explode_missile(mo);
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

	mobj_damage(thing, tmthing, tmthing, 1000000, NULL);

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

	teleblock = 0;

	mo->flags |= MF_TELEPORT;
	mo->iflags |= MFI_TELEPORT;
	blocked = !P_TryMove(mo, mo->x, mo->y) | teleblock;
	mo->flags &= ~MF_TELEPORT;
	mo->iflags &= ~MFI_TELEPORT;
	if(blocked)
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

	if(flags & TELEF_FOG)
	{
		if(!(mo->flags & MF_MISSILE))
		{
			mo->momx = 0;
			mo->momy = 0;
			mo->momz = 0;
		}

		if(mo->info->telefog[0])
			P_SpawnMobj(xx, yy, zz, mo->info->telefog[0]);

		if(mo->info->telefog[1])
		{
			x = mo->x + 20 * finecosine[angle];
			y = mo->y + 20 * finesine[angle];
			z = mo->z;
			P_SpawnMobj(x, y, z, mo->info->telefog[1]);
		}

		if(mo->player)
			mo->reactiontime = 18;
	}

	if(mo->player)
	{
		fixed_t viewheight = mo->info->player.view_height;
		mo->player->viewz = mo->z + viewheight;
		mo->player->viewheight = viewheight;
		mo->player->deltaviewheight = 0;
	}

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

void mobj_spawn_puff(divline_t *trace, mobj_t *target)
{
	mobj_t *mo;
	uint32_t state = 0;

	if(	target &&
		!(target->flags & MF_NOBLOOD) &&
		!(target->flags1 & MF1_DORMANT) &&
		!(mobjinfo[mo_puff_type].flags1 & MF1_PUFFONACTORS)
	)
		return;

	mo = P_SpawnMobj(trace->x, trace->y, trace->dx, mo_puff_type);

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
		mobj_set_state(mo, state);

	if(mo->flags1 & MF1_RANDOMIZE && mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	mo->angle = shootthing->angle + ANG180;
}

void mobj_spawn_blood(divline_t *trace, mobj_t *target, uint32_t damage)
{
	mobj_t *mo;
	uint32_t state;

	if(target->flags & MF_NOBLOOD)
		return;

	if(target->flags1 & MF1_DORMANT)
		return;

	mo = P_SpawnMobj(trace->x, trace->y, trace->dx, target->info->blood_type);
	mo->momz = FRACUNIT * 2;
	if(!(mo->flags2 & MF2_DONTTRANSLATE))
		mo->translation = target->info->blood_trns;

	state = mo->info->state_spawn;

	if(damage < 9)
		state += 2;
	else
	if(damage <= 12)
		state += 1;

	if(state >= mo->info->state_idx_limit)
		state >= mo->info->state_idx_limit - 1;

	mobj_set_state(mo, state);

	mo->angle = shootthing->angle;
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
	}

	// blockmap
	nofit = 0;
	crushchange = crush;

	for(int32_t x = sec->blockbox[BOXLEFT]; x <= sec->blockbox[BOXRIGHT]; x++)
		for(int32_t y = sec->blockbox[BOXBOTTOM]; y <= sec->blockbox[BOXTOP]; y++)
			P_BlockThingsIterator(x, y, PIT_ChangeSector);

	return nofit;
}

//
// hooks

static const hook_t hook_fast_missile[] =
{
	{0x00031CE1, CODE_HOOK | HOOK_UINT8, offsetof(mobjinfo_t, fast_speed)},
	{0x00031CF6, CODE_HOOK | HOOK_UINT8, offsetof(mobjinfo_t, fast_speed)},
	{0x00031D1F, CODE_HOOK | HOOK_UINT8, offsetof(mobjinfo_t, fast_speed)},
};

static const hook_t hook_slow_missile[] =
{
	{0x00031CE1, CODE_HOOK | HOOK_UINT8, offsetof(mobjinfo_t, speed)},
	{0x00031CF6, CODE_HOOK | HOOK_UINT8, offsetof(mobjinfo_t, speed)},
	{0x00031D1F, CODE_HOOK | HOOK_UINT8, offsetof(mobjinfo_t, speed)},
};

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
	// replace call to 'P_SetMobjState' in 'P_MobjThinker'
	{0x000314F0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)mobj_set_state},
	// fix jump condition in 'P_MobjThinker'
	{0x000314E4, CODE_HOOK | HOOK_UINT8, 0x7F},
	// replace 'P_SetMobjState'
	{0x00030EA0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)mobj_set_state},
	// replace 'P_RemoveMobj'
	{0x00031660, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)mobj_remove},
	// replace 'P_DamageMobj' - use trampoline
	{0x0002A460, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)hook_mobj_damage},
	// disable 'set gravity' in 'P_KillMobj'
	{0x0002A2C8, CODE_HOOK | HOOK_UINT16, 0x07EB},
	// extra stuff in 'P_KillMobj' - replaces animation change
	{0x0002A3C8, CODE_HOOK | HOOK_UINT16, 0xD889},
	{0x0002A3CA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)kill_animation},
	{0x0002A3CF, CODE_HOOK | HOOK_JMP_DOOM, 0x0002A40D},
	// replace 'P_ExplodeMissile'
	{0x00030F00, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)explode_missile},
	// replace 'P_ChangeSector'
	{0x0002BF90, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)mobj_change_sector},
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
	// replace most of 'PIT_ChangeSector'
	{0x0002BEB3, CODE_HOOK | HOOK_UINT16, 0xF089},
	{0x0002BEB5, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)pit_change_sector},
	{0x0002BEBA, CODE_HOOK | HOOK_UINT16, 0x25EB},
	// replace call to 'P_CheckPosition' in 'P_TryMove'
	{0x0002B217, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)try_move_check},
	// add extra floor check into 'P_CheckPosition'
	{0x0002B0D7, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)check_position_extra},
	{0x0002B0DC, CODE_HOOK | HOOK_UINT32, 0x16EBC085},
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
	// replace call to 'P_XYMovement' in 'P_MobjThinker'; call always
	{0x00031496, CODE_HOOK | HOOK_UINT16, 0x12EB},
	{0x000314AA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)mobj_xy_move},
	// replace call to 'P_ZMovement' in 'P_MobjThinker'
	{0x000314C9, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)mobj_z_move},
};

