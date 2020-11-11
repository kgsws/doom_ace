// kgsws' Doom ACE
// Custom code pointers.
#include "engine.h"
#include "defs.h"
#include "textpars.h"
#include "action.h"
#include "decorate.h"

#define ARG_PARSE_DEBUG

enum
{
	ARGTYPE_TERMINATOR,
	ARGTYPE_BOOLEAN,
	ARGTYPE_INT32,
	ARGTYPE_FIXED,
	ARGTYPE_ACTOR,
	ARGTYPE_FLAGLIST,
	//
	ARGFLAG_OPTIONAL
};

typedef struct
{
	uint8_t *match;
	uint32_t value;
} argflag_t;

typedef struct
{
	uint8_t type;
	uint8_t flags;
	argflag_t *flaglist;
	uint32_t def;
	uint32_t result;
} argtype_t;

//
//

static uint8_t kw_true[] = {'t', 'r', 'u', 'e', 0};
static uint8_t kw_false[] = {'f', 'a', 'l', 's', 'e', 0};

// args: optional boolean, default true
static argtype_t arg_boolean_opt_true[] =
{
	{ARGTYPE_BOOLEAN, ARGFLAG_OPTIONAL, NULL, 1},
	// terminator
	{ARGTYPE_TERMINATOR}
};

//
//

#ifdef ARG_PARSE_DEBUG
static void arg_dump(argtype_t *list)
{
	uint32_t idx = 0;
	while(list->type != ARGTYPE_TERMINATOR)
	{
		doom_printf("arg %u: value %u / %d / 0x%08X\n", idx++, list->result, list->result, list->result);
		list++;
	}
}
#endif

static int parse_args(argtype_t *list, uint8_t *arg, uint8_t *end)
{
	uint8_t *tmp;

	if(!arg)
	{
		if(list->flags & ARGFLAG_OPTIONAL)
		{
set_defaults:
			while(list->type != ARGTYPE_TERMINATOR)
			{
				list->result = list->def;
				list++;
			}
			return 0;
		}
		return 1;
	}

	tp_kw_is_func = 1;

	while(list->type != ARGTYPE_TERMINATOR)
	{
		// skip WS
		arg = tp_skip_wsc(arg, end);
		if(arg == end)
			goto end_eof;

		// parse
		switch(list->type)
		{
			case ARGTYPE_BOOLEAN:
				tmp = tp_ncompare_skip(arg, end, kw_true);
				if(!tmp)
				{
					arg = tp_ncompare_skip(arg, end, kw_false);
					if(!arg)
					{
						tp_kw_is_func = 0;
						return 1;
					}
					list->result = 0;
				} else
				{
					list->result = 1;
					arg = tmp;
				}
			break;
		}

		// skip WS
		arg = tp_skip_wsc(arg, end);
		if(arg == end)
			goto end_eof;

		list++;

		if(*arg != ',')
		{
			if(list->type != ARGTYPE_TERMINATOR)
				goto end_eof;
		} else
		{
			if(list->type == ARGTYPE_TERMINATOR)
				return 1;
		}
	}

	tp_kw_is_func = 0;

	return 0;

end_eof:
	tp_kw_is_func = 0;

	if(list->flags & ARGFLAG_OPTIONAL)
		goto set_defaults;

	return 1;
}

void *arg_NoBlocking(void *func, uint8_t *arg, uint8_t *end)
{
	if(parse_args(arg_boolean_opt_true, arg, end))
		return NULL;
	if(!arg_boolean_opt_true[0].result)
		func_extra_data = (void*)1;
	return A_NoBlocking;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_NoBlocking(mobj_t *mo)
{
	arg_droplist_t *drop;
	mobj_t *item;

	// make non-solid
	mo->flags &= ~MF_SOLID;

	// drop items
	drop = mo->state->extra;
	if(!drop)
		return;

	for(uint32_t i = 0; i < drop->count+1; i++)
	{
		uint32_t tc = drop->typecombo[i];
		if(P_Random() > (tc >> 24))
			continue;
		item = P_SpawnMobj(mo->x, mo->y, mo->z + (8 << FRACBITS), tc & 0xFFFFFF);
		item->flags |= MF_DROPPED;
		item->angle = P_Random() << 24;
		item->momx = 2*FRACUNIT - (P_Random() << 9);
		item->momy = 2*FRACUNIT - (P_Random() << 9);
		item->momz = (3 << 15) + 2*(P_Random() << 9);
	}
}

