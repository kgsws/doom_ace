// kgsws' ACE Engine
////
// Inventory system.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "decorate.h"
#include "inventory.h"
#include "stbar.h"

//
// funcs

static inline void inv_check_player(mobj_t *mo, mobjinfo_t *info, inventory_t *item, uint32_t take)
{
	// update player status bar
	// update player inventory bar
	player_t *pl = mo->player;

	if(!pl)
		return;

	if(info->extra_type == ETYPE_WEAPON)
	{
		pl->stbar_update |= STU_WEAPON;
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

	if(take)
	{
		if(pl->inv_sel != item)
			return;

		// currently selected item was taken

		// pick next
		item = item->next;
		while(item)
		{
			info = mobjinfo + item->type;

			if(info->inventory.icon && info->eflags & MFE_INVENTORY_INVBAR)
			{
				// got one
				pl->inv_sel = item;
				return;
			}

			item = item->next;
		}

		// pick previous
		item = pl->inv_sel->prev;
		while(item)
		{
			info = mobjinfo + item->type;

			if(info->inventory.icon && info->eflags & MFE_INVENTORY_INVBAR)
			{
				// got one
				pl->inv_sel = item;
				return;
			}

			item = item->prev;
		}

		// not found
		pl->inv_sel = NULL;
		return;
	}

	if(pl->inv_sel)
		return;

	pl->inv_sel = item;
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

		goto finished;
	}

	// not found; create new
	item = doom_malloc(sizeof(inventory_t));
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

finished:
	// player update
	inv_check_player(mo, info, item, 0);

	// done
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
			if(item->next)
				item->next->prev = item->prev;
			else
				mo->inventory = item->prev;

			if(item->prev)
				item->prev->next = item->next;

			doom_free(item);
		}
	} else
		item->count -= count;

	// done
	return count;
}

void inventory_destroy(inventory_t *items)
{
	while(items)
	{
		inventory_t *tmp = items;
		items = items->prev;
		doom_free(tmp);
	}
}

void inventory_clear(mobj_t *mo)
{
	inventory_destroy(mo->inventory);
	mo->inventory = NULL;
	// status bar update
	if(mo->player)
		mo->player->stbar_update = STU_EVERYTHING;
}

uint32_t inventory_check(mobj_t *mo, uint16_t type)
{
	inventory_t *item;

	item = inventory_find(mo, type);
	if(!item)
		return 0;

	return item->count;
}

void inventory_hubstrip(mobj_t *mo)
{
	mobjinfo_t *info;
	inventory_t *item;

	item = mo->inventory;
	while(item)
	{
		inventory_t *tmp = item;
		info = mobjinfo + tmp->type;

		item = item->prev;

		if(tmp->count > info->inventory.hub_count)
		{
			tmp->count = info->inventory.hub_count;
			if(!tmp->count && info->extra_type != ETYPE_AMMO && !(info->eflags & MFE_INVENTORY_KEEPDEPLETED))
			{
				// delete from inventory
				// but keep 'ammo' for status bar
				if(tmp->next)
					tmp->next->prev = tmp->prev;
				else
					mo->inventory = tmp->prev;

				if(tmp->prev)
					tmp->prev->next = tmp->next;

				doom_free(tmp);
			}
		}
	}
}

// DEBUG
void inventory_dump(const uint8_t *txt, inventory_t *items)
{
	uint32_t idx = 0;
	doom_printf("INV %s\n", txt);
	while(items)
	{
		doom_printf("INV[%02u] t %u c %u\n", idx, items->type, items->count);
		idx++;
		items = items->prev;
	}
}

