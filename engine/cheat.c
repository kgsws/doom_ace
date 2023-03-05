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
#include "hitscan.h"
#include "render.h"
#include "think.h"
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

uint_fast8_t cheat_disable;

// cheat list
static void cf_noclip(player_t*,uint8_t*);
static void cf_iddqd(player_t*,uint8_t*);
static void cf_idfa(player_t*,uint8_t*);
static void cf_idkfa(player_t*,uint8_t*);
static void cf_iddt(player_t*,uint8_t*);
static void cf_map(player_t*,uint8_t*);
static void cf_buddha(player_t*,uint8_t*);
static void cf_notarget(player_t*,uint8_t*);
static void cf_fly(player_t*,uint8_t*);
static void cf_mdk(player_t*,uint8_t*);
static void cf_kill(player_t*,uint8_t*);
static void cf_resurrect(player_t*,uint8_t*);
static void cf_summon(player_t*,uint8_t*);
static void cf_freeze(player_t*,uint8_t*);
static void cf_thaw(player_t*,uint8_t*);
static void cf_class(player_t*,uint8_t*);
static void cf_revenge(player_t*,uint8_t*);
static void cf_net_desync(player_t*,uint8_t*);
static void cf_save_light(player_t*,uint8_t*);
static const cheat_func_t cheat_func[] =
{
	// old
	{"idclip", cf_noclip},
	{"iddqd", cf_iddqd},
	{"idfa", cf_idfa},
	{"idkfa", cf_idkfa},
	{"iddt", cf_iddt},
	// new
	{"map", cf_map},
	{"noclip", cf_noclip},
	{"buddha", cf_buddha},
	{"notarget", cf_notarget},
	{"fly", cf_fly},
	{"mdk", cf_mdk},
	{"kill", cf_kill},
	{"resurrect", cf_resurrect},
	{"summon", cf_summon},
	{"freeze", cf_freeze},
	{"thaw", cf_thaw},
	{"class", cf_class},
	// kg
	{"kgRevenge", cf_revenge},
	// dev
//	{"desync", cf_net_desync}, // this should not be enable in release
	{"savelight", cf_save_light},
	// terminator
	{NULL}
};

//
// callbacks

static uint32_t kill_mobj(mobj_t *mo)
{
	if(!(mo->flags1 & MF1_ISMONSTER))
		return 0;
	mobj_damage(mo, NULL, NULL, 1000000, 0);
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
		pl->message = dtxt_STSTR_NCON;
	} else
	{
		pl->cheats &= ~CF_NOCLIP;
		pl->message = dtxt_STSTR_NCOFF;
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
		pl->message = dtxt_STSTR_DQDON;
	} else
	{
		pl->cheats &= ~CF_GODMODE;
		pl->message = dtxt_STSTR_DQDOFF;
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

			inventory_give(mo, type, 1);

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

	pl->message = dtxt_STSTR_FAADDED;
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

	pl->message = dtxt_STSTR_KFAADDED;
}

static void cf_iddt(player_t *pl, uint8_t *arg)
{
	uint32_t cheating = am_cheating + 1;
	if(cheating > 2)
		am_cheating = 0;
	else
		am_cheating = cheating;
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

static void cf_notarget(player_t *pl, uint8_t *arg)
{
	mobj_t *mo = pl->mo;

	mo->flags1 ^= MF1_NOTARGET;
	if(mo->flags1 & MF1_NOTARGET)
		pl->message = "Notarget mode ON";
	else
		pl->message = "Notarget mode OFF";
}

static void cf_fly(player_t *pl, uint8_t *arg)
{
	mobj_t *mo = pl->mo;

	mo->flags ^= MF_NOGRAVITY;
	if(mo->flags & MF_NOGRAVITY)
		pl->message = "Flight ON";
	else
		pl->message = "Flight OFF";
}

static void cf_mdk(player_t *pl, uint8_t *arg)
{
	fixed_t slope;

	if(player_info[pl - players].flags & PLF_AUTO_AIM || map_level_info->flags & MAP_FLAG_NO_FREELOOK)
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
		mobj_for_each(kill_mobj);
}

static void cf_resurrect(player_t *pl, uint8_t *arg)
{
	mobjinfo_t *info = pl->mo->info;
	mobj_t *mo = pl->mo;

	if(pl->state != PST_DEAD && pl->health > 0)
		return;

	if(mo->info->extra_type != ETYPE_PLAYERPAWN)
	{
		pl->message = "Original body was lost!";
		return;
	}

	pl->health = info->spawnhealth;
	pl->state = PST_LIVE;

	mo->health = info->spawnhealth;
	mo->height = info->height;
	mo->radius = info->radius;
	mo->flags = info->flags;
	mo->flags1 = info->flags1;
	mo->flags2 = info->flags2;

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
		return;

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
		missile_stuff(mo, pl->mo, NULL, projectile_speed(mo->info), pl->mo->angle, pl->mo->pitch, 0);
}

static void cf_freeze(player_t *pl, uint8_t *arg)
{
	think_freeze_mode = !think_freeze_mode;
	if(think_freeze_mode)
		pl->message = "Freeze mode ON";
	else
		pl->message = "Freeze mode OFF";
}

static void cf_thaw(player_t *pl, uint8_t *arg)
{
	pl->prop &= ~((1 << PROP_FROZEN) | (1 << PROP_TOTALLYFROZEN));
}

static void cf_class(player_t *pl, uint8_t *arg)
{
	uint64_t alias;

	alias = tp_hash64(arg);

	for(uint32_t i = 0; i < num_player_classes; i++)
	{
		mobjinfo_t *info = mobjinfo + player_class[i];

		if(info->alias == alias)
		{
			player_info_changed = 1;
			player_class_change = i;
			pl->cheats |= CF_CHANGE_CLASS;
			pl->message = "Cheat activated!";
			return;
		}
	}

	pl->message = "Invalid player class!";
}

static void cf_revenge(player_t *pl, uint8_t *arg)
{
	pl->cheats ^= CF_REVENGE;
	if(pl->cheats & CF_REVENGE)
		pl->message = "Revenge mode ON";
	else
		pl->message = "Revenge mode OFF";
}

static void cf_net_desync(player_t *pl, uint8_t *arg)
{
	// deliberately desynchronize local player
	if(pl != players + consoleplayer)
		return;
	if(pl->health > 1)
	{
		pl->health ^= 1;
		pl->mo->health = pl->health;
	}
}

static void cf_save_light(player_t *pl, uint8_t *arg)
{
	uint32_t fail = 0;
	uint8_t text[16];

	if(sector_light_count <= 1)
		return;

	for(uint32_t i = 1; i < sector_light_count; i++)
	{
		int32_t fd;
		sector_light_t *cl = sector_light + i;

		if(cl->color != 0x0FFF)
		{
			doom_sprintf(text, "+%03X%04X.lmp", cl->fade, cl->color);
			fd = doom_open_WR(text);
			if(fd >= 0)
			{
				doom_write(fd, cl->cmap, 256 * 32);
				doom_close(fd);
			} else
				fail = 1;
		}

		if(cl->fade != 0x0000)
		{
			doom_sprintf(text, "+%03X%04X.lmp", cl->fade, 0x0FFF);
			fd = doom_open_WR(text);
			if(fd >= 0)
			{
				doom_write(fd, cl->fmap, 256 * 32);
				doom_close(fd);
			} else
				fail = 1;
		}
	}

	pl->message = fail ? "Export error!" : "Color tables exported";
}

//
// API

void cheat_check(uint32_t pidx)
{
	const cheat_func_t *cf = cheat_func;
	cheat_buf_t *cb = cheat_buf + pidx;
	player_t *pl = players + pidx;
	uint8_t *arg;

	if(!cb->tpos)
		return;

	cb->text[cb->tpos] = 0;

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
			if(cheat_disable && cf->func != cf_map) // allow map changes (and synchronization)
				return;
			pl->cheats |= CF_IS_CHEATER; // mark cheaters forever
			cf->func(pl, arg);
			break;
		}
		cf++;
	}

	if(cheat_disable)
		return;

	if(!cf->name)
	{
		message_is_important = 1;
		pl->message = "Unknown cheat!";
		return;
	}

	if(pidx != consoleplayer || (!pl->message && (netgame || demoplayback)))
	{
		message_is_important = 1;
		players[consoleplayer].message = "Cheat activated!";
		return;
	}

	if(pl->message && pidx == consoleplayer)
		message_is_important = 1;
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

void cheat_reset()
{
	hu_char_tail = 0;
	hu_char_head = 0;
	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		cheat_buf[i].tpos = -1;
		cheat_buf[i].dpos = -1;
	}
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
	{0x0003BA34, CODE_HOOK | HOOK_UINT8, TIC_CMD_CHEAT},
	// do not display sent message
	{0x0003BC6D, CODE_HOOK | HOOK_UINT8, 0xEB},
	// disable chat macros
	{0x0003BB53, CODE_HOOK | HOOK_UINT16, 0xE990},
	// disable direct chat
	{0x0003BA44, CODE_HOOK | HOOK_JMP_DOOM, 0x0003BCB1},
	// disable chat processing
	{0x0003B78E, CODE_HOOK | HOOK_JMP_DOOM, 0x0003B8C7},
	{0x0003B64F, CODE_HOOK | HOOK_UINT16, 0x1AEB},
};

