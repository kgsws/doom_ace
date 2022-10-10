// kgsws' ACE Engine
////
// New cheat system.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "decorate.h"
#include "inventory.h"
#include "weapon.h"
#include "mobj.h"
#include "player.h"
#include "action.h"
#include "map.h"
#include "stbar.h"
#include "demo.h"
#include "textpars.h"
#include "cheat.h"

typedef struct
{
	const char *name;
	void (*func)(player_t*,uint8_t*);
} cheat_func_t;

//

cheat_buf_t *cheat_buf;

static uint32_t *am_cheating;
static uint32_t *message_is_important;
static mobj_t *tmp_mo;

// cheat list
static void cf_noclip(player_t*,uint8_t*);
static void cf_iddqd(player_t*,uint8_t*);
static void cf_idfa(player_t*,uint8_t*);
static void cf_idkfa(player_t*,uint8_t*);
static void cf_idclev(player_t*,uint8_t*);
static void cf_iddt(player_t*,uint8_t*);
static void cf_map(player_t*,uint8_t*);
static void cf_buddha(player_t*,uint8_t*);
static void cf_mdk(player_t*,uint8_t*);
static void cf_kill(player_t*,uint8_t*);
static void cf_resurrect(player_t*,uint8_t*);
static void cf_summon(player_t*,uint8_t*);
static void cf_revenge(player_t*,uint8_t*);
static const cheat_func_t cheat_func[] =
{
	// old
	{"idclip", cf_noclip},
	{"iddqd", cf_iddqd},
	{"idfa", cf_idfa},
	{"idkfa", cf_idkfa},
	{"idclev", cf_idclev},
	{"iddt", cf_iddt},
	// new
	{"map", cf_map},
	{"noclip", cf_noclip},
	{"buddha", cf_buddha},
	{"mdk", cf_mdk},
	{"kill", cf_kill},
	{"resurrect", cf_resurrect},
	{"summon", cf_summon},
	// kg
	{"kgRevenge", cf_revenge},
	// terminator
	{NULL}
};

//
// callbacks

static uint32_t kill_mobj(mobj_t *mo)
{
	if(!(mo->flags1 & MF1_ISMONSTER))
		return 0;
	mobj_damage(mo, NULL, tmp_mo, 1000000, 0);
	return 0;
}

//
// cheat functions

static void cf_noclip(player_t *pl, uint8_t *arg)
{
	mobj_t *mo = pl->mo;

	mo->flags ^= MF_NOCLIP;
	if(mo->flags & MF_NOCLIP)
	{
		pl->cheats |= CF_NOCLIP;
		pl->message = (uint8_t*)0x00023EB8 + doom_data_segment;
	} else
	{
		pl->cheats &= ~CF_NOCLIP;
		pl->message = (uint8_t*)0x00023ECC + doom_data_segment;
	}
}

static void cf_iddqd(player_t *pl, uint8_t *arg)
{
	mobj_t *mo = pl->mo;

	mo->flags1 &= ~MF1_BUDDHA;
	mo->flags1 ^= MF1_INVULNERABLE;
	if(mo->flags1 & MF1_INVULNERABLE)
	{
		mo->health = mo->info->spawnhealth;
		pl->health = mo->health;
		pl->cheats &= ~CF_BUDDHA;
		pl->cheats |= CF_GODMODE;
		pl->message = (uint8_t*)0x00023E30 + doom_data_segment;
	} else
	{
		pl->cheats &= ~CF_GODMODE;
		pl->message = (uint8_t*)0x00023E48 + doom_data_segment;
	}
}

static void cf_idfa(player_t *pl, uint8_t *arg)
{
	mobj_t *mo = pl->mo;

	// update status bar after this
	pl->stbar_update = STU_EVERYTHING;

	// give backpack (extra)
	pl->backpack = 1;

	// give all (allowed) weapons
	for(uint32_t i = 0; i < NUM_WPN_SLOTS; i++)
	{
		uint16_t *ptr;

		ptr = pl->mo->info->player.wpn_slot[i];
		if(!ptr)
			continue;

		while(*ptr)
		{
			uint16_t type = *ptr++;
			mobjinfo_t *info = mobjinfo + type;

			inventory_give(mo, type, INV_MAX_COUNT);

			if(info->weapon.ammo_type[0])
				inventory_give(mo, info->weapon.ammo_type[0], INV_MAX_COUNT);
			if(info->weapon.ammo_type[1])
				inventory_give(mo, info->weapon.ammo_type[1], INV_MAX_COUNT);
		}
	}

	// armor
	if(pl->armorpoints < 200)
	{
		pl->armorpoints = 200;
		pl->armortype = 44;
	}

	pl->message = (uint8_t*)0x00023E60 + doom_data_segment;
}

static void cf_idkfa(player_t *pl, uint8_t *arg)
{
	mobj_t *mo = pl->mo;

	// all weapons & stuff
	cf_idfa(pl, arg);

	// all keys
	for(uint32_t i = 0; i < num_mobj_types; i++)
	{
		mobjinfo_t *info = mobjinfo + i;

		if(info->extra_type == ETYPE_KEY)
			inventory_give(mo, i, INV_MAX_COUNT);
	}

	pl->message = (uint8_t*)0x00023E78 + doom_data_segment;
}

static void cf_idclev(player_t *pl, uint8_t *arg)
{
	uint32_t map, epi;

	if(gamemode)
	{
		// map only
		if(doom_sscanf(arg, "%u", &map) == 1)
		{
			G_DeferedInitNew(gameskill, 1, map);
			return;
		}
	} else
	{
		// episode and map
		if(doom_sscanf(arg, "%u %u", &epi, &map) == 2)
		{
			G_DeferedInitNew(gameskill, epi, map);
			return;
		}
	}

	pl->message = "Wrong level!";
}

static void cf_iddt(player_t *pl, uint8_t *arg)
{
	uint32_t cheating = *am_cheating + 1;
	if(cheating > 2)
		*am_cheating = 0;
	else
		*am_cheating = cheating;
}

static void cf_map(player_t *pl, uint8_t *arg)
{
	while(*arg == ' ')
		arg++;

	if(!arg[0])
	{
		pl->message = "Wrong level!";
		return;
	}

	strncpy(map_lump.name, arg, 8);
	G_DeferedInitNew(gameskill, 0, 0);
}

static void cf_buddha(player_t *pl, uint8_t *arg)
{
	mobj_t *mo = pl->mo;

	mo->flags1 &= ~MF1_INVULNERABLE;
	mo->flags1 ^= MF1_BUDDHA;
	if(mo->flags1 & MF1_BUDDHA)
	{
		pl->cheats &= ~CF_GODMODE;
		pl->cheats |= CF_BUDDHA;
		pl->message = "Buddha mode ON";
	} else
	{
		pl->cheats &= ~CF_BUDDHA;
		pl->message = "Buddha mode OFF";
	}
}

static void cf_mdk(player_t *pl, uint8_t *arg)
{
	fixed_t slope;

	if(pl->info_flags & PLF_AUTO_AIM || map_level_info->flags & MAP_FLAG_NO_FREELOOK)
		slope = P_AimLineAttack(pl->mo, pl->mo->angle, 1024 * FRACUNIT);
	else
		linetarget = NULL;

	if(!linetarget)
		slope = finetangent[(pl->mo->pitch + ANG90) >> ANGLETOFINESHIFT];

	P_LineAttack(pl->mo, pl->mo->angle, MISSILERANGE, slope, 1000000);
}

static void cf_kill(player_t *pl, uint8_t *arg)
{
	if(!arg[0])
	{
		mobj_damage(pl->mo, NULL, pl->mo, 1000000, 0);
		return;
	}

	if(!strcmp(arg, "monsters"))
	{
		tmp_mo = pl->mo;
		mobj_for_each(kill_mobj);
	}
}

static void cf_resurrect(player_t *pl, uint8_t *arg)
{
	mobjinfo_t *info = pl->mo->info;
	mobj_t *mo = pl->mo;

	if(pl->playerstate != PST_DEAD && pl->health > 0)
		return;

	pl->health = info->spawnhealth;
	pl->playerstate = PST_LIVE;

	mo->health = info->spawnhealth;
	mo->height = info->height;
	mo->radius = info->radius;
	mo->flags = info->flags;
	mo->flags1 = info->flags1;

	cheat_player_flags(pl);

	mobj_set_animation(mo, ANIM_SPAWN);
	weapon_setup(pl);
}

static void cf_summon(player_t *pl, uint8_t *arg)
{
	int32_t type;
	mobj_t *mo;
	fixed_t x, y, z;

	if(!arg[0])
	{
		pl->message = "usage: summon thing_type";
		return;
	}

	type = mobj_check_type(tp_hash64(arg));
	if(type < 0)
	{
		pl->message = "Unknown thing type!";
		return;
	}

	mo = pl->mo;

	if(mo->flags & MF_NOCLIP || mobjinfo[type].flags & MF_MISSILE)
	{
		x = mo->x;
		y = mo->y;
		z = mo->z;
		if(mobjinfo[type].flags & MF_MISSILE)
			z += (pl->mo->height / 2) + pl->mo->info->player.attack_offs;
	} else
	{
		uint32_t ang = mo->angle >> ANGLETOFINESHIFT;
		z = ((mo->radius + mobjinfo[type].radius) * 3) / 2;
		x = mo->x + FixedMul(z, finecosine[ang]);
		y = mo->y + FixedMul(z, finesine[ang]);
		z = mo->z;
	}

	mo = P_SpawnMobj(x, y, z, type);
	mo->angle = pl->mo->angle;
	if(mo->flags & MF_MISSILE)
		missile_stuff(mo, pl->mo, NULL, pl->mo->angle, pl->mo->pitch, 0);
}

static void cf_revenge(player_t *pl, uint8_t *arg)
{
	pl->cheats ^= CF_REVENGE;
	if(pl->cheats & CF_REVENGE)
		pl->message = "Revenge mode ON";
	else
		pl->message = "Revenge mode OFF";
}

//
// API

void cheat_check(uint32_t pidx)
{
	const cheat_func_t *cf = cheat_func;
	cheat_buf_t *cb = cheat_buf + pidx;
	player_t *pl = players + pidx;
	uint8_t *arg;

	if(!cb->len)
		return;

	cb->text[cb->len] = 0;

	// split code and paramenter
	arg = cb->text;
	while(*arg && *arg != ' ')
		arg++;
	if(*arg == ' ')
	{
		*arg++ = 0;
		// skip spaces
		while(*arg == ' ')
			arg++;
	}

	// find cheat function
	while(cf->name)
	{
		if(!strcmp(cf->name, cb->text))
		{
			pl->cheats |= CF_IS_CHEATER; // mark cheaters forever
			cf->func(pl, arg);
			break;
		}
		cf++;
	}
	if(!cf->name)
		pl->message = "Unknown cheat!";

	if(!pl->message && demoplayback)
		pl->message = "Cheat activated!";

	if(pl->message)
		*message_is_important = 1;

	cb->len = -1;
}

void cheat_player_flags(player_t *pl)
{
	mobj_t *mo = pl->mo;

	if(pl->cheats & CF_NOCLIP)
		mo->flags |= MF_NOCLIP;
	if(pl->cheats & CF_GODMODE)
		mo->flags1 |= MF1_INVULNERABLE;
	if(pl->cheats & CF_BUDDHA)
		mo->flags1 |= MF1_BUDDHA;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// disable original cheats
	{0x00039B08, CODE_HOOK | HOOK_UINT16, 0xE990},
	{0x00025403, CODE_HOOK | HOOK_UINT16, 0xE990},
	// enable chat in singleplayer, disable 'enter' to repeat last message
	{0x0003B9EE, CODE_HOOK | HOOK_UINT16, 0x29EB},
	// change chat key
	{0x0003BA1B, CODE_HOOK | HOOK_UINT8, '`'},
	// enable lowercase
	{0x0003BC1F, CODE_HOOK | HOOK_UINT16, 0x15EB},
	{0x0003C26B, CODE_HOOK | HOOK_UINT8, 0x7E},
	// cheat marker (first byte)
	{0x0003BA34, CODE_HOOK | HOOK_UINT8, CHAT_CHEAT_MARKER},
	// do not display sent message
	{0x0003BC6D, CODE_HOOK | HOOK_UINT8, 0xEB},
	// disable chat macros
	{0x0003BB53, CODE_HOOK | HOOK_UINT16, 0xE990},
	// disable direct chat
	{0x0003BA44, CODE_HOOK | HOOK_JMP_DOOM, 0x0003BCB1},
	// disable chat processing
	{0x0003B78E, CODE_HOOK | HOOK_JMP_DOOM, 0x0003B8C7},
	{0x0003B64F, CODE_HOOK | HOOK_UINT16, 0x1AEB},
	// import variables
	{0x000756F4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&cheat_buf}, // size: 0x1D4 (w_inputbuffer)
	{0x000756F0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&message_is_important},
	{0x00012C50, DATA_HOOK | HOOK_IMPORT, (uint32_t)&am_cheating},
};

