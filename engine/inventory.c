// kgsws' ACE Engine
////
// Inventory system.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "decorate.h"
#include "inventory.h"

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
		info->extra_type == ETYPE_POWERUP
	;
}

inventory_t *inventory_find(mobj_t *mo, uint16_t type)
{
	inventory_t *item;

	item = mo->inventory;
	while(item)
	{
		if(item->type == type)
			return item;
		item = item->prev;
	}

	return NULL;
}

uint32_t inventory_give(mobj_t *mo, uint16_t type, uint16_t count)
{
	// returns amount left over
	uint32_t ret;
	inventory_t *item;
	mobjinfo_t *info;
	uint32_t max_count;

	// ammo inheritance
	info = mobjinfo + type;
	if(info->extra_type == ETYPE_AMMO_LINK)
		type = info->inventory.special;

	// checks
	info = mobjinfo + type;
	if(!inventory_is_valid(info))
		I_Error("Invalid inventory item!");

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

		return ret;
	}

	// not found; create new
	item = doom_malloc(sizeof(inventory_t*));
	if(!item)
		I_Error("Failed to allocate memory for inventory slot!");

	item->next = NULL;
	item->prev = mo->inventory;
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

	if(mo->inventory)
		mo->inventory->next = item;

	mo->inventory = item;

	return ret;
}

uint32_t inventory_take(mobj_t *mo, uint16_t type, uint16_t count)
{
	// returns amount taken
	inventory_t *item;
	mobjinfo_t *info;

	// checks
	info = mobjinfo + type;
	if(!inventory_is_valid(info))
		I_Error("Invalid inventory item!");

	// find
	item = inventory_find(mo, type);
	if(!item)
		return 0;

	// remove
	if(item->count <= count)
	{
		count -= item->count;
		item->count = 0;
		// TODO: remove (MFE_INVENTORY_KEEPDEPLETED)
		return count;
	} else
	{
		item->count -= count;
		return 0;
	}
}

void inventory_clear(mobj_t *mo)
{
	inventory_t *item;

	if(!mo->inventory)
		return;

	item = mo->inventory;
	while(item)
	{
		inventory_t *tmp = item;
		item = item->prev;
		doom_free(item);
	}
}

uint32_t inventory_check(mobj_t *mo, uint16_t type)
{
	inventory_t *item;

	item = inventory_find(mo, type);
	if(!item)
		return 0;

	return item->count;
}

