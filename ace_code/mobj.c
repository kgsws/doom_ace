// kgsws' Doom ACE
// MOBJ stuff.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "mobj.h"
#include "decorate.h"

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

// this will find state slot for requested animation ID
__attribute((regparm(2),no_caller_saved_registers))
uint32_t *P_GetAnimPtr(uint8_t anim, mobjinfo_t *info)
{
	static uint32_t zero_ptr;

	if(anim <= MOANIM_RAISE)
		return (void*)info + anim_state_base[anim];

	// TODO: extra states
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
doom_printf("set anim %u = state %u\n", anim, state);
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
doom_printf("new anim %u for %p\n", mo->animation, mo);
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
	// copy new values
	mo->flags2 = mobjinfo[type].flags2;
}

