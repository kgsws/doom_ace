// kgsws' Doom ACE
// MOBJ stuff.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "mobj.h"
#include "decorate.h"
#include "weapon.h"

// mobj ID
uint32_t mobj_id;

// current player class // TODO: from menu? multiplayer?
uint16_t player_class = 0;

// custom viewheight for player classes
static hook_t hook_list_viewheight[] =
{
	{0x00031227, CODE_HOOK | HOOK_UINT32},
	{0x00033088, CODE_HOOK | HOOK_UINT32},
	{0x000330ee, CODE_HOOK | HOOK_UINT32},
	{0x000330f7, CODE_HOOK | HOOK_UINT32},
	{0x00033101, CODE_HOOK | HOOK_UINT32},
	{0x0003310d, CODE_HOOK | HOOK_UINT32},
};

// animation state offsets in mobjinfo_t
static uint32_t anim_state_base[] =
{
	offsetof(mobjinfo_t, spawnstate),
	offsetof(mobjinfo_t, seestate),
	offsetof(mobjinfo_t, painstate),
	offsetof(mobjinfo_t, meleestate),
	offsetof(mobjinfo_t, missilestate),
	offsetof(mobjinfo_t, deathstate),
	offsetof(mobjinfo_t, xdeathstate),
	offsetof(mobjinfo_t, raisestate)
};

// animation state offsets in mobjinfo_t->extrainfo
static uint32_t anim_state_extra[] =
{
	offsetof(mobj_extra_info_t, state_heal),
	offsetof(mobj_extra_info_t, state_death_fire),
	offsetof(mobj_extra_info_t, state_death_ice),
	offsetof(mobj_extra_info_t, state_death_disintegrate),
};

// animation state offsets in mobjinfo_t->extra for inventory
static uint32_t anim_state_inventory[] =
{
	offsetof(dextra_inventory_t, state_pickup),
	offsetof(dextra_inventory_t, state_use)
};

// animation state offsets in mobjinfo_t->extra for weapon
static uint32_t anim_state_weapon[] =
{
	offsetof(dextra_weapon_t, ready_state),
	offsetof(dextra_weapon_t, deselect_state),
	offsetof(dextra_weapon_t, select_state),
	offsetof(dextra_weapon_t, fire1_state),
	offsetof(dextra_weapon_t, fire2_state),
	offsetof(dextra_weapon_t, hold1_state),
	offsetof(dextra_weapon_t, hold2_state),
	offsetof(dextra_weapon_t, flash1_state),
	offsetof(dextra_weapon_t, flash2_state),
	offsetof(dextra_weapon_t, deadlow_state)
};

// this will find state slot for requested animation ID
__attribute((regparm(2),no_caller_saved_registers))
uint32_t *P_GetAnimPtr(uint8_t anim, mobjinfo_t *info)
{
	static uint32_t zero_ptr;

	if(anim < MOANIM_HEAL)
		return (void*)info + anim_state_base[anim];

	if(anim < INANIM_PICKUP)
	{
		// extra states
		if(info->extrainfo)
			return (void*)info->extrainfo + anim_state_extra[anim - MOANIM_HEAL];
	} else
	if(anim < WEANIM_READY)
	{
		// inventory states
		if(info->extra && info->extra->type == DECORATE_EXTRA_INVENTORY && info->extra->inventory.flags & INVFLAG_IS_CUSTOM)
			return (void*)info->extra + anim_state_inventory[anim - INANIM_PICKUP];
	} else
	if(anim < MOBJ_ANIM_COUNT)
	{
		// weapon states
		if(info->extra && info->extra->type == DECORATE_EXTRA_WEAPON)
			return (void*)info->extra + anim_state_weapon[anim - WEANIM_READY];
	}

	zero_ptr = 0;
	return &zero_ptr;
}

// new animation system
__attribute((regparm(2),no_caller_saved_registers))
void P_SetMobjAnimation(mobj_t *mo, uint8_t anim)
{
	uint32_t state;

	// set new animation ID
	mo->animation = anim;

	// translate animation ID
	state = *P_GetAnimPtr(anim, mo->info);
	// set new state
	P_SetMobjState(mo, state);
}

//
// replacements

__attribute((regparm(2),no_caller_saved_registers))
uint32_t P_SetMobjState(mobj_t *mo, uint32_t state)
{
	while(1)
	{
		if(!state)
		{
			mo->state = NULL;
			P_RemoveMobj(mo);
			return 0;
		}

		// check for animation changes
		if(state & 0xFF000000)
		{
			// set new animation ID
			mo->animation = (state >> 24) - 1;
			// actual state number
			state &= 0xFFFFFF;
		}

		// set new state
		state_t *st = states + state;
		mo->state = st;
		mo->tics = st->tics;
		if(mo->sprite != KEEP_SPRITE_OF_FRAME)
			mo->sprite = st->sprite;
		if((mo->frame & FF_FRAMEMASK) != KEEP_SPRITE_OF_FRAME)
			mo->frame = st->frame;

		// call codepointer
		if(st->acp)
			st->acp(mo);

		// next
		if(mo->tics == -2)
		{
			// state was chagned in codepointer
			// passed state is not a pointer
			state = (uint32_t)mo->state;
		} else
		{
			// check for ending
			if(mo->tics)
				return 1;
			state = st->nextstate;
		}
	}
}

__attribute((regparm(2),no_caller_saved_registers))
void P_ExplodeMissile(mobj_t *mo)
{
	mo->momx = 0;
	mo->momy = 0;
	mo->momz = 0;

	P_SetMobjAnimation(mo, MOANIM_DEATH);

	if(mo->flags & MF_RANDOMIZE && mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	mo->flags &= ~MF_MISSILE;

	if(mo->info->deathsound)
		S_StartSound(mo, mo->info->deathsound);
}

__attribute((regparm(2),no_caller_saved_registers))
void set_player_viewheight(fixed_t wh)
{
	uint32_t i;
	for(i = 0; i < 4; i++)
		hook_list_viewheight[i].value = wh;
	for(; i < 6; i++)
		hook_list_viewheight[i].value = wh / 2;
	utils_install_hooks(hook_list_viewheight);
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
void mobj_kill_animation(mobj_t *mo)
{
	// custom death animations can be added

	if(mo->info->xdeathstate && mo->health < -mo->info->spawnhealth)
		P_SetMobjAnimation(mo, MOANIM_XDEATH);
	else
		P_SetMobjAnimation(mo, MOANIM_DEATH);

	if(mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}
}

__attribute((regparm(2),no_caller_saved_registers))
void mobj_spawn_init(mobj_t *mo, uint32_t type)
{
	// clear memory
	{
		uint32_t count = sizeof(mobj_t) / sizeof(uint32_t);
		uint32_t *ptr = (uint32_t*)mo;
		do
		{
			*ptr++ = 0;
		} while(--count);
	}
	// assign ID
	mo->id = mobj_id++;
	// copy new values
	mo->flags2 = mobjinfo[type].flags2;
}

__attribute((regparm(2),no_caller_saved_registers))
void mobj_player_init(player_t *pl)
{
	dextra_playerclass_t *pc = pl->class;
	mobj_t *mo = pl->mo;

	*displayplayer = *consoleplayer;

	if(pl->playerstate == PST_REBORN)
	{
		// major cleanup // TODO: keep kill/item/secret stats
		memset(pl, 0, sizeof(player_t));
		pl->mo = mo;

		// setup player class
		pc = &mobjinfo[player_class].extra->playerclass;
		pl->class = pc;

		// pre-setup
		pl->usedown = 1;
		pl->attackdown = 1;
		pl->health = pc->maxhealth;

		// reset inventory // TODO: major rewrite
		pl->weaponowned[0] = 1;
		pl->readyweapon = 0;
	}

	// initialize player
	pl->playerstate = PST_LIVE;
	pl->viewheight = pc->viewheight;
	set_player_viewheight(pc->viewheight);

	// initialize mobj
	// TODO: apply translation

	// fix player->mo to use correct player class
	{
		uint32_t type = pl->class->motype;
		mobjinfo_t *info = mobjinfo + type;
		state_t *st = states + info->spawnstate;

		mo->type = type;
		mo->info = info;
		mo->health = info->spawnhealth;
		mo->radius = info->radius;
		mo->height = info->height;
		mo->flags = info->flags;
		mo->flags2 = info->flags2;
		mo->state = st;
		mo->tics = st->tics;
		mo->sprite = st->sprite;
		mo->frame = st->frame;
	}

	// init weapon
	weapon_setup(pl);

	// extra stuff
	if(*deathmatch)
		for(uint32_t i = 0; i < NUMCARDS; i++)
			pl->cards[i] = 1; // TODO: rewrite
}

__attribute((regparm(2),no_caller_saved_registers))
void mobj_use_fail(mobj_t *mo)
{
	if(mo->info->activesound)
		S_StartSound(mo, mo->info->activesound);
}

__attribute((regparm(2),no_caller_saved_registers))
void player_land(mobj_t *mo)
{
	// fall damage can be added here

	if(mo->info->seesound)
		S_StartSound(mo, mo->info->seesound);	
}

