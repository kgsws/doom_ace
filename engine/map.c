// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "decorate.h"
#include "map.h"
#include "mobj.h"
#include "inventory.h"

mapthing_t *playerstarts;
mapthing_t *deathmatchstarts;
mapthing_t **deathmatch_p;

uint32_t *nomonsters;
uint32_t *fastparm;
uint32_t *respawnparm;

uint32_t *netgame;
uint32_t *deathmatch;
uint32_t *gamemap;
uint32_t *gameepisode;
uint32_t *gameskill;
uint32_t *leveltime;

uint32_t *totalkills;
uint32_t *totalitems;
uint32_t *totalsecret;

uint32_t *numsides;
uint32_t *numlines;
uint32_t *numsectors;
line_t **lines;
vertex_t **vertexes;
side_t **sides;
sector_t **sectors;

uint8_t map_lump_name[9];
int32_t map_lump_idx;

uint_fast8_t map_skip_things;

//

static const uint8_t skillbits[] = {1, 1, 2, 4, 4};

//
// callbacks

static uint32_t cb_free_inventory(mobj_t *mo)
{
	inventory_clear(mo);
	return 0;
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
void map_load_setup()
{
	if(*gamemode)
		doom_sprintf(map_lump_name, "MAP%02u", *gamemap);
	else
		doom_sprintf(map_lump_name, "E%uM%u", *gameepisode, *gamemap);

	map_lump_idx = W_GetNumForName(map_lump_name); // TODO: do not crash

	// free old inventories
	mobj_for_each(cb_free_inventory);

	// reset netID
	mobj_netid = 1; // 0 is NULL, so start with 1

	P_SetupLevel();
}

__attribute((regparm(2),no_caller_saved_registers))
static void spawn_map_thing(mapthing_t *mt)
{
	uint32_t idx;
	mobj_t *mo;
	mobjinfo_t *info;
	fixed_t x, y, z;

	// deathmatch starts
	if(mt->type == 11)
	{
		if(*deathmatch_p < deathmatchstarts + 10)
		{
			**deathmatch_p = *mt;
			*deathmatch_p = *deathmatch_p + 1;
		}
		return;
	}

	// player starts
	if(mt->type && mt->type <= 4)
	{
		playerstarts[mt->type - 1] = *mt;
		if(!*deathmatch /* && !map_skip_things */)
			spawn_player(mt);
		return;
	}

	if(map_skip_things)
		return;

	// check network game
	if(!*netgame && mt->options & 16)
		return;

	// check skill level
	if(*gameskill > sizeof(skillbits) || !(mt->options & skillbits[*gameskill]))
		return;

	// backward search for type
	idx = num_mobj_types;
	do
	{
		idx--;
		if(mobjinfo[idx].doomednum == mt->type)
			break;
	} while(idx);
	if(!idx)
		idx = MOBJ_IDX_UNKNOWN;
	info = mobjinfo + idx;

	// 'not in deathmatch'
	if(*deathmatch && info->flags & MF_NOTDMATCH)
		return;

	// '-nomonsters'
	if(*nomonsters && info->flags1 & MF1_ISMONSTER)
		return;

	// position
	x = (fixed_t)mt->x << FRACBITS;
	y = (fixed_t)mt->y << FRACBITS;
	z = info->flags & MF_SPAWNCEILING ? 0x7FFFFFFF : 0x80000000;

	// spawn
	mo = P_SpawnMobj(x, y, z, idx);
	mo->spawnpoint = *mt;
	mo->angle = ANG45 * (mt->angle / 45);

	if(mo->tics > 0)
		mo->tics = 1 + (P_Random() % mo->tics);

	if(mt->options & 8)
		mo->flags |= MF_AMBUSH;

	if(mo->flags & MF_COUNTKILL)
		*totalkills = *totalkills + 1;

	if(mo->flags & MF_COUNTITEM)
		*totalitems = *totalitems + 1;
}

__attribute((regparm(2),no_caller_saved_registers))
static uint32_t check_door_key(line_t *line, mobj_t *mo)
{
	player_t *pl = mo->player;
	uint16_t k0, k1;
	uint8_t *text;

	switch(line->special)
	{
		case 26:
		case 32:
			if(!pl)
				return 0;
			k0 = 47;
			k1 = 52;
			text = (uint8_t*)0x00022C90 + doom_data_segment;
		break;
		case 27:
		case 34:
			if(!pl)
				return 0;
			k0 = 49;
			k1 = 50;
			text = (uint8_t*)0x00022CB8 + doom_data_segment;
		break;
		case 28:
		case 33:
			if(!pl)
				return 0;
			k0 = 48;
			k1 = 51;
			text = (uint8_t*)0x00022CE0 + doom_data_segment;
		break;
		default:
			return 1;
	}

	if(inventory_check(mo, k0))
		return 1;

	if(inventory_check(mo, k1))
		return 1;

	pl->message = text;
	S_StartSound(mo, 34); // TODO: use-fail sound

	return 0;
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t check_obj_key(line_t *line, mobj_t *mo)
{
	player_t *pl = mo->player;
	uint16_t k0, k1;
	uint8_t *text;

	switch(line->special)
	{
		case 99:
		case 133:
			if(!pl)
				return 0;
			k0 = 47;
			k1 = 52;
			text = (uint8_t*)0x00022C08 + doom_data_segment;
		break;
		case 134:
		case 135:
			if(!pl)
				return 0;
			k0 = 48;
			k1 = 51;
			text = (uint8_t*)0x00022C34 + doom_data_segment;
		break;
		case 136:
		case 137:
			if(!pl)
				return 0;
			k0 = 49;
			k1 = 50;
			text = (uint8_t*)0x00022C60 + doom_data_segment;
		break;
		default:
			goto do_door;
	}

	if(inventory_check(mo, k0))
		goto do_door;

	if(inventory_check(mo, k1))
		goto do_door;

	pl->message = text;
	S_StartSound(mo, 34); // TODO: use-fail sound

	return 0;

do_door:
	return EV_DoDoor(line, 6); // only 'blazeOpen' is ever used here
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'P_SetupLevel' in 'G_DoLoadLevel'
	{0x000200AA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)map_load_setup},
	// replace call to 'P_SpawnMapThing' in 'P_LoadThings'
	{0x0002E1F9, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)spawn_map_thing},
	// replace key checks in 'EV_VerticalDoor'
	{0x00026C85, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)check_door_key},
	{0x00026C8A, CODE_HOOK | HOOK_UINT16, 0xC085},
	{0x00026C8D, CODE_HOOK | HOOK_JMP_DOOM, 0x00026E97},
	{0x00026C8C, CODE_HOOK | HOOK_UINT16, 0x840F},
	{0x00026C92, CODE_HOOK | HOOK_JMP_DOOM, 0x00026D3E},
	// replace calls to 'EV_DoLockedDoor'
	{0x00030B0E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_obj_key},
	{0x00030E55, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_obj_key},
	// change map name variable in 'P_SetupLevel'
	{0x0002E858, CODE_HOOK | HOOK_UINT16, 0x61EB},
	{0x0002E8BB, CODE_HOOK | HOOK_UINT8, 0xB8},
	{0x0002E8BC, CODE_HOOK | HOOK_UINT32, (uint32_t)map_lump_name},
	// import variables
	{0x0002C0D0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&playerstarts},
	{0x0002C154, DATA_HOOK | HOOK_IMPORT, (uint32_t)&deathmatchstarts},
	{0x0002C150, DATA_HOOK | HOOK_IMPORT, (uint32_t)&deathmatch_p},
	{0x0002A3C0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&nomonsters},
	{0x0002A3C4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&fastparm},
	{0x0002A3C8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&respawnparm},
	{0x0002B400, DATA_HOOK | HOOK_IMPORT, (uint32_t)&netgame},
	{0x0002B3FC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&deathmatch},
	{0x0002B3E8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gamemap},
	{0x0002B3F8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gameepisode},
	{0x0002B3E0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gameskill},
	{0x0002CF80, DATA_HOOK | HOOK_IMPORT, (uint32_t)&leveltime},
	{0x0002B3C8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalsecret},
	{0x0002B3D0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalitems},
	{0x0002B3D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalkills},
	{0x0002C11C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsides},
	{0x0002C134, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numlines},
	{0x0002C14C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsectors},
	{0x0002C120, DATA_HOOK | HOOK_IMPORT, (uint32_t)&lines},
	{0x0002C138, DATA_HOOK | HOOK_IMPORT, (uint32_t)&vertexes},
	{0x0002C118, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sides},
	{0x0002C148, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sectors},
};

