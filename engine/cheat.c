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
#include "textpars.h"
#include "cheat.h"

typedef struct
{
	const char *name;
	void (*func)(player_t*,uint8_t*);
} cheat_func_t;

//

cheat_buf_t *cheat_buf;

static uint32_t *message_is_important;
static mobj_t *tmp_mo;

// cheat list
static void cf_mdk(player_t*,uint8_t*);
static void cf_kill(player_t*,uint8_t*);
static void cf_resurrect(player_t*,uint8_t*);
static void cf_summon(player_t*,uint8_t*);
static const cheat_func_t cheat_func[] =
{
	{"mdk", cf_mdk},
	{"kill", cf_kill},
	{"resurrect", cf_resurrect},
	{"summon", cf_summon},
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

static void cf_mdk(player_t *pl, uint8_t *name)
{
	P_BulletSlope(pl->mo);
	P_LineAttack(pl->mo, pl->mo->angle, MISSILERANGE, *bulletslope, 1000000);
}

static void cf_kill(player_t *pl, uint8_t *name)
{
	if(!name[0])
	{
		mobj_damage(pl->mo, NULL, pl->mo, 1000000, 0);
		return;
	}

	if(!strcmp(name, "monsters"))
	{
		tmp_mo = pl->mo;
		mobj_for_each(kill_mobj);
	}
}

static void cf_resurrect(player_t *pl, uint8_t *name)
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

	mobj_set_animation(mo, ANIM_SPAWN);
	weapon_setup(pl);
}

static void cf_summon(player_t *pl, uint8_t *name)
{
	int32_t type;
	mobj_t *mo;
	fixed_t x, y, z;
	uint32_t ang;

	if(!name[0])
	{
		pl->message = "usage: summon thing_type";
		return;
	}

	type = mobj_check_type(tp_hash64(name));
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
		ang = mo->angle >> ANGLETOFINESHIFT;
		x = mo->x + FixedMul(z, finecosine[ang]);
		y = mo->y + FixedMul(z, finesine[ang]);
		z = mo->z;
	}

	mo = P_SpawnMobj(x, y, z, type);
	mo->angle = pl->mo->angle;
	if(mo->flags & MF_MISSILE)
		missile_stuff(mo, pl->mo, NULL, pl->mo->angle);
}

//
// API

uint32_t cheat_check(uint32_t pidx)
{
	const cheat_func_t *cf = cheat_func;
	cheat_buf_t *cb = cheat_buf + pidx;
	player_t *pl = players + pidx;
	uint8_t *arg;

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
			cf->func(pl, arg);
			break;
		}
		cf++;
	}
	if(!cf->name)
		pl->message = "Unknown cheat!";

	if(pl->message)
		*message_is_important = 1;

	cb->len = -1;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// disable original cheats
	{0x00039B08, CODE_HOOK | HOOK_UINT16, 0xE990},
	{0x00025403, CODE_HOOK | HOOK_UINT16, 0xE990},
	// enable chat in singleplayer
	{0x0003BA10, CODE_HOOK | HOOK_UINT16, 0x07EB},
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
	{0x000756F4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&cheat_buf}, // size: 0x1D4
	{0x000756F0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&message_is_important},
};

