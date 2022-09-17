// kgsws' ACE Engine
////
// New thinker system, run before 'mobj' thinkers. The original is still active.
// This is (or will be) better for level geometry.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "animate.h"
#include "think.h"

thinker_t thcap;

//
// thinkers

static __attribute((regparm(2),no_caller_saved_registers))
void run_thinkers()
{
	thinker_t *th;

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
	// replace call to 'P_RunThinkers' in 'P_Ticker'
	{0x00032FD1, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)run_thinkers},
};

