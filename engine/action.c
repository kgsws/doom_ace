// kgsws' ACE Engine
////
// Action functions for DECORATE.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "dehacked.h"
#include "decorate.h"
#include "action.h"
#include "textpars.h"
#include "sound.h"

typedef struct
{
	uint8_t *name;
	void *func;
	uint8_t (*handler)();
} dec_action_t;

//

void *parse_action_func;
void *parse_action_arg;

static const dec_action_t mobj_action[];

//
// basic sounds

static __attribute((regparm(2),no_caller_saved_registers))
void A_Pain(mobj_t *mo)
{
	// TODO: boss check
	S_StartSound(mo, mo->info->painsound);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_Scream(mobj_t *mo)
{
	// TODO: boss check
	S_StartSound(mo, mo->info->deathsound);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_XScream(mobj_t *mo)
{
	// TODO: boss check
	S_StartSound(mo, 31);
}

//
// A_FaceTarget

static __attribute((regparm(2),no_caller_saved_registers))
void A_FaceTarget(mobj_t *mo)
{
	if(!mo->target)
		return;

	mo->flags0 &= ~MF_AMBUSH;
	mo->angle = R_PointToAngle2(mo->x, mo->y, mo->target->x, mo->target->y);

	// TODO: use correct flag
	if(mo->target->flags0 & MF_SHADOW)
		mo->angle += (P_Random() - P_Random()) << 21;
}

//
// A_NoBlocking

static __attribute((regparm(2),no_caller_saved_registers))
void A_NoBlocking(mobj_t *mo)
{
	mo->flags0 &= ~MF_SOLID;
	// TODO: drop items
}

//
// A_Look

static __attribute((regparm(2),no_caller_saved_registers))
void A_Look(mobj_t *mo)
{
	doom_A_Look(mo);
}

//
// A_Chase

static __attribute((regparm(2),no_caller_saved_registers))
void A_Chase(mobj_t *mo)
{
	// workaround for non-fixed speed
	fixed_t speed = mo->info->speed;

	mo->info->speed = (speed + (FRACUNIT / 2)) >> FRACBITS;

	doom_A_Chase(mo);

	mo->info->speed = speed;
}

//
// parser

uint8_t *action_parser(uint8_t *name)
{
	uint8_t *kw;
	const dec_action_t *act = mobj_action;

	// find action
	while(act->name)
	{
		if(!strcmp(act->name, name))
			break;
		act++;
	}
	if(!act->name)
		I_Error("[DECORATE] Unknown action '%s' in '%s'!", name, parse_actor_name);

	// action function
	parse_action_func = act->func;

	// TODO: args & handler
	kw = tp_get_keyword();
	if(!kw)
		return NULL;

	return kw;
}

//
// table of actions

static const dec_action_t mobj_action[] =
{
	// basic sounds
	{"a_pain", A_Pain, NULL},
	{"a_scream", A_Scream, NULL},
	{"a_xscream", A_XScream, NULL},
	// basic control
	{"a_facetarget", A_FaceTarget, NULL},
	{"a_noblocking", A_NoBlocking, NULL},
	// "AI"
	{"a_look", A_Look, NULL},
	{"a_chase", A_Chase, NULL},
	// terminator
	{NULL}
};

