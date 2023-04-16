// kgsws' ACE Engine
////
// Inventory system.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "config.h"
#include "decorate.h"
#include "inventory.h"
#include "player.h"
#include "stbar.h"

//
// functions

int32_t inv_player_next(mobj_t *mo)
{
	if(!mo->inventory)
		return 0;

	for(int32_t i = mo->player->inv_sel + 1; i < mo->inventory->numslots; i++)
	{
		mobjinfo_t *info = mobjinfo + mo->inventory->slot[i].type;
		if(info->inventory.icon && info->eflags & MFE_INVENTORY_INVBAR)
		{
			mo->player->inv_sel = i;
			return 1;
		}
	}

	return 0;
}

int32_t inv_player_prev(mobj_t *mo)
{
	if(!mo->inventory)
		return 0;

	for(int32_t i = mo->player->inv_sel - 1; i >= 0; i--)
	{
		mobjinfo_t *info = mobjinfo + mo->inventory->slot[i].type;
		if(info->inventory.icon && info->eflags & MFE_INVENTORY_INVBAR)
		{
			mo->player->inv_sel = i;
			return 1;
		}
	}

	return 0;
}

static void inv_check_player(mobj_t *mo, mobjinfo_t *info, invitem_t *item, uint32_t flags)
{
	// update player status bar
	// update player inventory bar
	player_t *pl = mo->player;
	inventory_t *inv = mo->inventory;

	if(!pl)
		return;

	if(info->extra_type == ETYPE_WEAPON)
	{
		pl->stbar_update |= STU_WEAPON;
		return;
	}

	if(info->extra_type == ETYPE_AMMO)
	{
		uint32_t type = info - mobjinfo;

		if(	flags && // only 'take' or 'fresh'
			(
				type == (uint32_t)mod_config.ammo_bullet ||
				type == (uint32_t)mod_config.ammo_shell ||
				type == (uint32_t)mod_config.ammo_rocket ||
				type == (uint32_t)mod_config.ammo_cell
			)
		)
			pl->stbar_update |= STU_AMMO;

		return;
	}

	if(!info->inventory.icon)
		return;

	if(info->extra_type == ETYPE_KEY)
	{
		pl->stbar_update |= STU_KEYS;
		return;
	}

	if(!(info->eflags & MFE_INVENTORY_INVBAR))
		// this also filters out ammo
		return;

	if(flags & 1)
	{
		if(pl->inv_sel < 0)
			return;

		if(inv->slot + pl->inv_sel != item)
			return;

		// currently selected item was taken

		// pick next
		if(inv_player_next(mo))
			return;

		// or pick previous
		if(inv_player_prev(mo))
			return;

		// or none
		pl->inv_sel = -1;
		return;
	}

	if(pl->inv_sel >= 0)
		return;

	// find slot with this item
	// there SHOULD be one

	for(uint32_t i = 0; i < inv->numslots; i++)
	{
		if(inv->slot + i == item)
		{
			pl->inv_sel = i;
			return;
		}
	}
}

static invitem_t *create_slot(mobj_t *mo)
{
	inventory_t *inv = mo->inventory;
	inventory_t *inn;
	uint32_t count;

	if(!inv)
	{
		count = mo->player ? INV_SLOT_PLAYER : INV_SLOT_MOBJ;

		inv = Z_Malloc(sizeof(inventory_t) + sizeof(invitem_t) * count, PU_LEVEL_INV, NULL);
		inv->numslots = count;
		mo->inventory = inv;

		for(uint32_t i = 0; i < count; i++)
			inv->slot[i].type = 0;

		return inv->slot;
	}

	for(uint32_t i = 0; i < inv->numslots; i++)
	{
		invitem_t *item = inv->slot + i;
		if(!item->type)
			// found free slot
			return item;
	}

	// no free slot found
	count = inv->numslots;
	count += mo->player ? INV_SLOT_PLAYER : INV_SLOT_MOBJ;

	// add new slots
	inn = Z_Malloc(sizeof(inventory_t) + sizeof(invitem_t) * count, PU_LEVEL_INV, NULL);
	inn->numslots = count;
	mo->inventory = inn;

	// copy original
	for(uint32_t i = 0; i < inv->numslots; i++)
		inn->slot[i] = inv->slot[i];

	// clear the rest
	for(uint32_t i = inv->numslots; i < count; i++)
		inn->slot[i].type = 0;

	// return first new slot
	count = inv->numslots;

	// free original
	Z_Free(inv);

	// players need to update status bar
	if(mo->player)
		mo->player->stbar_update = STU_EVERYTHING;

	return inn->slot + count;
}

//
// API

uint32_t inventory_is_valid(mobjinfo_t *info)
{
	return	info->extra_type == ETYPE_INVENTORY ||
		info->extra_type == ETYPE_INVENTORY_CUSTOM ||
		info->extra_type == ETYPE_WEAPON ||
		info->extra_type == ETYPE_AMMO ||
		info->extra_type == ETYPE_AMMO_LINK ||
		info->extra_type == ETYPE_KEY ||
		info->extra_type == ETYPE_ARMOR ||
		info->extra_type == ETYPE_ARMOR_BONUS ||
		info->extra_type == ETYPE_POWERUP ||
		info->extra_type == ETYPE_HEALTH_PICKUP
	;
}

uint32_t inventory_is_usable(mobjinfo_t *info)
{
	return	info->inventory.icon &&
		info->eflags & MFE_INVENTORY_INVBAR &&
		(
			info->extra_type == ETYPE_INVENTORY_CUSTOM ||
			info->extra_type == ETYPE_ARMOR ||
			info->extra_type == ETYPE_ARMOR_BONUS ||
			info->extra_type == ETYPE_POWERUP ||
			info->extra_type == ETYPE_HEALTH_PICKUP
		)
	;
}

invitem_t *inventory_find(mobj_t *mo, uint16_t type)
{
	inventory_t *inv = mo->inventory;

	if(!inv)
		return NULL;

	for(uint32_t i = 0; i < inv->numslots; i++)
	{
		invitem_t *item = inv->slot + i;
		if(item->type == type)
			return item;
	}

	return NULL;
}

uint32_t inventory_give(mobj_t *mo, uint16_t type, uint16_t count)
{
	// returns amount left over
	uint32_t ret;
	invitem_t *item;
	mobjinfo_t *info;
	uint32_t max_count;

	// ammo inheritance
	info = mobjinfo + type;
	if(info->extra_type == ETYPE_AMMO_LINK)
		type = info->inventory.special;

	// checks
	info = mobjinfo + type;
	if(!inventory_is_valid(info))
		engine_error("ACE", "Invalid inventory item!");

	// backpack hack
	if(	info->extra_type == ETYPE_AMMO &&
		mo->player &&
		mo->player->backpack
	)
		max_count = info->ammo.max_count;
	else
		max_count = info->inventory.max_count;

	// zero check
	if(!max_count)
		return count;

	// another zero check
	if(!(info->eflags & MFE_INVENTORY_KEEPDEPLETED) && !count)
		return 0;

	// find in inventory
	item = inventory_find(mo, type);
	if(item)
	{
		// found; check & add
		uint32_t newcount;

		if(!count)
			// nothing to add
			return 0;

		newcount = item->count + count;
		if(newcount > INV_MAX_COUNT)
			newcount = INV_MAX_COUNT;

		if(newcount > max_count)
			newcount = max_count;

		ret = ((uint32_t)item->count + (uint32_t)count) - newcount;

		item->count = newcount;

		// player update
		inv_check_player(mo, info, item, 0);

		return ret;
	}

	// not found; create new
	item = create_slot(mo);
	item->type = type;

	if(count > max_count)
	{
		item->count = max_count;
		ret = count - max_count;
	} else
	{
		item->count = count;
		ret = 0;
	}

	// player update
	inv_check_player(mo, info, item, 2);

	// done
	return ret;
}

uint32_t inventory_take(mobj_t *mo, uint16_t type, uint16_t count)
{
	// returns amount taken
	invitem_t *item;
	mobjinfo_t *info;

	info = mobjinfo + type;

	// base powerup
	if(info->extra_type == ETYPE_POWERUP_BASE)
	{
		if(!mo->player)
			return 0;

		count = !!mo->player->powers[info->powerup.type];

		powerup_take(mo->player, info);

		return count;
	}

	// check
	if(!inventory_is_valid(info))
		engine_error("ACE", "Invalid inventory item!");

	// find
	item = inventory_find(mo, type);
	if(!item)
		return 0;

	if(item->count <= count)
	{
		// player update
		inv_check_player(mo, info, item, 1);

		// removed
		count = item->count;
		item->count = 0;

		if(info->extra_type != ETYPE_AMMO && !(info->eflags & MFE_INVENTORY_KEEPDEPLETED))
		{
			// delete from inventory
			// but keep 'ammo' for status bar
			item->type = 0;
		}
	} else
		item->count -= count;

	// done
	return count;
}

void inventory_clear(mobj_t *mo)
{
	if(!mo->inventory)
		return;

	Z_Free(mo->inventory);
	mo->inventory = NULL;

	if(mo->player)
		mo->player->stbar_update = STU_EVERYTHING;
}

uint32_t inventory_check(mobj_t *mo, uint16_t type)
{
	invitem_t *item;

	item = inventory_find(mo, type);
	if(!item)
		return 0;

	return item->count;
}

void inventory_hubstrip(mobj_t *mo)
{
	inventory_t *inv = mo->inventory;

	if(!inv)
		return;

	for(uint32_t i = 0; i < inv->numslots; i++)
	{
		mobjinfo_t *info;
		invitem_t *item = inv->slot + i;

		if(!item->type)
			continue;

		info = mobjinfo + item->type;

		if(item->count > info->inventory.hub_count)
		{
			item->count = info->inventory.hub_count;
			if(!item->count && info->extra_type != ETYPE_AMMO && !(info->eflags & MFE_INVENTORY_KEEPDEPLETED))
			{
				// delete from inventory
				// but keep 'ammo' for status bar
				item->type = 0;
			}
		}
	}
}

