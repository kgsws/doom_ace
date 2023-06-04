// kgsws' ACE Engine
////
// New thinker system, runs before 'mobj' thinkers. The original is still active.
// This is better for level geometry.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "vesa.h"
#include "draw.h"
#include "saveload.h"
#include "animate.h"
#include "terrain.h"
#include "player.h"
#include "special.h"
#include "map.h"
#include "think.h"

thinker_t thcap;

uint_fast8_t think_freeze_mode;

//
// ticker

static __attribute((regparm(2),no_caller_saved_registers))
void P_Ticker()
{
	thinker_t *th;

	if(!demorecording || (!paused && !(menuactive && !netgame)))
	{
		// player data transfers - even when paused
		for(uint32_t i = 0; i < MAXPLAYERS; i++)
		{
			if(!playeringame[i])
				continue;
			player_chat_char(i);
		}
	}

	if(paused)
		return;

	if(!netgame && menuactive && !is_title_map && !demoplayback && leveltime)
		return;

	// player thinkers
	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		if(!playeringame[i])
			continue;
		player_think(i);
	}

	if(think_freeze_mode)
	{
		// only players
		for(uint32_t i = 0; i < MAXPLAYERS; i++)
		{
			player_t *pl;

			if(!playeringame[i])
				continue;

			pl = players + i;

			if(!pl->mo)
				continue;

			P_MobjThinker(pl->mo);
		}

		// unlike in ZDoom, do not run anything else
		return;
	}

	// animations
	animate_step();

	// run new thinkers
	th = thcap.next;
	while(th != &thcap)
	{
		if(th->function == (void*)-1) // keep same hack
		{
			th->next->prev = th->prev;
			th->prev->next = th->next;
			Z_Free(th);
		} else
		if(th->func)
			th->func(th);
		th = th->next;
	}

	// run original after
	P_RunThinkers();

	// terrain splash sound
	terrain_sound();

	// next tic
	leveltime++;

	// autosave from line action
	if(spec_autosave)
	{
		if(gameaction == ga_nothing)
		{
			int32_t lump = W_CheckNumForName("WIAUTOSV");
			if(lump >= 0)
			{
				vesa_copy();
				V_DrawPatchDirect(0, 0, W_CacheLumpNum(lump, PU_CACHE));
				vesa_update();
			}
			save_auto(0);
		}
		spec_autosave = 0;
	}
}

//
// API

void think_clear()
{
	thcap.prev = &thcap;
	thcap.next = &thcap;
}

void think_add(thinker_t *th)
{
	thcap.prev->next = th;
	th->next = &thcap;
	th->prev = thcap.prev;
	thcap.prev = th;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'P_Ticker' in 'G_Ticker'
	{0x0002081A, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_Ticker},
};

