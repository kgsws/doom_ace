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
#include "demo.h"
#include "cheat.h"
#include "extra3d.h"

#define STOPSPEED	0x1000
#define FRICTION	0xe800
#define FLOATSPEED	(FRACUNIT * 4)
#define GRAVITY	FRACUNIT

static line_t *ceilingline;
static line_t *floorline;

static line_t *specbump[MAXSPECIALBUMP];
static uint32_t numspecbump;

uint32_t mo_puff_type = 37;
uint32_t mo_puff_flags;

uint32_t mobj_netid;

static uint_fast8_t kill_xdeath;

//

// this only exists because original animations are all over the place in 'mobjinfo_t'
const uint16_t base_anim_offs[NUM_MOBJ_ANIMS] =
{
	[ANIM_SPAWN] = offsetof(mobjinfo_t, state_spawn),
	[ANIM_SEE] = offsetof(mobjinfo_t, state_see),
	[ANIM_PAIN] = offsetof(mobjinfo_t, state_pain),
	[ANIM_MELEE] = offsetof(mobjinfo_t, state_melee),
	[ANIM_MISSILE] = offsetof(mobjinfo_t, state_missile),
	[ANIM_DEATH] = offsetof(mobjinfo_t, state_death),
	[ANIM_XDEATH] = offsetof(mobjinfo_t, state_xdeath),
	[ANIM_RAISE] = offsetof(mobjinfo_t, state_raise),
	[ANIM_HEAL] = offsetof(mobjinfo_t, state_heal),
	[ANIM_CRUSH] = offsetof(mobjinfo_t, state_crush),
	[ANIM_CRASH] = offsetof(mobjinfo_t, state_crash),
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
			mo->animation = (state >> 16) & 0xFF;

			if(mo->animation < NUM_MOBJ_ANIMS)
				state = *((uint16_t*)((void*)mo->info + base_anim_offs[mo->animation]));
			else
				state = mo->info->extra_states[mo->animation - NUM_MOBJ_ANIMS];

			if(state)
				state += offset;

			if(state >= mo->info->state_idx_limit)
				I_Error("[MOBJ] State jump '+%u' is invalid!", offset);
		}

		if(!state)
		{
			mobj_remove(mo);
			return 0;
		}

		st = states + state;
		mo->state = st;
		mo->sprite = st->sprite;
		mo->frame = st->frame;

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
			anim = (state >> 16) & 0xFF;

			if(anim < NUM_MOBJ_ANIMS)
				state = *((uint16_t*)((void*)info + base_anim_offs[anim]));
			else
				state = info->extra_states[anim - NUM_MOBJ_ANIMS];

			if(state)
				state += offset;

			if(state >= info->state_idx_limit)
				I_Error("[MOBJ] State jump '+%u' is invalid!", offset);
		}

		if(state <= 1)
		{
			mo->custom_inventory = NULL;
			return !state;
		}

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

static uint32_t give_health(mobj_t *mo, uint32_t count, uint32_t max_count)
{
	uint32_t maxhp;

	if(max_count)
		maxhp = max_count;
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
			give_health(mo, 100, 0);
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
		I_Error("Nested CustomInventory is not supported!");

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
		I_Error("Nested CustomInventory is not supported!");

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

	if(!toucher->player || toucher->player->playerstate != PST_LIVE)
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
			if(!give_health(toucher, info->inventory.count, info->inventory.max_count) && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
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
				if(demoplayback == DEMO_OLD || pl->info_flags & PLF_AUTO_SWITCH)
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
				given = give_health(toucher, info->spawnhealth, toucher->info->spawnhealth);
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
		pl->itemcount++;

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

	// check for reset
	if(pl->playerstate == PST_REBORN || map_level_info->flags & MAP_FLAG_RESET_INVENTORY)
	{
		// cleanup
		uint32_t killcount;
		uint32_t itemcount;
		uint32_t secretcount;
		uint32_t is_cheater;

		inventory_destroy(pl->inventory);

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
		pl->playerstate = PST_LIVE;

		if(info == mobjinfo)
			pl->health = dehacked.start_health;
		else
			pl->health = info->spawnhealth;

		pl->pendingweapon = NULL;
		pl->readyweapon = NULL;

		// give 'depleted' original ammo; for status bar
		inventory_give(mo, 63, 0); // Clip
		inventory_give(mo, 69, 0); // Shell
		inventory_give(mo, 67, 0); // Cell
		inventory_give(mo, 65, 0); // RocketAmmo

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
	}

	// TODO: translation not in flags
	mo->flags |= idx << 26;

	mo->angle = angle;
	mo->player = pl;
	mo->health = pl->health;

	pl->mo = mo;
	pl->playerstate = PST_LIVE;
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
		player_viewheight(pl->viewheight);
	}

	return mo;
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t set_mobj_animation(mobj_t *mo, uint8_t anim)
{
	mobj_set_state(mo, STATE_SET_ANIMATION(anim, 0));
}

static __attribute((regparm(2),no_caller_saved_registers))
mobjinfo_t *prepare_mobj(mobj_t *mo, uint32_t type)
{
	uint32_t tmp = 8;
	mobjinfo_t *info;

	// find replacement
	while(mobjinfo[type].replacement)
	{
		if(!tmp)
			I_Error("[DECORATE] Too many replacements!");
		type = mobjinfo[type].replacement;
		tmp--;
	}

	info = mobjinfo + type;

	// clear memory
	memset(mo, 0, sizeof(mobj_t));

	// fill in new stuff
	mobj_netid++;
	mo->type = type;
	mo->flags1 = info->flags1;
	mo->netid = mobj_netid;

	// vertical speed
	mo->momz = info->vspeed;

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
		tmfloorz = mo->floorz;
		tmceilingz = mo->ceilingz;
		e3d_check_heights(mo, mo->subsector->sector, 1);
		mo->floorz = tmextrafloor;
		mo->ceilingz = tmextraceiling;
	}

	// ZDoom compatibility
	// teleport fog starts teleport sound
	if(mo->type == 39)
		S_StartSound(mo, 35);
}

__attribute((regparm(2),no_caller_saved_registers))
static void kill_animation(mobj_t *mo)
{
	// custom death animations can be added

	if(	mo->info->state_xdeath &&
		(	kill_xdeath == 1 ||
			(!kill_xdeath && mo->health < -mo->info->spawnhealth)
		)
	)
		set_mobj_animation(mo, ANIM_XDEATH);
	else
		set_mobj_animation(mo, ANIM_DEATH);

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

	if(map_format != MAP_FORMAT_DOOM && !(tmthing->flags & MF_MISSILE))
	{
		// thing-over-thing
		// TODO: overlapping things should be marked and checked every tic

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

	if(tmthing->flags & MF_SKULLFLY)
	{
		damage = tmthing->info->damage;
		if(!(damage & DAMAGE_IS_CUSTOM))
			damage |= DAMAGE_IS_PROJECTILE;

		mobj_damage(thing, tmthing, tmthing, damage, 0);

		tmthing->flags &= ~MF_SKULLFLY;
		tmthing->momx = 0;
		tmthing->momy = 0;
		tmthing->momz = 0;

		mobj_set_animation(tmthing, ANIM_SPAWN);

		return 0;
	}

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

		mobj_damage(thing, tmthing, tmthing->target, damage, 0);

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

	if(ld->hexspec && numspechit < MAXSPECIALCROSS)
	{
		spechit[numspechit] = ld;
		numspechit++;
	}

	return 1;

blocked:
	if(ld->hexspec && numspecbump < MAXSPECIALBUMP)
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
		if(thing->health <= 0)
		{
			thing->flags1 |= MF1_DONTGIB;

			if(!(thing->flags & MF_NOBLOOD) || demoplayback == DEMO_OLD)
			{
				uint32_t state;

				if(!thing->info->state_crush)
					state = 895;
				else
					state = thing->info->state_crush;

				thing->flags &= ~MF_SOLID;
				thing->radius = 0;
				thing->height = 0;

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
		mobj_damage(thing, NULL, NULL, 10, 0);

		if(!(thing->flags & MF_NOBLOOD) || demoplayback == DEMO_OLD)
		{
			mobj_t *mo;

			mo = P_SpawnMobj(thing->x, thing->y, thing->z + thing->height / 2, 38);
			mo->momx = (P_Random() - P_Random()) << 12;
			mo->momy = (P_Random() - P_Random()) << 12;
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

void mobj_use_item(mobj_t *mo, inventory_t *item)
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
			if(!give_health(mo, info->spawnhealth, mo->info->spawnhealth))
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
			return give_health(mo, (uint32_t)count * (uint32_t)info->inventory.count, info->inventory.max_count);
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
				given = give_health(mo, info->spawnhealth, mo->info->spawnhealth);
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
	if(!thinkercap.next)
		// this happens only before any level was loaded
		return 0;

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
	inventory_destroy(mo->inventory);
	P_UnsetThingPosition(mo);
	S_StopSound(mo);
	P_RemoveThinker((thinker_t*)mo);
}

__attribute((regparm(2),no_caller_saved_registers))
void explode_missile(mobj_t *mo)
{
	mo->momx = 0;
	mo->momy = 0;
	mo->momz = 0;

	set_mobj_animation(mo, ANIM_DEATH);

	if(mo->flags1 & MF1_RANDOMIZE && mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	mo->flags &= ~MF_MISSILE;

	S_StartSound(mo, mo->info->deathsound);
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

	if(!(target->flags & MF_SHOOTABLE))
		return;

	if(target->flags1 & MF1_DORMANT)
		return;

	if(target->health <= 0)
		return;

	if(target->flags & MF_SKULLFLY)
	{
		target->momx = 0;
		target->momy = 0;
		target->momz = 0;
	}

	if(pufftype)
		if_flags1 = pufftype->flags1;
	else
	if(inflictor)
		if_flags1 = inflictor->flags1;
	else
		if_flags1 = 0;

	if(target->flags1 & MF1_SPECTRAL && !(if_flags1 & MF1_SPECTRAL))
		return;

	if(damage & DAMAGE_IS_CUSTOM)
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
	} else
	switch(damage & DAMAGE_TYPE_CHECK)
	{
		case DAMAGE_IS_PROJECTILE:
			damage = ((P_Random() & 7) + 1) * (damage & 0x00FFFFFF);
		break;
		case DAMAGE_IS_RIPPER:
			damage = ((P_Random() & 3) + 2) * (damage & 0x00FFFFFF);
		break;
	}

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

		if(demoplayback != DEMO_OLD)
		{
			thrust = (thrust * (FRACUNIT >> 5) * 100 / target->info->mass);
			if(thrust > (30 * FRACUNIT) >> 2)
				thrust = 30 * FRACUNIT;
			else
				thrust <<= 2;
		} else
			thrust = thrust * (FRACUNIT >> 3) * 100 / target->info->mass;

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

		if(player->armortype)
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
			mobj_damage(source, NULL, target, 1000000, 0);

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
				kill_xdeath = 0;
				if(if_flags1 & MF1_NOEXTREMEDEATH)
					kill_xdeath = -1;
				else
				if(if_flags1 & MF1_EXTREMEDEATH)
					kill_xdeath = 1;
				P_KillMobj(source, target);
				return;
			}
		}
	}

	if(	P_Random() < target->info->painchance &&
		!(target->flags & MF_SKULLFLY)
	) {
		target->flags |= MF_JUSTHIT;
		mobj_set_state(target, STATE_SET_ANIMATION(ANIM_PAIN, 0));
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
		mobj_damage(mo, NULL, NULL, 1000000, 0);
		return;
	}

	dist = FixedMul(-mo->momz, 16 * FRACUNIT / 23);
	damage = ((FixedMul(dist, dist) / 10) >> FRACBITS) - 24;

	if(	mo->momz > -39 * FRACUNIT &&
		damage > mo->health &&
		mo->health != 1
	)
		damage = mo->health - 1;

	mobj_damage(mo, NULL, NULL, damage, 0);
}

__attribute((regparm(2),no_caller_saved_registers))
static void mobj_xy_move(mobj_t *mo)
{
	player_t *pl = mo->player;

	if(!mo->momx && !mo->momy)
	{
		if(mo->flags & MF_SKULLFLY)
		{
			mo->flags &= ~MF_SKULLFLY;
			mo->momz = 0;
			mobj_set_animation(mo, ANIM_SPAWN);
		}
		return;
	}

	if(demoplayback != DEMO_OLD)
	{
		// new, better movement code
		// split movement into half-radius steps
		fixed_t ox, oy; // original location
		fixed_t mx, my; // momentnum
		fixed_t nx, ny; // new location
		fixed_t step; // increment

		ox = mo->x;
		oy = mo->y;

		nx = mo->x;
		ny = mo->y;

		mx = mo->momx;
		my = mo->momy;

		step = mo->radius / 2;
		if(step < 4 * FRACUNIT)
			step = 4 * FRACUNIT;

		while(mx || my)
		{
			// current X step
			if(mx > step)
			{
				nx += step;
				mx -= step;
			} else
			if(mx < -step)
			{
				nx -= step;
				mx += step;
			} else
			{
				nx += mx;
				mx = 0;
			}

			// current Y step
			if(my > step)
			{
				ny += step;
				my -= step;
			} else
			if(my < -step)
			{
				ny -= step;
				my += step;
			} else
			{
				ny += my;
				my = 0;
			}

			// move
			if(P_TryMove(mo, nx, ny))
			{
				// check for teleport
				if(mo->x != nx || mo->y != ny)
					break;
			} else
			{
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
						if(demoplayback == DEMO_OLD)
						{
							if(ceilingline->backsector->ceilingpic == skyflatnum)
								mobj_remove(mo);
						} else
						{
							if(	(ceilingline->backsector->ceilingpic == skyflatnum && mo->z + mo->height >= ceilingline->backsector->ceilingheight) ||
								(ceilingline->frontsector->ceilingpic == skyflatnum && mo->z + mo->height >= ceilingline->frontsector->ceilingheight)
							)
								mobj_remove(mo);
						}
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
		}
	} else
		// use old movement code
		P_XYMovement(mo);

	if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
		// no friction for projectiles
		return;

	if(mo->z > mo->floorz)
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
		(!pl || (pl->cmd.forwardmove== 0 && pl->cmd.sidemove == 0))
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
			if(mo->player && mo->momz < GRAVITY * -8)
			{
				if(mo->health > 0)
				{
					mo->player->deltaviewheight = mo->momz >> 3;
					S_StartSound(mo, mo->info->player.sound.land);
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
			if(	demoplayback == DEMO_OLD ||
				mo->flags1 & MF1_SKYEXPLODE ||
				!mo->subsector ||
				!mo->subsector->sector ||
				mo->subsector->sector->floorheight < mo->z ||
				mo->subsector->sector->floorpic != skyflatnum
			)
				explode_missile(mo);
			else
				mobj_remove(mo);
			return;
		}
	} else
	if(!(mo->flags & MF_NOGRAVITY))
	{
		if(mo->momz == 0)
			mo->momz = GRAVITY * -2;
		else
			mo->momz -= GRAVITY;
	}

	if(mo->z + mo->height > mo->ceilingz)
	{
		// hit the ceiling
		if(mo->momz > 0)
			mo->momz = 0;

		mo->z = mo->ceilingz - mo->height;

		if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
		{
			if(	demoplayback == DEMO_OLD ||
				mo->flags1 & MF1_SKYEXPLODE ||
				!mo->subsector ||
				!mo->subsector->sector ||
				mo->subsector->sector->ceilingheight > mo->z + mo->height ||
				mo->subsector->sector->ceilingpic != skyflatnum
			)
				explode_missile(mo);
			else
				mobj_remove(mo);
			return;
		}
	}
}

void mobj_spawn_puff(divline_t *trace, mobj_t *target)
{
	mobj_t *mo;

	if(	target &&
		!(target->flags & MF_NOBLOOD) &&
		!(target->flags1 & MF1_DORMANT) &&
		!(mobjinfo[mo_puff_type].flags1 & MF1_PUFFONACTORS)
	)
		return;

	if(!(mo_puff_flags & FBF_NORANDOMPUFFZ))
		trace->dx += ((P_Random() - P_Random()) << 10);

	mo = P_SpawnMobj(trace->x, trace->y, trace->dx, mo_puff_type);

	if(!target && mo->info->state_crash)
		mobj_set_state(mo, mo->info->state_crash);
	else
	if(attackrange <= 64 * FRACUNIT && mo->info->state_melee)
		mobj_set_state(mo, mo->info->state_melee);

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

	mo = P_SpawnMobj(trace->x, trace->y, trace->dx, 38);
	mo->momz = FRACUNIT * 2;

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
	// extra stuff in 'P_KillMobj' - replaces animation change
	{0x0002A3C8, CODE_HOOK | HOOK_UINT16, 0xD889},
	{0x0002A3CA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)kill_animation},
	{0x0002A3CF, CODE_HOOK | HOOK_JMP_DOOM, 0x0002A40D},
	// replace 'P_ExplodeMissile'
	{0x00030F00, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)explode_missile},
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
	{0x00027776, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_SEE << 8)}, // A_Look
	{0x00027779, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Look
	{0x0002782A, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_SPAWN << 8)}, // A_Chase
	{0x0002782D, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	{0x0002789D, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_MELEE << 8)}, // A_Chase
	{0x000278A0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	{0x000278DD, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_MISSILE << 8)}, // A_Chase
	{0x000278E0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	// replace 'P_SetMobjState' with new animation system (P_MovePlayer)
	{0x0003324B, CODE_HOOK | HOOK_UINT16, 0xB880},
	{0x0003324D, CODE_HOOK | HOOK_UINT32, offsetof(mobj_t, animation)},
	{0x00033251, CODE_HOOK | HOOK_UINT8, ANIM_SPAWN}, // check for
	{0x00033255, CODE_HOOK | HOOK_UINT32, ANIM_SEE}, // replace with
	{0x00033259, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation},
	// skip some stuff in 'P_XYMovement'
	{0x00030F6B, CODE_HOOK | HOOK_UINT16, 0x3BEB}, // disable 'MF_SKULLFLY'
	{0x000310B7, CODE_HOOK | HOOK_UINT16, 0x14EB}, // after-move checks
	// replace call to 'P_XYMovement' in 'P_MobjThinker'
	{0x000314AA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)mobj_xy_move},
	// replace call to 'P_ZMovement' in 'P_MobjThinker'
	{0x000314C9, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)mobj_z_move},
};

