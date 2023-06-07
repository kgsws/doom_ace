// kgsws' ACE Engine
////
// Action functions for DECORATE.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "dehacked.h"
#include "decorate.h"
#include "action.h"
#include "wadfile.h"
#include "mobj.h"
#include "player.h"
#include "weapon.h"
#include "hitscan.h"
#include "inventory.h"
#include "terrain.h"
#include "sound.h"
#include "stbar.h"
#include "font.h"
#include "map.h"
#include "special.h"
#include "demo.h"
#include "sight.h"
#include "render.h"
#include "config.h"
#include "textpars.h"

#include "act_const.h"

#define MAKE_CONST(x)	{.name = #x, .value = (x)}

#define MATHFRAC	10 // signed 22.10

#define ARG_OFFS_MASK	0x07FF
#define ARG_MAGIC_MASK	0x7FFF
#define ARG_TYPE_MASK	0xF800
#define ARG_DEF_MAGIC	0x8000 // processed string
#define ARG_DEF_VALUE	0x0000 // this is MATH_VALUE << 11
#define ARG_DEF_RANDOM	0x0800 // this is MATH_RANDOM << 11
#define ARG_DEF_MATH	0x7000
#define ARG_DEF_VAR	0x7800

#define MAX_STR_ARG	3 // currently 3 string arguments is max

#define STRARG(a,t)	(((a) << 4) | (t))

enum
{
	AT_U8,
	AT_U16,
	AT_S16,
	AT_S32,
	AT_FIXED,
	AT_ANGLE,
	AT_ALPHA,
};

enum
{
	// 0b00xxxxxx // operators
	MATH_LOR,
	MATH_LAND,

	MATH_OR,
	MATH_XOR,
	MATH_AND,

	MATH_LESS,
	MATH_LEQ,
	MATH_GREAT,
	MATH_GEQ,

	MATH_EQ,
	MATH_NEQ,

	MATH_ADD,
	MATH_SUB,

	MATH_MUL,
	MATH_DIV,
	MATH_MOD,

	MATH_NEG,

	MATH_TERMINATOR, // fake

	// unrecognized
	MATH_INVALID = 0x6F,

	// check for number is 'type >= MATH_VALUE'

	MATH_VALUE = 0x70,
	MATH_RANDOM,

	// 0b1xxxxxxx // variables
	// variable index is 'type & MATH_VAR_MASK'

	MATH_VARIABLE = 0x80,
	MATH_VAR_MASK = 0x7F
};

enum
{
	STR_NONE,
	STR_NORMAL,
	STR_STATE,
	//
	STR_MOBJ_TYPE,
	STR_MOBJ_FLAG,
	STR_DAMAGE_TYPE,
	STR_TRANSLATION,
	STR_SOUND,
	STR_LUMP,
};

typedef struct
{
	uint16_t value;
	uint16_t lines;
	uint8_t text[];
} act_str_t;

typedef struct
{
	uint32_t next;
	uint16_t extra;
} act_state_t;

typedef struct
{
	uint32_t bits;
	uint16_t offset;
} act_moflag_t;

typedef struct
{
	uint8_t buff[128];
	uint8_t *ptr;
	uint32_t count;
} math_stack_t;

typedef struct
{
	uint8_t *name;
	uint16_t offset;
	uint8_t type;
	uint8_t source;
} math_var_t;

typedef struct
{
	uint8_t *name;
	int32_t value;
} math_con_t;

typedef struct
{
	uint16_t count;
	uint16_t offs_type;
} act_arg_t;

typedef struct
{
	const uint8_t *name;
	void *func;
	uint8_t maxarg;
	int8_t minarg;
	uint8_t strarg[MAX_STR_ARG];
} dec_action_t;

typedef struct
{
	uint8_t special; // so far u8 is enough but ZDoom has types > 255
	uint32_t alias;
} __attribute__((packed)) dec_linespec_t; // 'packed' for DOS use only

//

void *parse_action_func;
void *parse_action_arg;

static const uint8_t *action_name;
static const dec_action_t *action_parsed;

static int_fast8_t parse_arg_idx;
static int32_t parse_math_res;

uint32_t act_cc_tick;

static uint32_t bombflags;
static fixed_t bombdist;
static fixed_t bombfist;
static int_fast8_t bombdmgtype = -1;

static mobj_t *enemy_look_pick;
static mobj_t *enemy_looker;

static const dec_action_t mobj_action[];
static const dec_linespec_t special_action[];

// line special action
static void A_LineSpecial(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
static const dec_action_t line_action =
{
	.func = A_LineSpecial,
	.maxarg = 5, // 5 args
	.minarg = 0, // this is different than in ZDoom
};

static uint8_t math_op[] =
{
	[MATH_LOR] = 1,
	[MATH_LAND] = 2,
	[MATH_OR] = 3,
	[MATH_XOR] = 4,
	[MATH_AND] = 5,
	[MATH_EQ] = 6,
	[MATH_NEQ] = 6,
	[MATH_LESS] = 7,
	[MATH_LEQ] = 7,
	[MATH_GREAT] = 7,
	[MATH_GEQ] = 7,
	[MATH_ADD] = 8,
	[MATH_SUB] = 8,
	[MATH_MUL] = 9,
	[MATH_DIV] = 9,
	[MATH_MOD] = 9,
	[MATH_NEG] = 100,
	// terminator has to force calculation of stack contents
	// lowest priority is required
	[MATH_TERMINATOR] = 0
};

static const math_var_t math_variable[] =
{
	{"X", offsetof(mobj_t, x), AT_FIXED},
	{"Y", offsetof(mobj_t, y), AT_FIXED},
	{"Z", offsetof(mobj_t, z), AT_FIXED},
	{"ANGLE", offsetof(mobj_t, angle), AT_ANGLE},
	{"CEILINGZ", offsetof(mobj_t, ceilingz), AT_FIXED},
	{"FLOORZ", offsetof(mobj_t, floorz), AT_FIXED},
	{"PITCH", offsetof(mobj_t, pitch), AT_ANGLE},
	{"VELX", offsetof(mobj_t, momx), AT_FIXED},
	{"VELY", offsetof(mobj_t, momy), AT_FIXED},
	{"VELZ", offsetof(mobj_t, momz), AT_FIXED},
	{"ALPHA", offsetof(mobj_t, render_alpha), AT_ALPHA},
	{"ARGS[0]", offsetof(mobj_t, special.arg[0]), AT_S16},
	{"ARGS[1]", offsetof(mobj_t, special.arg[1]), AT_S16},
	{"ARGS[2]", offsetof(mobj_t, special.arg[2]), AT_S16},
	{"ARGS[3]", offsetof(mobj_t, special.arg[3]), AT_S16},
	{"ARGS[4]", offsetof(mobj_t, special.arg[4]), AT_S16},
	{"HEALTH", offsetof(mobj_t, health), AT_S32},
	{"HEIGHT", offsetof(mobj_t, height), AT_FIXED},
	{"RADIUS", offsetof(mobj_t, radius), AT_FIXED},
	{"REACTIONTIME", offsetof(mobj_t, reactiontime), AT_S32},
	{"SCALEX", offsetof(mobj_t, scale), AT_FIXED},
	{"SCALEY", offsetof(mobj_t, scale), AT_FIXED},
	{"SPECIAL", offsetof(mobj_t, special.special), AT_U8},
	{"TID", offsetof(mobj_t, special.tid), AT_U16},
	{"THRESHOLD", offsetof(mobj_t, threshold), AT_S32},
	{"WATERLEVEL", offsetof(mobj_t, waterlevel), AT_U8},
	//
	{"MASS", offsetof(mobjinfo_t, mass), AT_FIXED, 1},
	{"SPEED", offsetof(mobjinfo_t, speed), AT_FIXED, 1},
	// terminator
	{NULL}
};

static const math_con_t math_constant[] =
{
	{"TRUE", 1},
	{"FALSE", 0},
	{"STYLE_NORMAL", RS_NORMAL},
	{"STYLE_FUZZY", RS_FUZZ},
	{"STYLE_SHADOW", RS_SHADOW},
	{"STYLE_TRANSLUCENT", RS_TRANSLUCENT},
	{"STYLE_ADD", RS_ADDITIVE},
	{"STYLE_NONE", RS_INVISIBLE},
	MAKE_CONST(GFF_NOEXTCHANGE),
	MAKE_CONST(CHAN_BODY),
	MAKE_CONST(CHAN_WEAPON),
	MAKE_CONST(CHAN_VOICE),
	MAKE_CONST(ATTN_NORM),
	MAKE_CONST(ATTN_NONE),
	MAKE_CONST(VAF_DMGTYPEAPPLYTODIRECT),
	MAKE_CONST(WRF_NOBOB),
	MAKE_CONST(WRF_NOFIRE),
	MAKE_CONST(WRF_NOSWITCH),
	MAKE_CONST(WRF_DISABLESWITCH),
	MAKE_CONST(WRF_NOPRIMARY),
	MAKE_CONST(WRF_NOSECONDARY),
	MAKE_CONST(CMF_AIMOFFSET),
	MAKE_CONST(CMF_AIMDIRECTION),
	MAKE_CONST(CMF_TRACKOWNER),
	MAKE_CONST(CMF_CHECKTARGETDEAD),
	MAKE_CONST(CMF_ABSOLUTEPITCH),
	MAKE_CONST(CMF_OFFSETPITCH),
	MAKE_CONST(CMF_SAVEPITCH),
	MAKE_CONST(CMF_ABSOLUTEANGLE),
	MAKE_CONST(FPF_AIMATANGLE),
	MAKE_CONST(FPF_TRANSFERTRANSLATION),
	MAKE_CONST(FPF_NOAUTOAIM),
	MAKE_CONST(FBF_USEAMMO),
	MAKE_CONST(FBF_NOFLASH),
	MAKE_CONST(FBF_NORANDOM),
	MAKE_CONST(FBF_NORANDOMPUFFZ),
	MAKE_CONST(CBAF_AIMFACING),
	MAKE_CONST(CBAF_NORANDOM),
	MAKE_CONST(CBAF_NORANDOMPUFFZ),
	MAKE_CONST(CPF_USEAMMO),
	MAKE_CONST(CPF_PULLIN),
	MAKE_CONST(CPF_NORANDOMPUFFZ),
	MAKE_CONST(CPF_NOTURN),
	MAKE_CONST(SMF_LOOK),
	MAKE_CONST(SMF_PRECISE),
	MAKE_CONST(SXF_TRANSFERTRANSLATION),
	MAKE_CONST(SXF_ABSOLUTEPOSITION),
	MAKE_CONST(SXF_ABSOLUTEANGLE),
	MAKE_CONST(SXF_ABSOLUTEVELOCITY),
	MAKE_CONST(SXF_SETMASTER),
	MAKE_CONST(SXF_NOCHECKPOSITION),
	MAKE_CONST(SXF_TELEFRAG),
	MAKE_CONST(SXF_TRANSFERAMBUSHFLAG),
	MAKE_CONST(SXF_TRANSFERPITCH),
	MAKE_CONST(SXF_TRANSFERPOINTERS),
	MAKE_CONST(SXF_USEBLOODCOLOR),
	MAKE_CONST(SXF_CLEARCALLERTID),
	MAKE_CONST(SXF_SETTARGET),
	MAKE_CONST(SXF_SETTRACER),
	MAKE_CONST(SXF_NOPOINTERS),
	MAKE_CONST(SXF_ORIGINATOR),
	MAKE_CONST(SXF_ISTARGET),
	MAKE_CONST(SXF_ISMASTER),
	MAKE_CONST(SXF_ISTRACER),
	MAKE_CONST(SXF_MULTIPLYSPEED),
	MAKE_CONST(CVF_RELATIVE),
	MAKE_CONST(CVF_REPLACE),
	MAKE_CONST(WARPF_ABSOLUTEOFFSET),
	MAKE_CONST(WARPF_ABSOLUTEANGLE),
	MAKE_CONST(WARPF_ABSOLUTEPOSITION),
	MAKE_CONST(WARPF_USECALLERANGLE),
	MAKE_CONST(WARPF_NOCHECKPOSITION),
	MAKE_CONST(WARPF_STOP),
	MAKE_CONST(WARPF_MOVEPTR),
	MAKE_CONST(WARPF_COPYVELOCITY),
	MAKE_CONST(WARPF_COPYPITCH),
	MAKE_CONST(XF_HURTSOURCE),
	MAKE_CONST(XF_NOTMISSILE),
	MAKE_CONST(XF_THRUSTZ),
	MAKE_CONST(XF_NOSPLASH),
	MAKE_CONST(PTROP_NOSAFEGUARDS),
	MAKE_CONST(AAPTR_DEFAULT),
	MAKE_CONST(AAPTR_TARGET),
	MAKE_CONST(AAPTR_TRACER),
	MAKE_CONST(AAPTR_MASTER),
	MAKE_CONST(AAPTR_PLAYER1),
	MAKE_CONST(AAPTR_PLAYER2),
	MAKE_CONST(AAPTR_PLAYER3),
	MAKE_CONST(AAPTR_PLAYER4),
	MAKE_CONST(AAPTR_NULL),
	// terminator
	{NULL}
};

//
// stuff

const dec_flag_t *find_mobj_flag(uint8_t *name, const dec_flag_t *flag)
{
	while(flag->name)
	{
		if(!strcmp(flag->name, name))
			return flag;
		flag++;
	}

	return NULL;
}

//
// math handling

static void stack_push(math_stack_t *ms, uint32_t type, int32_t value)
{
	if(ms->ptr >= ms->buff + sizeof(ms->buff))
		engine_error("DECORATE", "Stack overflow in equation!");

#if 0
	if(type >= MATH_VALUE && !(type & MATH_VARIABLE))
		doom_printf(" push[%u] value %d\n", ms->count, value);
	else
		doom_printf(" push[%u] operator %u\n", ms->count, type);
#endif

	if(type >= MATH_VALUE && !(type & MATH_VARIABLE))
	{
		if(ms->ptr + 4 >= ms->buff + sizeof(ms->buff))
			engine_error("DECORATE", "Stack overflow in equation!");
		// add value first
		*((int32_t*)ms->ptr) = value;
		ms->ptr += sizeof(int32_t);
	}

	// add type
	*ms->ptr++ = type;

	// update counter
	ms->count++;
}

static uint32_t stack_pop(math_stack_t *ms, int32_t *value)
{
	// TODO: check underflow?
	uint32_t type;

	// read type
	type = *--ms->ptr;

	if(type >= MATH_VALUE && !(type & MATH_VARIABLE))
	{
		// also read value
		ms->ptr -= sizeof(int32_t);
		*value = *((int32_t*)ms->ptr);
	}

	// update counter
	ms->count--;

#if 0
	if(type >= MATH_VALUE && !(type & MATH_VARIABLE))
		doom_printf(" pop[%u] value %d\n", ms->count, *value);
	else
		doom_printf(" pop[%u] operator %u\n", ms->count, type);
#endif

	// done
	return type;
}

static int32_t calculate_value(int32_t v0, int32_t v1, uint32_t op)
{
//	doom_printf("math: %d and %d; op %u\n", v0, v1, op);

	switch(op)
	{
		case MATH_LOR:
			return (v0 || v1) << FRACBITS;
		case MATH_LAND:
			return (v0 && v1) << FRACBITS;
		case MATH_OR:
			return v0 | v1;
		case MATH_XOR:
			return v0 ^ v1;
		case MATH_AND:
			return v0 & v1;
		case MATH_LESS:
			return (v0 < v1) << FRACBITS;
		case MATH_LEQ:
			return (v0 <= v1) << FRACBITS;
		case MATH_GREAT:
			return (v0 > v1) << FRACBITS;
		case MATH_GEQ:
			return (v0 >= v1) << FRACBITS;
		case MATH_EQ:
			return (v0 == v1) << FRACBITS;
		case MATH_NEQ:
			return (v0 != v1) << FRACBITS;
		case MATH_ADD:
			return v0 + v1;
		case MATH_SUB:
		case MATH_NEG:
			return v0 - v1;
		case MATH_MOD:
			return ((v0 >> MATHFRAC) % (v1 >> MATHFRAC)) << MATHFRAC;
		case MATH_MUL:
			return ((int64_t)v0 * (int64_t)v1) >> MATHFRAC;
		case MATH_DIV:
//			return ((int64_t)v0 << MATHFRAC) / v1;
		{
			union
			{
				int64_t w;
				struct
				{
					uint32_t a, b;
				};
			} num;
			int32_t res;

			num.w = v0;
			num.w <<= MATHFRAC;

			if(abs(v1) <= abs(num.w >> 31))
				return (v1 ^ v0) & 0x80000000 ? 0x80000000 : 0x7FFFFFFF;

			asm(	"idiv %%ecx"
				: "=a" (res)
				: "a" (num.a), "d" (num.b), "c" (v1)
				: "cc");

			return res;
		}
	}

	return 0;
}

static int32_t resolve_type(mobj_t *mo, uint32_t type, uint32_t value)
{
	const math_var_t *var;
	union
	{
		void *ptr;
		uint8_t *u8;
		uint16_t *u16;
		int16_t *s16;
		int32_t *s32;
		fixed_t *fixed;
		angle_t *angle;
	} base;

	if(!mo)
	{
		// this is preprocessing stage
		if(parse_math_res != MATH_INVALID)
		{
			if(type & MATH_VARIABLE)
			{
				parse_math_res = type;
				return 1;
			} else
			if(type == MATH_RANDOM)
			{
				parse_math_res = MATH_RANDOM;
				// value contains random range
				// this must be returned
				return value;
			}
		}
		// return something
		return 1;
	}

	if(type == MATH_RANDOM)
	{
		uint32_t diff, rng;
		uint32_t v0, v1;

		v1 = value & 0xFFFF;
		v0 = (value >> 16) & 0xFFFF;

		if(v0 == v1)
			return v0 << MATHFRAC;

		if(v0 > v1)
		{
			diff = v0;
			v0 = v1;
			v1 = diff;
		}

		diff = v1 - v0;

		rng = P_Random();
		if(diff > 255)
			rng |= P_Random() << 8;

		v0 += rng % (diff + 1);

		return v0 << MATHFRAC;
	}

	type &= MATH_VAR_MASK;

	var = math_variable + type;

	if(var->source)
		base.ptr = mo->info;
	else
		base.ptr = mo;

	base.ptr += var->offset;

	switch(var->type)
	{
		case AT_U8:
			return *base.u8 << MATHFRAC;
		case AT_U16:
			return *base.u16 << MATHFRAC;
		case AT_S16:
			return *base.s16 << MATHFRAC;
		case AT_S32:
			return *base.s32 << MATHFRAC;
		case AT_FIXED:
			return *base.fixed >> (FRACBITS - MATHFRAC);
		case AT_ANGLE:
			return *base.angle / ((4096 << MATHFRAC) / 360);
		case AT_ALPHA:
		{
			fixed_t tmp = *base.u8 * 257;
			if(tmp == 0xFFFF)
				return FRACUNIT;
			return tmp;
		}
	}

	return 0;
}

static int32_t math_calculate(mobj_t *mo, uint8_t *buff, uint32_t size)
{
	math_stack_t stack;
	uint8_t *end = buff + size;

	stack.ptr = stack.buff;
	stack.count = 0;

	while(1)
	{
		uint32_t type;
		int32_t value;

		if(buff < end)
		{
			type = *buff++;
			if(type >= MATH_VALUE && !(type & MATH_VARIABLE))
			{
				value = *((int32_t*)buff);
				buff += sizeof(int32_t);
			}
		} else
			// terminate the equation
			type = MATH_TERMINATOR;

//		doom_printf("arg type %u value %d\n", type, value);

		if(type < MATH_VALUE)
		{
			// math operator
			if(stack.count >= 3)
			{
				uint32_t last_type[3];
				int32_t last_value[3];
				uint8_t *sp = stack.ptr;

				// this should be value
				last_type[0] = stack_pop(&stack, last_value + 0);
				if(last_type[0] < MATH_VALUE)
					goto math_fail;

				// this should be operator
				last_type[1] = stack_pop(&stack, last_value + 1);
				if(last_type[1] >= MATH_VALUE)
					goto math_fail;

				// check priority
				if(math_op[last_type[1]] >= math_op[type])
				{
					// higher or same priority; process now

					// this should be value
					last_type[2] = stack_pop(&stack, last_value + 2);
					if(last_type[2] < MATH_VALUE)
						goto math_fail;

					// process variables
					if(last_type[2] != MATH_VALUE)
						last_value[2] = resolve_type(mo, last_type[2], last_value[2]);
					if(last_type[0] != MATH_VALUE)
						last_value[0] = resolve_type(mo, last_type[0], last_value[0]);

					// for preprocessing stage
					if(!mo && last_type[2] != MATH_VALUE || last_type[0] != MATH_VALUE)
						// it's no longer possible to avoid math equation
						parse_math_res = MATH_INVALID;

					// do the operation
					value = calculate_value(last_value[2], last_value[0], last_type[1]);

					// push the result
					stack_push(&stack, MATH_VALUE, value);
				} else
				{
					// lower priority; push back
					stack.ptr = sp;
					stack.count += 2;
				}
			}

			// teminator?
			if(type == MATH_TERMINATOR)
			{
				if(stack.count <= 1)
					break;
				continue;
			}
		}

		// push value or operator
		stack_push(&stack, type, value);
	}

	if(stack.count == 1)
	{
		uint32_t last_type;
		int32_t last_value;

		// this should be value
		last_type = stack_pop(&stack, &last_value);
		if(last_type < MATH_VALUE)
			goto math_fail;

		// process variables
		if(last_type != MATH_VALUE)
			last_value = resolve_type(mo, last_type, last_value);

		return last_value;
	}

math_fail:
	parse_math_res = -1;
	return 0;
}

//
// argument handlers

int32_t actarg_raw(mobj_t *mo, void *data, uint32_t arg, int32_t def)
{
	uint16_t *argoffs;
	uint16_t type, offs;

	if(!data)
		return def;

	argoffs = data;

	if(arg >= *argoffs)
		return def;

	type = argoffs[1 + arg];
	offs = type & ARG_OFFS_MASK;

	switch(type & ARG_TYPE_MASK)
	{
		case ARG_DEF_VAR:
			return resolve_type(mo, MATH_VARIABLE | offs, 0);
		case ARG_DEF_MATH:
			argoffs = data + offs;
			return math_calculate(mo, (uint8_t*)(argoffs+1), *argoffs);
		case ARG_DEF_VALUE:
			return *((int32_t*)(data + offs));
		case ARG_DEF_RANDOM:
			return resolve_type(mo, MATH_RANDOM, *((int32_t*)(data + offs)));
	}

	return def;
}

fixed_t actarg_fixed(mobj_t *mo, void *data, uint32_t arg, fixed_t def)
{
	return actarg_raw(mo, data, arg, def >> (FRACBITS - MATHFRAC)) << (FRACBITS - MATHFRAC);
}

int32_t actarg_integer(mobj_t *mo, void *data, uint32_t arg, int32_t def)
{
	return actarg_raw(mo, data, arg, def << MATHFRAC) >> MATHFRAC;
}

angle_t actarg_angle(mobj_t *mo, void *data, uint32_t arg)
{
	return actarg_raw(mo, data, arg, 0) * ((4096 << MATHFRAC) / 360);
}

static mobj_t *actarg_pointer(mobj_t *mo, void *data, uint32_t arg, uint32_t def)
{
	def = actarg_raw(mo, data, arg, def);

	switch(def)
	{
		case AAPTR_DEFAULT:
			return mo;
		case AAPTR_TARGET:
			return mo->target;
		case AAPTR_TRACER:
			return mo->tracer;
		case AAPTR_MASTER:
			return mo->master;
		case AAPTR_PLAYER1:
			return players[0].mo;
		case AAPTR_PLAYER2:
			return players[1].mo;
		case AAPTR_PLAYER3:
			return players[2].mo;
		case AAPTR_PLAYER4:
			return players[3].mo;
		default:
			return NULL;
	}
}

static uint32_t actarg_magic(mobj_t *mo, void *data, uint32_t arg, uint32_t def)
{
	uint16_t *argoffs;
	uint16_t type, offs;

	if(!data)
		return def;

	argoffs = data;

	if(arg >= *argoffs)
		return def;

	return argoffs[1 + arg] & ARG_MAGIC_MASK;
}

static void *actarg_string(mobj_t *mo, void *data, uint32_t arg, uint32_t *lines)
{
	act_str_t *as;
	uint16_t *argoffs;
	uint16_t type, offs;

	if(!data)
		return NULL;

	argoffs = data;

	if(arg >= *argoffs)
		return NULL;

	as = data + (argoffs[1 + arg] & ARG_MAGIC_MASK);

	if(lines)
		*lines = as->lines;

	return as->text;
}

static uint32_t actarg_moflag(mobj_t *mo, void *data, uint32_t arg, uint32_t *bits)
{
	act_moflag_t *arf;
	uint16_t *argoffs;

	if(!data)
		return 0;

	argoffs = data;

	if(arg >= *argoffs)
		return 0;

	arf = data + (argoffs[1 + arg] & ARG_OFFS_MASK);
	*bits = arf->bits;
	return arf->offset;
}

static uint32_t actarg_state(mobj_t *mo, void *data, uint32_t arg, uint32_t *extra)
{
	act_state_t *st;
	uint16_t *argoffs;
	uint16_t type, offs;

	if(!data)
	{
		*extra = 0;
		return 0;
	}

	argoffs = data;

	if(arg >= *argoffs)
	{
		*extra = 0;
		return 0;
	}

	argoffs += 1 + arg;

	type = *argoffs;
	offs = type & ARG_OFFS_MASK;
	type &= ARG_TYPE_MASK;

	if(type != ARG_DEF_MAGIC)
	{
		*extra = STATE_SET_OFFSET;
		return actarg_integer(mo, data, arg, 0);
	}

	st = data + offs;

	*extra = st->extra;
	return st->next;
}

//
// iterators

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t PIT_Explode(mobj_t *thing)
{
	fixed_t dx, dy, dz;
	fixed_t dist;
	uint32_t damage;

	if(!(thing->flags & MF_SHOOTABLE))
		return 1;

	if(thing->flags1 & MF1_NORADIUSDMG && !(bombspot->flags2 & MF2_FORCERADIUSDMG))
		return 1;

	if(thing == bombsource)
	{
		if(!(bombflags & XF_HURTSOURCE))
			return 1;
		if(	thing->player &&
			bombspot->flags1 & MF1_SPECTRAL
		)	// well ... this is obscure
			// and yet it does not match ZDoom
			// in ZDoom, rocket jump is still applied
			return 1;
	}

	dx = abs(thing->x - bombspot->x);
	dy = abs(thing->y - bombspot->y);

	dist = dx > dy ? dx : dy;

	dz = thing->z + thing->height;

	if(	!(thing->flags2 & MF2_OLDRADIUSDMG) &&
		(!bombspot || !(bombspot->flags2 & MF2_OLDRADIUSDMG)) &&
		(bombspot->z < thing->z || bombspot->z >= dz)
	){
		if(bombspot->z > thing->z)
			dz = bombspot->z - dz;
		else
			dz = thing->z - bombspot->z;

		if(dz > dist)
			dist = dz;
	}

	if(dist > bombdist)
		return 1;

	if(!P_CheckSight(bombspot, thing))
		return 1;

	dist -= thing->radius;
	if(dist < 0)
		dist = 0;

	if(dist)
	{
		if(bombfist)
		{
			if(dist > bombfist)
			{
				dist -= bombfist;
				dist = FixedDiv(dist, (bombdist - bombfist));
			} else
				dist = 0;
		} else
			dist = FixedDiv(dist, bombdist);

		dist = bombdamage - ((dist * bombdamage) >> FRACBITS);
	} else
		dist = bombdamage;

	damage = dist;

	if(bombdmgtype >= 0)
		damage |= DAMAGE_SET_TYPE(bombdmgtype);

	mobj_damage(thing, bombspot, bombsource, damage, NULL);

	if(!(bombflags & XF_THRUSTZ))
		return 1;

	if(thing->flags1 & MF1_DONTTHRUST)
		return 1;

	if(bombspot->flags2 & MF2_FORCERADIUSDMG)
		return 1;

	dist = (dist * 50) / thing->info->mass;
	dz = (thing->z + thing->height / 2) - bombspot->z;
	dz = FixedMul(dz, dist * 655);

	if(thing == bombsource)
		dz = FixedMul(dz, 0xCB00);
	else
		dz /= 2;

	thing->momz += dz;

	return 1;
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t PIT_VileCheck(mobj_t *thing)
{
	int32_t maxdist;
	uint32_t ret;
	uint32_t flags;

	if(!(thing->flags & MF_CORPSE))
		return 1;

	if(thing->tics != -1 && !(thing->frame & FF_CANRAISE))
		return 1;

	if(!thing->info->state_raise)
		return 1;

	maxdist = thing->info->radius + vileobj->radius;

	if(abs(thing->x - viletryx) > maxdist || abs(thing->y - viletryy) > maxdist)
		return 1;

	corpsehit = thing;

	flags = thing->flags;

	thing->momx = 0;
	thing->momy = 0;

	thing->height <<= 2;
	thing->flags |= MF_SOLID;
	ret = P_CheckPosition(thing, thing->x, thing->y);
	thing->flags = flags;
	thing->height >>= 2;

	if(!ret)
		return 1;

	return 0;
}

//
// enemy search

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t PIT_EnemySearch(mobj_t *thing)
{
	if(thing == enemy_looker)
		return 1;

	if(enemy_looker->flags1 & MF1_SEEKERMISSILE)
	{
		if(thing == enemy_looker->target)
			return 1;
		if(thing->flags2 & MF2_CANTSEEK)
			return 1;
	}

	if(!(thing->flags & MF_SHOOTABLE))
		return 1;

	if(	thing->flags1 & MF1_ISMONSTER ||
		(thing->player && deathmatch)
	){
		enemy_look_pick = thing;
		return 0;
	}

	return 1;
}

static uint32_t enemy_block_check(int32_t x, int32_t y)
{
	if(x < 0)
		return 0;

	if(x >= bmapwidth)
		return 0;

	if(y < 0)
		return 0;

	if(y >= bmapheight)
		return 0;

	P_BlockThingsIterator(x, y, PIT_EnemySearch);
	if(enemy_look_pick)
		return 1;

	return 0;
}

mobj_t *look_for_monsters(mobj_t *mo, uint32_t dist)
{
	int32_t sx, sy;
	mobj_t *ret;

	enemy_looker = mo;
	enemy_look_pick = NULL;

	sx = (mo->x - bmaporgx) >> MAPBLOCKSHIFT;
	sy = (mo->y - bmaporgy) >> MAPBLOCKSHIFT;

	if(enemy_block_check(sx, sy))
		return enemy_look_pick;

	for(uint32_t count = 1; count <= dist; count++)
	{
		int32_t lx, ly;
		int32_t hx, hy;
		int32_t px, py;

		lx = sx - count;
		ly = sy - count;
		hx = sx + count;
		hy = sy + count;

		px = lx;
		py = ly;

		// top
		while(1)
		{
			if(enemy_block_check(px, py))
				return enemy_look_pick;
			if(px >= hx)
				break;
			px++;
		}

		// right
		do
		{
			py++;
			if(enemy_block_check(px, py))
				return enemy_look_pick;
		} while(py < hy);

		// bot
		do
		{
			px--;
			if(enemy_block_check(px, py))
				return enemy_look_pick;
		} while(px > lx);

		// left
		do
		{
			py--;
			if(enemy_block_check(px, py))
				return enemy_look_pick;
		} while(py > ly+1);
	}

	return NULL;
}

//
// slope to angle

static inline uint32_t SlopeDiv(unsigned num, unsigned den)
{
	unsigned ans;

	if(den < 512)
		return SLOPERANGE;

	ans = (num << 3) / (den >> 8);

	return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

angle_t slope_to_angle(fixed_t slope)
{
	if(slope > 0)
	{
		if(slope < FRACUNIT)
			return tantoangle[SlopeDiv(slope, FRACUNIT)];
		else
			return ANG90 - 1 - tantoangle[SlopeDiv(FRACUNIT, slope)];
	} else
	{
		slope = -slope;
		if(slope < FRACUNIT)
			return -tantoangle[SlopeDiv(slope, FRACUNIT)];
		else
			return ANG270 + tantoangle[SlopeDiv(FRACUNIT, slope)];
	}
}

//
// player aim

static uint32_t player_aim(player_t *pl, angle_t *angle, fixed_t *slope, uint32_t seeker)
{
	static player_t *cc_player;
	static angle_t cc_angle;
	static angle_t cr_angle;
	static mobj_t *cr_target;
	static fixed_t cr_slope;
	static uint32_t cr_res;
	mobj_t *mo = pl->mo;
	fixed_t sl;
	angle_t an = *angle;

	if(	act_cc_tick == leveltime &&
		cc_player == pl &&
		cc_angle == *angle
	)
	{
		linetarget = cr_target;
		*slope = cr_slope;
		if(cr_res)
		{
			*angle = cr_angle;
			return 1;
		} else
			return 0;
	}

	act_cc_tick = leveltime;
	cc_player = pl;
	cc_angle = *angle;

	if(player_info[pl - players].flags & PLF_AUTO_AIM || map_level_info->flags & MAP_FLAG_NO_FREELOOK)
	{
		// autoaim enabled
		sl = P_AimLineAttack(mo, an, AIMRANGE);
		if(!linetarget)
		{
			an += 1 << 26;
			sl = P_AimLineAttack(mo, an, AIMRANGE);

			if(!linetarget)
			{
				an -= 2 << 26;
				sl = P_AimLineAttack(mo, an, AIMRANGE);

				if(!linetarget)
				{
					*slope = 0;
					cr_res = 0;
					goto done;
				}
			}
		}
		*slope = sl;
		*angle = an;

		cr_res = 1;
		goto done;
	} else
	{
		// autoaim disabled
		*slope = 0;
		// seeker missile check
		linetarget = NULL;
		if(seeker)
			P_AimLineAttack(mo, an, AIMRANGE);

		cr_res = 0;
		goto done;
	}

done:
	cr_target = linetarget;
	cr_slope = *slope;
	cr_angle = *angle;
	return cr_res;
}

//
// player ammo

static uint32_t remove_ammo(mobj_t *mo)
{
	player_t *pl = mo->player;
	mobjinfo_t *weap = pl->readyweapon;
	uint32_t take;

	// ammo check
	if(!weapon_has_ammo(mo, weap, pl->attackdown))
		return 1;

	// which ammo
	take = pl->attackdown;
	if(take > 1)
	{
		if(weap->eflags & MFE_WEAPON_ALT_USES_BOTH)
			take |= 1;
	} else
	{
		if(weap->eflags & MFE_WEAPON_PRIMARY_USES_BOTH)
			take |= 2;
	}

	// ammo use
	if(take & 1 && weap->weapon.ammo_type[0])
		inventory_take(mo, weap->weapon.ammo_type[0], weap->weapon.ammo_use[0]);
	if(take & 2 && weap->weapon.ammo_type[1])
		inventory_take(mo, weap->weapon.ammo_type[1], weap->weapon.ammo_use[1]);

	return 0;
}

//
// projectile spawn

fixed_t projectile_speed(mobjinfo_t *info)
{
	if(info->fast_speed && (fastparm || gameskill == sk_nightmare))
		return info->fast_speed;
	else
		return info->speed;
}

void missile_stuff(mobj_t *mo, mobj_t *source, mobj_t *target, fixed_t speed, angle_t angle, angle_t pitch, fixed_t slope)
{
	if(source && mo->flags2 & MF2_SPAWNSOUNDSOURCE)
		S_StartSound(SOUND_CHAN_WEAPON(source), mo->info->seesound);
	else
		S_StartSound(mo, mo->info->seesound);

	if(!(mo->iflags & MFI_MARKED))
	{
		mo->target = source;
		if(mo->flags1 & MF1_SEEKERMISSILE)
			mo->tracer = target;
		mo->angle = angle;

		if(slope || pitch)
		{
			if(slope)
				pitch += slope_to_angle(slope);
			pitch >>= ANGLETOFINESHIFT;
			mo->momz = FixedMul(speed, finesine[pitch]);
			speed = FixedMul(speed, finecosine[pitch]);
		}

		angle >>= ANGLETOFINESHIFT;
		mo->momx = FixedMul(speed, finecosine[angle]);
		mo->momy = FixedMul(speed, finesine[angle]);
	} else
		mo->iflags &= ~MFI_MARKED;

	if(mo->flags1 & MF1_RANDOMIZE && mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	// this is an extra addition
	// projectile spawn can be called in pickup function,
	// that creates nested 'P_TryMove' call
	// which breaks basicaly everything
	// so let's avoid that
	if(!source->custom_inventory)
	{
		fixed_t x, y, z;

		P_UnsetThingPosition(mo);

		x = mo->momx >> 1;
		y = mo->momy >> 1;
		z = mo->momz >> 1;

		// extra check for faster projectiles
		while(abs(x) > 14 * FRACUNIT || abs(y) > 14 * FRACUNIT)
		{
			x >>= 1;
			y >>= 1;
			z >>= 1;
		}

		mo->x += x;
		mo->y += y;
		mo->z += z;

		P_SetThingPosition(mo);

		if(!P_TryMove(mo, mo->x, mo->y))
		{
			if(mo->flags & MF_SHOOTABLE)
				mobj_damage(mo, NULL, NULL, 100000, 0);
			else
				mobj_explode_missile(mo);
		}
	}
}

//
// shatter / burst

static void shatter_spawn(mobj_t *mo, uint32_t type)
{
	uint32_t count, i;
	mobj_t *inside;

	inside = mo;
	while(inside && !(inside->flags & MF_SOLID))
		inside = inside->inside;

	mo->momx = 0;
	mo->momy = 0;
	mo->momz = 0;

	count = FixedMul(mo->info->height, mo->radius) >> (FRACBITS + 5);
	if(count < 4)
		count = 4;

	count += (P_Random() % (count / 4));
	if(count < 8)
		count = 8;

	// limit the amount for low-end PCs
	count /= 4;
	if(count > 100)
		count = 100;

	do
	{
		mobj_t *th;
		fixed_t x, y, z;

		x = -mo->radius + ((mo->radius >> 7) * P_Random());
		x += mo->x;
		y = -mo->radius + ((mo->radius >> 7) * P_Random());
		y += mo->y;
		z = (mo->info->height >> 8) * P_Random();
		z += mo->z;

		th = P_SpawnMobj(x, y, z, type);
		th->momx = -FRACUNIT + (P_Random() << 9);
		th->momy = -FRACUNIT + (P_Random() << 9);
		th->momz = 4 * FixedDiv(th->z - mo->z, mo->info->height);

		th->inside = inside;
	} while(--count);
}

//
// original weapon attacks
// these are not available for DECORATE
// and so are only used in primary fire

__attribute((regparm(2),no_caller_saved_registers))
void A_OldProjectile(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint16_t count;
	uint16_t proj = (uint32_t)st->arg;
	player_t *pl = mo->player;
	uint16_t ammo = pl->readyweapon->weapon.ammo_type[0];
	fixed_t slope, z;
	angle_t angle, pitch;
	mobj_t *th;

	if(proj == 35) // BFG
		count = dehacked.bfg_cells;
	else
		count = 1;

	if(proj == 34) // plasma
	{
		uint32_t state;
		state = pl->readyweapon->st_weapon.flash;
		state += P_Random() & 1;
		weapon_set_state(pl, 1, pl->readyweapon, state, 0);
	}

	inventory_take(mo, ammo, count);

	pitch = 0;
	angle = mo->angle;

	if(!player_aim(pl, &angle, &slope, 0))
		pitch = mo->pitch;

	z = mo->z;
	z += mo->height / 2;
	z += mo->info->player.attack_offs;
	z += mo->player->viewz - mo->z - mo->info->player.view_height;

	th = P_SpawnMobj(mo->x, mo->y, z, proj);
	missile_stuff(th, mo, NULL, projectile_speed(th->info), angle, pitch, slope);
}

__attribute((regparm(2),no_caller_saved_registers))
void A_OldBullets(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint16_t count;
	uint16_t sound = (uint32_t)st->arg;
	player_t *pl = mo->player;
	uint16_t ammo = pl->readyweapon->weapon.ammo_type[0];
	uint16_t hs, vs;
	uint32_t state;
	angle_t angle;

	if(sound == 4)
		count = 2;
	else
		count = 1;

	if(!inventory_take(mo, ammo, count))
		return;

	state = pl->readyweapon->st_weapon.flash;

	switch(sound)
	{
		case 86:
			state += pl->psprites[0].state - &states[52];
		case 1:
			count = 1;
			hs = pl->refire ? 18 : 0;
			vs = 0;
		break;
		case 2:
			count = 7;
			hs = 18;
			vs = 0;
		break;
		default:
			count = 20;
			hs = 19;
			vs = 5;
		break;
	}

	S_StartSound(SOUND_CHAN_WEAPON(mo), sound);

	// mobj to 'melee' animation
	if(mo->animation != ANIM_MELEE && mo->info->state_melee)
		mobj_set_animation(mo, ANIM_MELEE);

	weapon_set_state(pl, 1, pl->readyweapon, state, 0);

	angle = mo->angle;

	if(!player_aim(pl, &angle, &bulletslope, 0))
		bulletslope = finetangent[(pl->mo->pitch + ANG90) >> ANGLETOFINESHIFT];

	for(uint32_t i = 0; i < count; i++)
	{
		uint32_t damage;
		angle_t aaa;
		fixed_t sss;

		damage = 5 + 5 * (P_Random() % 3);
		aaa = angle;
		if(hs)
			aaa += (P_Random() - P_Random()) << hs;
		sss = bulletslope;
		if(vs)
			sss += (P_Random() - P_Random()) << vs;

		P_LineAttack(mo, aaa, MISSILERANGE, sss, damage);
	}
}

//
// hide / restore items

__attribute((regparm(2),no_caller_saved_registers))
void A_SpecialHide(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(	mo->spawnpoint.options != mo->type ||
		!mo->tics ||
		mo->info->eflags & MFE_INVENTORY_NEVERRESPAWN
	){
		mobj_remove(mo);
		return;
	}

	P_UnsetThingPosition(mo);
	mo->flags |= MF_NOBLOCKMAP | MF_NOSECTOR;
	P_SetThingPosition(mo);
}

__attribute((regparm(2),no_caller_saved_registers))
void A_SpecialRestore(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	P_UnsetThingPosition(mo);

	mo->flags = mo->info->flags;
	mo->x = (fixed_t)mo->spawnpoint.x * FRACUNIT;
	mo->y = (fixed_t)mo->spawnpoint.y * FRACUNIT;

	P_SetThingPosition(mo);

	P_CheckPosition(mo, mo->x, mo->y);
	mo->floorz = tmfloorz;
	mo->ceilingz = tmceilingz;

	if(mo->z > mo->ceilingz - mo->height)
		mo->z = mo->ceilingz - mo->height;
	if(mo->z < mo->floorz)
		mo->z = mo->floorz;

	P_SpawnMobj(mo->x, mo->y, mo->z, 40); // MT_IFOG
	S_StartSound(mo, 90); // sfx_itmbk

	stfunc(mo, 0xFFFFFFFF, STATE_SET_ANIMATION | ANIM_SPAWN);
}

//
// weapon (logic)

__attribute((regparm(2),no_caller_saved_registers))
void A_Lower(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	int32_t value;

	if(!pl)
		return;

	if(mo->custom_inventory)
		return;

	value = actarg_integer(mo, st->arg, 0, 6);

	pl->weapon_ready = 0;
	pl->psprites[1].sy += value;

	if(pl->psprites[1].sy < WEAPONBOTTOM)
		return;

	pl->psprites[1].sy = WEAPONBOTTOM;
	pl->psprites[1].state = NULL;

	if(pl->state == PST_DEAD)
	{
		if(pl->readyweapon->st_weapon.deadlow)
			stfunc(mo, pl->readyweapon->st_weapon.deadlow, 0);
		return;
	}

	if(!pl->health)
	{
		// Voodoo Doll ...
		for(uint32_t i = 0; i < NUMPSPRITES; i++)
			pl->psprites[i].state = NULL;
		return;
	}

	pl->readyweapon = pl->pendingweapon;
	pl->pendingweapon = NULL;

	if(!pl->readyweapon)
		return;

	pl->stbar_update |= STU_WEAPON_NOW;

	S_StartSound(SOUND_CHAN_WEAPON(mo), pl->readyweapon->weapon.sound_up);

	stfunc(mo, pl->readyweapon->st_weapon.raise, 0);
}

__attribute((regparm(2),no_caller_saved_registers))
void A_Raise(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	int32_t value;

	if(!pl)
		return;

	if(mo->custom_inventory)
		return;

	value = actarg_integer(mo, st->arg, 0, 6);

	pl->weapon_ready = 0;
	pl->psprites[1].sy -= value;

	if(pl->psprites[1].sy > WEAPONTOP)
		return;

	pl->psprites[1].sy = WEAPONTOP;

	stfunc(mo, pl->readyweapon->st_weapon.ready, 0);
}

//
// A_GunFlash

static __attribute((regparm(2),no_caller_saved_registers))
void A_GunFlash(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	uint32_t state, extra;
	uint32_t flags;

	if(!pl)
		return;

	if(mo->custom_inventory)
		return;

	state = actarg_state(mo, st->arg, 0, &extra);
	flags = actarg_raw(mo, st->arg, 1, 0);

	// mobj to 'melee' animation
	if(!(flags & GFF_NOEXTCHANGE) && mo->info->state_melee)
		mobj_set_animation(mo, ANIM_MELEE);

	if(!state)
	{
		extra = 0;
		if(pl->attackdown > 1)
			state = pl->readyweapon->st_weapon.flash_alt;
		else
			state = pl->readyweapon->st_weapon.flash;
	}

	if(!state && !extra)
		return;

	state = dec_reslove_state(pl->readyweapon, 0, state, extra);

	// just hope that this is called in 'weapon PSPR'
	pl->psprites[1].state = states + state;
	pl->psprites[1].tics = 0;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_DoomGunFlash(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	// replacement for Doom codeptr
	player_t *pl = mo->player;
	uint32_t state;

	if(!mo->player)
		return;

	state = pl->readyweapon->st_weapon.flash;
	weapon_set_state(pl, 1, pl->readyweapon, state, 0);
}

//
// A_CheckReload

__attribute((regparm(2),no_caller_saved_registers))
void A_CheckReload(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->player)
		return;
	if(mo->custom_inventory)
		return;
	weapon_check_ammo(mo->player);
}

//
// weapon (light)

__attribute((regparm(2),no_caller_saved_registers))
void A_Light0(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->player)
		return;
	mo->player->extralight = 0;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_Light1(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->player)
		return;
	mo->player->extralight = 1;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_Light2(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->player)
		return;
	mo->player->extralight = 2;
}

//
// weapon (attack)

__attribute((regparm(2),no_caller_saved_registers))
void A_WeaponReady(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	uint32_t flags;

	if(!pl)
		return;

	if(mo->custom_inventory)
		return;

	pl->psprites[1].sx = 1;
	pl->psprites[1].sy = WEAPONTOP;

	flags = actarg_raw(mo, st->arg, 0, 0);

	if(extra_config.center_weapon != 2)
		pl->weapon_ready = 0;

	// mobj animation (not shooting)
	if(mo->animation == ANIM_MELEE || mo->animation == ANIM_MISSILE)
		mobj_set_animation(mo, ANIM_SPAWN);

	// disable switch
	if(flags & WRF_DISABLESWITCH)
		pl->pendingweapon = NULL;

	// new selection
	if((pl->pendingweapon && !(flags & WRF_NOSWITCH)) || !pl->health)
	{
		stfunc(mo, pl->readyweapon->st_weapon.lower, 0);
		return;
	}

	// sound
	if(	pl->readyweapon->weapon.sound_ready &&
		st == states + pl->readyweapon->st_weapon.ready
	)
		S_StartSound(SOUND_CHAN_WEAPON(mo), pl->readyweapon->weapon.sound_ready);

	if(!(pl->readyweapon->eflags & MFE_WEAPON_NOAUTOFIRE) || !pl->attackdown)
	{
		// primary attack
		if(pl->cmd.buttons & BT_ATTACK && !(flags & WRF_NOPRIMARY))
		{
			if(weapon_fire(pl, 1, 0))
				return;
		}

		// secondary attack
		if(pl->cmd.buttons & BT_ALTACK && !(flags & WRF_NOSECONDARY))
		{
			if(weapon_fire(pl, 2, 0))
				return;
		}

		// not shooting
		pl->attackdown = 0;
	} else
	if(!(pl->cmd.buttons & (BT_ATTACK | BT_ALTACK)))
		// not shooting
		pl->attackdown = 0;

	// enable bob
	if(!(flags & WRF_NOBOB))
		pl->weapon_ready = 1;
}

//
// A_Refire

__attribute((regparm(2),no_caller_saved_registers))
void A_ReFire(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	uint32_t btn, state, extra;

	if(!pl)
		return;

	if(mo->custom_inventory)
		return;

	state = actarg_state(mo, st->arg, 0, &extra);

	state = dec_reslove_state(pl->readyweapon, 0, state, extra);

	if(pl->attackdown > 1)
		btn = BT_ALTACK;
	else
		btn = BT_ATTACK;

	if(	pl->cmd.buttons & btn &&
		!pl->pendingweapon &&
		pl->health
	) {
		pl->refire++;
		if(state)
			weapon_fire(pl, state, 2);
		else
			weapon_fire(pl, pl->attackdown, 1);
		return;
	}

	pl->refire = 0;
	weapon_check_ammo(pl);
}

//
// basic sounds

static __attribute((regparm(2),no_caller_saved_registers))
void A_Pain(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(mo->info->extra_type == ETYPE_PLAYERPAWN)
	{
		uint32_t hp = (mo->health - 1) / 25;
		if(hp > 3)
			hp = 3;
		S_StartSound(mo, mo->info->player.sound.pain[hp]);
		return;
	}
	S_StartSound(mo, mo->info->painsound);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_Scream(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	S_StartSound(mo->flags1 & MF1_BOSS ? NULL : mo, mo->info->deathsound);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_XScream(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(mo->info->extra_type == ETYPE_PLAYERPAWN)
	{
		S_StartSound(mo, mo->info->player.sound.gibbed);
		return;
	}
	S_StartSound(mo, 31);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_PlayerScream(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(mo->info->extra_type == ETYPE_PLAYERPAWN)
	{
		if(mo->health < -mo->info->spawnhealth / 2)
			S_StartSound(mo, mo->info->player.sound.xdeath);
		else
			S_StartSound(mo, mo->info->player.sound.death);
		return;
	}
	S_StartSound(mo, mo->info->deathsound);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_ActiveSound(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	S_StartSound(mo, mo->info->activesound);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_BrainPain(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	S_StartSound(NULL, 97);
}

//
// A_StartSound

static __attribute((regparm(2),no_caller_saved_registers))
void A_StartSound(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	void *origin;
	uint32_t sound;
	uint32_t slot;
	uint32_t attn;

	if(actarg_raw(mo, st->arg, 2, 0) != 0)
		return;

	if(actarg_raw(mo, st->arg, 3, 1 << MATHFRAC) != (1 << MATHFRAC))
		return;

	sound = actarg_magic(mo, st->arg, 0, 0);
	slot = actarg_raw(mo, st->arg, 1, CHAN_BODY);
	attn = actarg_raw(mo, st->arg, 4, ATTN_NORM);

	if(attn != ATTN_NORM)
		origin = NULL;
	else
	switch(slot)
	{
		case CHAN_WEAPON:
			origin = SOUND_CHAN_WEAPON(mo);
		break;
		case CHAN_VOICE:
			origin = mo;
		break;
		default:
			origin = SOUND_CHAN_BODY(mo);
		break;
	}

	S_StartSound(origin, sound);
}

//
// A_FaceTarget

static __attribute((regparm(2),no_caller_saved_registers))
void A_FaceTarget(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->target)
		return;

	mo->flags &= ~MF_AMBUSH;
	mo->angle = R_PointToAngle2(mo->x, mo->y, mo->target->x, mo->target->y);

	if(mo->target->flags & MF_SHADOW)
		mo->angle += (P_Random() - P_Random()) << 21;
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_FaceTracer(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->tracer)
		return;

	mo->flags &= ~MF_AMBUSH;
	mo->angle = R_PointToAngle2(mo->x, mo->y, mo->tracer->x, mo->tracer->y);

	if(mo->tracer->flags & MF_SHADOW)
		mo->angle += (P_Random() - P_Random()) << 21;
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_FaceMaster(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->master)
		return;

	mo->flags &= ~MF_AMBUSH;
	mo->angle = R_PointToAngle2(mo->x, mo->y, mo->master->x, mo->master->y);

	if(mo->master->flags & MF_SHADOW)
		mo->angle += (P_Random() - P_Random()) << 21;
}

//
// A_NoBlocking

static __attribute((regparm(2),no_caller_saved_registers))
void A_NoBlocking(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mobj_t *inside = mo;

	while(inside && !(inside->flags & MF_SOLID))
		inside = inside->inside;

	mo->flags &= ~MF_SOLID;

	if(mo->info->extra_type == ETYPE_PLAYERPAWN)
		// no item drops for player classes
		return;

	// drop items
	for(mobj_dropitem_t *drop = mo->info->dropitem.start; drop < (mobj_dropitem_t*)mo->info->dropitem.end; drop++)
	{
		mobj_t *item;

		if(!drop->type) // HACK
			continue;

		if(drop->chance < 255 && drop->chance <= P_Random())
			continue;

		item = P_SpawnMobj(mo->x, mo->y, mo->z + (8 << FRACBITS), drop->type);
		item->inside = mo;
		item->flags |= MF_DROPPED;
		item->angle = P_Random() << 24;
		item->momx = FRACUNIT - (P_Random() << 9);
		item->momy = FRACUNIT - (P_Random() << 9);
		item->momz = (4 << 16) + (P_Random() << 10);
		item->inside = inside;
	}
}

//
// A_Look

__attribute((regparm(2),no_caller_saved_registers))
void A_Look(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mobj_t *targ;

	mo->threshold = 0;
	targ = mo->subsector->sector->soundtarget;

	if(	targ && targ != mo &&
		(targ->flags & MF_SHOOTABLE) &&
		!(targ->flags1 & MF1_NOTARGET) &&
		targ->render_style < RS_INVISIBLE
	){
		mo->target = targ;
		if(mo->flags & MF_AMBUSH)
		{
			if(P_CheckSight(mo, mo->target))
				goto seeyou;
		} else
			goto seeyou;
	}

	if(!P_LookForPlayers(mo, 0))
		return;

	if(	mo->target->flags1 & MF1_NOTARGET ||
		mo->target->render_style >= RS_INVISIBLE
	){
		mo->target = NULL;
		return;
	}

seeyou:
	if(mo->info->seesound)
		S_StartSound(mo->flags1 & MF1_BOSS ? NULL : mo, mo->info->seesound);

	mobj_set_animation(mo, ANIM_SEE);
}

//
// A_Chase

static __attribute((regparm(2),no_caller_saved_registers))
void A_Chase(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	fixed_t speed, oldspeed;
	state_t *state;

	if(mo->target == mo || (mo->target && mo->target->flags1 & MF1_NOTARGET))
		mo->target = NULL;

	oldspeed = mo->info->speed;

	if(mo->iflags & MFI_FOLLOW_MOVE)
		speed = 0;
	else
	if(mo->info->fast_speed && (fastparm || gameskill == sk_nightmare))
		speed = mo->info->fast_speed;
	else
		speed = mo->info->speed;

	// attack detection
	state = mo->state;

	// workaround for non-fixed speed
	mo->info->speed = (speed + (FRACUNIT / 2)) >> FRACBITS;
	doom_A_Chase(mo);
	mo->info->speed = oldspeed;

	// this is not how ZDoom handles stealth
	if(state != mo->state && mo->flags2 & MF2_STEALTH)
		mo->alpha_dir = 14;
	else
	if(mo->render_alpha)
		mo->alpha_dir = -10;

	// HACK - move other sound slots
	mo->sound_body.x = mo->x;
	mo->sound_body.y = mo->y;
	mo->sound_weapon.x = mo->x;
	mo->sound_weapon.y = mo->y;
}

//
// A_VileChase

static __attribute((regparm(2),no_caller_saved_registers))
void A_VileChase(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	fixed_t speed;

	if(mo->target == mo || (mo->target && mo->target->flags1 & MF1_NOTARGET))
		mo->target = NULL;

	if(mo->info->fast_speed && fastparm || gameskill == sk_nightmare)
		speed = mo->info->fast_speed;
	else
		speed = mo->info->speed;

	// workaround for non-fixed speed
	mo->info->speed = (speed + (FRACUNIT / 2)) >> FRACBITS;
	doom_A_VileChase(mo);
	mo->info->speed = speed;

	// HACK - move other sound slots
	mo->sound_body.x = mo->x;
	mo->sound_body.y = mo->y;
	mo->sound_weapon.x = mo->x;
	mo->sound_weapon.y = mo->y;
}

//
// A_SpawnProjectile

static __attribute((regparm(2),no_caller_saved_registers))
void A_SpawnProjectile(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	angle_t angle = mo->angle;
	angle_t atmp;
	fixed_t pitch = 0;
	int32_t dist = 0;
	fixed_t x, y, z, speed;
	mobj_t *th, *target;
	uint32_t flags;

	flags = actarg_raw(mo, st->arg, 4, 0); // flags

	x = mo->x;
	y = mo->y;
	z = mo->z + actarg_fixed(mo, st->arg, 1, 32 * FRACUNIT); // spawnheight

	if(flags & CMF_TRACKOWNER && mo->flags & MF_MISSILE)
	{
		mo = mo->target;
		if(!mo)
			return;
	}

	if(flags & CMF_AIMDIRECTION)
		target = NULL;
	else
		target = actarg_pointer(mo, st->arg, 6, AAPTR_TARGET); // pointer

	if(!target)
	{
		flags |= CMF_AIMDIRECTION;
		flags &= ~CMF_AIMOFFSET;
	}

	if(flags & CMF_CHECKTARGETDEAD && (!target || target->health <= 0))
		return;

	if(flags & CMF_AIMOFFSET)
	{
		angle = R_PointToAngle2(x, y, target->x, target->y);
		dist = P_AproxDistance(target->x - x, target->y - y);
	}

	speed = actarg_fixed(mo, st->arg, 2, 0); // spawnofs_xy
	if(speed)
	{
		angle_t a = (mo->angle - ANG90) >> ANGLETOFINESHIFT;
		x += FixedMul(speed, finecosine[a]);
		y += FixedMul(speed, finesine[a]);
	}

	if(!(flags & (CMF_AIMOFFSET|CMF_AIMDIRECTION)))
	{
		angle = R_PointToAngle2(x, y, target->x, target->y);
		dist = P_AproxDistance(target->x - x, target->y - y);
	}

	atmp = actarg_angle(mo, st->arg, 3); // angle

	if(flags & CMF_ABSOLUTEANGLE)
		angle = atmp;
	else
	{
		angle += atmp;
		if(target && target->flags & MF_SHADOW)
			angle += (P_Random() - P_Random()) << 20;
	}

	th = P_SpawnMobj(x, y, z, actarg_magic(mo, st->arg, 0, 0)); // missiletype

	speed = projectile_speed(th->info);

	if(flags & (CMF_AIMDIRECTION|CMF_ABSOLUTEPITCH))
		pitch = -actarg_angle(mo, st->arg, 5); // pitch
	else
	if(speed)
	{
		dist /= speed;
		if(dist <= 0)
			dist = 1;
		dist = ((target->z + (target->height / 2)) - z) / dist; // TODO: propper aim for the middle?
		th->momz = FixedDiv(dist, speed);
		if(flags & CMF_OFFSETPITCH)
			pitch = -actarg_angle(mo, st->arg, 5); // pitch
	}

	missile_stuff(th, mo, target, speed, angle, pitch, th->momz);
}

//
// A_CustomBulletAttack

static __attribute((regparm(2),no_caller_saved_registers))
void A_CustomBulletAttack(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t damage;
	angle_t angle;
	fixed_t slope;
	fixed_t range;
	uint32_t bcount;
	uint32_t flags;
	angle_t spread_hor;
	angle_t spread_ver;

	if(!mo->target)
		return;

	bcount = actarg_integer(mo, st->arg, 2, 0); // numbullets
	if(bcount <= 0)
		return;

	spread_hor = actarg_angle(mo, st->arg, 0); // horz_spread
	spread_ver = actarg_angle(mo, st->arg, 1); // vert_spread

	damage = actarg_integer(mo, st->arg, 3, 0); // damageperbullet
	mo_puff_type = actarg_magic(mo, st->arg, 4, 37); // pufftype
	range = actarg_fixed(mo, st->arg, 5, 0); // range
	flags = actarg_raw(mo, st->arg, 6, 0); // flags

	if(!(flags & CBAF_AIMFACING))
	{
		angle = R_PointToAngle2(mo->x, mo->y, mo->target->x, mo->target->y);
		mo->angle = angle;
	} else
		angle = mo->angle;

	if(mo->target->flags & MF_SHADOW)
	{
		angle += (P_Random() - P_Random()) << 20;
		mo->angle = angle;
	}

	if(mobjinfo[mo_puff_type].replacement)
		mo_puff_type = mobjinfo[mo_puff_type].replacement;
	else
		mo_puff_type = mo_puff_type;

	mo_puff_flags = !!(flags & CBAF_NORANDOMPUFFZ);

	if(!range)
		range = 2048 * FRACUNIT;

	slope = P_AimLineAttack(mo, angle, range);

	S_StartSound(SOUND_CHAN_WEAPON(mo), mo->info->attacksound);

	for(uint32_t i = 0; i < bcount; i++)
	{
		angle_t aaa;
		fixed_t sss;
		uint32_t dmg;

		if(!(flags & CBAF_NORANDOM))
			dmg = damage * (P_Random() % 3) + 1;
		else
			dmg = damage;

		aaa = angle;
		sss = slope;

		if(spread_hor)
		{
			aaa -= spread_hor;
			aaa += (spread_hor >> 7) * P_Random();
		}
		if(spread_ver)
		{
			fixed_t tmp;
			tmp = -spread_ver;
			tmp += (spread_ver >> 7) * P_Random();
			sss += tmp >> 14;
		}

		P_LineAttack(mo, aaa, range, sss, damage);
	}

	// must restore original puff!
	mo_puff_type = 37;
	mo_puff_flags = 0;
}

//
// A_CustomMeleeAttack

static __attribute((regparm(2),no_caller_saved_registers))
void A_CustomMeleeAttack(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t damage;
	uint32_t dtype;

	if(!mo->target)
		return;

	A_FaceTarget(mo, NULL, NULL);

	if(!mobj_check_melee_range(mo))
	{
		S_StartSound(mo, actarg_magic(mo, st->arg, 2, 0)); // misssound
		return;
	}

	S_StartSound(mo, actarg_magic(mo, st->arg, 1, 0)); // meleesound

	damage = actarg_integer(mo, st->arg, 0, 0); // damage
	dtype = actarg_magic(mo, st->arg, 3, DAMAGE_NORMAL); // damagetype

	damage |= DAMAGE_SET_TYPE(dtype);

	mobj_damage(mo->target, mo, mo, damage, NULL);
}

//
// A_VileTarget

static __attribute((regparm(2),no_caller_saved_registers))
void A_VileTarget(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mobj_t *th;
	uint32_t type;

	if(!mo->target)
		return;

	type = actarg_magic(mo, st->arg, 0, 0); // type

	A_FaceTarget(mo, NULL, NULL);

	th = P_SpawnMobj(mo->target->x, mo->target->y, mo->target->z, type);
	mo->tracer = th;
	th->target = mo;
	th->tracer = mo->target;
}

//
// A_VileAttack

static __attribute((regparm(2),no_caller_saved_registers))
void A_VileAttack(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	int32_t xl, xh, yl, yh;
	uint32_t damage;
	mobj_t *fire;
	angle_t an;
	fixed_t	dist;

	if(!mo->target)
		return;

	A_FaceTarget(mo, NULL, NULL);

	if(!P_CheckSight(mo, mo->target))
		return;

	damage = actarg_integer(mo, st->arg, 1, 20); // initialdamage
	bombdmgtype = actarg_magic(mo, st->arg, 5, 0); // damagetype

	dist = actarg_raw(mo, st->arg, 6, 0); // flags

	if(dist & VAF_DMGTYPEAPPLYTODIRECT)
		damage |= DAMAGE_SET_TYPE(bombdmgtype);

	dist = actarg_magic(mo, st->arg, 0, 0); // sound

	S_StartSound(mo, dist);
	mobj_damage(mo->target, mo, mo, damage, 0);

	dist = actarg_fixed(mo, st->arg, 4, 0); // thrustfactor

	mo->target->momz = FixedMul(1000 * FRACUNIT / mo->target->info->mass, dist);

	an = mo->angle >> ANGLETOFINESHIFT;

	fire = mo->tracer;
	if(!fire)
		return;

	bombdamage = actarg_integer(mo, st->arg, 2, 70); // blastdamage
	bombdist = actarg_fixed(mo, st->arg, 3, 70 * FRACUNIT); // blastradius

	fire->x = mo->target->x - FixedMul(24*FRACUNIT, finecosine[an]);
	fire->y = mo->target->y - FixedMul(24*FRACUNIT, finesine[an]);	

	dist = bombdist + MAXRADIUS;
	yh = (fire->y + dist - bmaporgy) >> MAPBLOCKSHIFT;
	yl = (fire->y - dist - bmaporgy) >> MAPBLOCKSHIFT;
	xh = (fire->x + dist - bmaporgx) >> MAPBLOCKSHIFT;
	xl = (fire->x - dist - bmaporgx) >> MAPBLOCKSHIFT;

	bombsource = mo;
	bombspot = fire;
	bombfist = 0;
	bombflags = 0;

	for(int32_t y = yl; y <= yh; y++)
		for(int32_t x = xl; x <= xh; x++)
			P_BlockThingsIterator(x, y, PIT_Explode);
}

//
// A_FireProjectile

static __attribute((regparm(2),no_caller_saved_registers))
void A_FireProjectile(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	mobj_t *th;
	angle_t angle, pitch, aaa;
	fixed_t slope, offs;
	fixed_t x, y, z;
	uint32_t flags;
	uint32_t missiletype;

	if(!pl)
		return;

	if(	!mo->custom_inventory &&
		actarg_raw(mo, st->arg, 2, 1) && // useammo
		remove_ammo(mo)
	)
		return;

	missiletype = actarg_magic(mo, st->arg, 0, 0);
	aaa = actarg_angle(mo, st->arg, 1); // pitch

	offs = actarg_fixed(mo, st->arg, 3, 0); // spawnofs_xy

	flags = actarg_raw(mo, st->arg, 5, 0); // flags
	pitch = -actarg_angle(mo, st->arg, 6); // pitch

	angle = mo->angle;

	if(flags & FPF_AIMATANGLE)
		angle += aaa;

	if(!player_aim(pl, &angle, &slope, mobjinfo[missiletype].flags1 & MF1_SEEKERMISSILE))
		pitch += mo->pitch;

	if(aaa && !(flags & FPF_AIMATANGLE))
		angle = mo->angle + aaa;

	x = mo->x;
	y = mo->y;
	z = mo->z;
	z += mo->height / 2;
	z += mo->info->player.attack_offs;
	z += mo->player->viewz - mo->z - mo->info->player.view_height;
	z += actarg_fixed(mo, st->arg, 4, 0); // spawnheight

	if(offs)
	{
		angle_t a = (mo->angle - ANG90) >> ANGLETOFINESHIFT;
		x += FixedMul(offs, finecosine[a]);
		y += FixedMul(offs, finesine[a]);
	}

	th = P_SpawnMobj(x, y, z, missiletype);
	if(th->flags & MF_MISSILE)
		missile_stuff(th, mo, linetarget, projectile_speed(th->info), angle, pitch, slope);

	if(flags & FPF_TRANSFERTRANSLATION)
		th->translation = mo->translation;
}

//
// A_FireBullets

static __attribute((regparm(2),no_caller_saved_registers))
void A_FireBullets(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	uint32_t spread, damage;
	uint32_t flags;
	fixed_t slope, range;
	angle_t spread_hor;
	angle_t spread_ver;
	int32_t count;
	angle_t angle;

	if(!pl)
		return;

	flags = actarg_raw(mo, st->arg, 5, FBF_USEAMMO); // flags

	if(	!mo->custom_inventory &&
		flags & FBF_USEAMMO &&
		remove_ammo(mo)
	)
		return;

	spread_hor = actarg_angle(mo, st->arg, 0); // spread_horz
	spread_ver = actarg_angle(mo, st->arg, 1); // spread_vert
	count = actarg_integer(mo, st->arg, 2, 0); // numbullets
	damage = actarg_integer(mo, st->arg, 3, 0); // damage
	mo_puff_type = actarg_magic(mo, st->arg, 4, 37); // pufftype

	range = actarg_fixed(mo, st->arg, 6, 0); // range

	if(range <= 0)
		range = 8192 * FRACUNIT;

	if(mobjinfo[mo_puff_type].replacement)
		mo_puff_type = mobjinfo[mo_puff_type].replacement;
	mo_puff_flags = !!(flags & FBF_NORANDOMPUFFZ);

	if(count < 0)
	{
		count = -count;
		spread = 1;
	} else
	{
		if(!count)
		{
			spread = 0;
			count = 1;
		} else
		if(count > 1)
			spread = 1;
		else
			spread = pl->refire;
	}

	if(!(flags & FBF_NOFLASH) && mo->info->state_melee)
		mobj_set_animation(mo, ANIM_MELEE);

	angle = mo->angle;
	if(!player_aim(pl, &angle, &slope, 0))
		slope = finetangent[(pl->mo->pitch + ANG90) >> ANGLETOFINESHIFT];

	S_StartSound(SOUND_CHAN_WEAPON(mo), pl->readyweapon->attacksound);

	for(uint32_t i = 0; i < count; i++)
	{
		angle_t aaa;
		fixed_t sss;
		uint32_t dmg;

		if(!(flags & FBF_NORANDOM))
			dmg = damage * (P_Random() % 3) + 1;
		else
			dmg = damage;

		aaa = angle;
		sss = slope;

		if(spread)
		{
			if(spread_hor)
			{
				aaa -= spread_hor;
				aaa += (spread_hor >> 7) * P_Random();
			}
			if(spread_ver)
			{
				fixed_t tmp;
				tmp = -spread_ver;
				tmp += (spread_ver >> 7) * P_Random();
				sss += tmp >> 14;
			}
		}

		P_LineAttack(mo, aaa, range, sss, damage);
	}

	// must restore original puff!
	mo_puff_type = 37;
	mo_puff_flags = 0;
}

//
// A_CustomPunch

static __attribute((regparm(2),no_caller_saved_registers))
void A_CustomPunch(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	uint32_t flags, damage;
	fixed_t slope, range;
	angle_t angle;

	if(!pl)
		return;

	flags = actarg_raw(mo, st->arg, 2, 0); // flags

	if(	!mo->custom_inventory &&
		flags & CPF_USEAMMO &&
		!weapon_has_ammo(mo, pl->readyweapon, pl->attackdown)
	)
		return;

	damage = actarg_integer(mo, st->arg, 0, 0); // damage

	if(!actarg_raw(mo, st->arg, 1, 0)) // norandom
		damage *= 1 + (P_Random() & 7);

	mo_puff_type = actarg_magic(mo, st->arg, 3, 37); // pufftype

	range = actarg_fixed(mo, st->arg, 4, 0); // range

	if(range <= 0)
		range = 64 * FRACUNIT;

	if(mobjinfo[mo_puff_type].replacement)
		mo_puff_type = mobjinfo[mo_puff_type].replacement;
	mo_puff_flags = !!(flags & CPF_NORANDOMPUFFZ);

	angle = mo->angle;
	if(!player_aim(pl, &angle, &slope, 0))
		slope = finetangent[(pl->mo->pitch + ANG90) >> ANGLETOFINESHIFT];

	angle += (P_Random() - P_Random()) << 18;

	linetarget = NULL;
	P_LineAttack(mo, angle, range, slope, damage);

	if(linetarget)
	{
		if(flags & CPF_PULLIN)
			mo->flags |= MF_JUSTATTACKED;
		if(!(flags & CPF_NOTURN))
			mo->angle = R_PointToAngle2(mo->x, mo->y, linetarget->x, linetarget->y);
		if(!mo->custom_inventory && flags & CPF_USEAMMO)
			remove_ammo(mo);
		S_StartSound(SOUND_CHAN_WEAPON(mo), pl->readyweapon->attacksound);
	}

	// must restore original puff!
	mo_puff_type = 37;
	mo_puff_flags = 0;
}

//
// A_BFGSpray

static __attribute((regparm(2),no_caller_saved_registers))
void A_BFGSpray(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mobj_t *source = mo->target;
	uint32_t damage, type, count, dmgcnt;
	angle_t angle, astep;
	fixed_t range;

	if(!source)
		return;

	count = actarg_integer(mo, st->arg, 1, 40); // numrays

	if(!count)
		return;

	type = actarg_magic(mo, st->arg, 0, 42); // spraytype

	dmgcnt = actarg_integer(mo, st->arg, 2, 15); // damagecnt
	astep = actarg_angle(mo, st->arg, 3); // ang
	range = actarg_fixed(mo, st->arg, 4, 0); // distance
	// vrange is ignored and should be 32
	damage = actarg_integer(mo, st->arg, 6, 0); // defdamage

	angle = mo->angle;
	angle -= astep / 2;
	astep /= count;
	angle += astep / 2;

	for(uint32_t i = 0; i < count; i++, angle += astep)
	{
		uint32_t dmg = damage;

		P_AimLineAttack(source, angle, range);

		if(!linetarget)
			continue;

		P_SpawnMobj(linetarget->x, linetarget->y, linetarget->z + (linetarget->height >> 2), type);

		if(!dmg)
		{
			for(uint32_t j = 0; j < dmgcnt; j++)
				dmg += (P_Random() & 7) + 1;
		}

		dmg |= DAMAGE_SET_TYPE(mobjinfo[type].damage_type);
		mobj_damage(linetarget, source, source, dmg, NULL);
	}
}

//
// A_SeekerMissile

static __attribute((regparm(2),no_caller_saved_registers))
void A_SeekerMissile(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mobj_t *target = mo->tracer;
	uint32_t sub = 0;
	uint32_t flags;
	angle_t dir, delta;
	fixed_t speed, dist;

	speed = projectile_speed(mo->info);
	if(!speed)
		return;

	flags = actarg_raw(mo, st->arg, 2, 0); // flags

	if(flags & SMF_LOOK && !target)
	{
		uint16_t chance = actarg_integer(mo, st->arg, 3, 0); // chance
		if(!chance)
			chance = 50;

		if(chance < 256 && P_Random() >= chance)
			return;

		dist = actarg_integer(mo, st->arg, 4, 0); // distance
		if(!dist)
			dist = 10;

		target = look_for_monsters(mo, dist);
		mo->tracer = target;
	}

	if(!target)
		return;

	if(target->flags2 & MF2_CANTSEEK)
		return;

	if(!(target->flags & MF_SHOOTABLE))
	{
		mo->tracer = NULL;
		return;
	}

	dir = R_PointToAngle2(mo->x, mo->y, target->x, target->y);
	delta = dir - mo->angle;
	if(delta & 0x80000000)
	{
		delta = -delta;
		sub = 1;
	}

	if(!delta)
	{
		if(dir != mo->angle)
			delta = ANG180;
	}

	if(delta > actarg_angle(mo, st->arg, 0)) // threshold
	{
		angle_t maxturn = actarg_angle(mo, st->arg, 1);
		delta /= 2;
		if(delta > maxturn)
			delta = maxturn;
	}

	if(sub)
		mo->angle -= delta;
	else
		mo->angle += delta;

	dist = P_AproxDistance(target->x - mo->x, target->y - mo->y);
	dist /= speed;
	if(dist <= 0)
		dist = 1;
	dist = ((target->z + 32 * FRACUNIT) - mo->z) / dist;

	if(flags & SMF_PRECISE)
	{
		dist = FixedDiv(dist, speed);
		if(dist)
		{
			angle_t pitch = slope_to_angle(dist);
			pitch >>= ANGLETOFINESHIFT;
			mo->momz = FixedMul(speed, finesine[pitch]);
			speed = FixedMul(speed, finecosine[pitch]);
		}
	} else
	{
		if(	target->z + target->height < mo->z ||
			mo->z + mo->height < target->z
		)
			mo->momz = dist;
	}

	delta = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul(speed, finecosine[delta]);
	mo->momy = FixedMul(speed, finesine[delta]);
}

//
// A_SpawnItemEx

static __attribute((regparm(2),no_caller_saved_registers))
void A_SpawnItemEx(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	mobj_t *th, *origin;
	angle_t angle;
	fixed_t ss, cc;
	fixed_t x, y;
	fixed_t mx, my;
	fixed_t xx, yy;
	uint32_t flags;

	flags = actarg_integer(mo, st->arg, 9, 0); // failchance

	if(flags && P_Random() < flags)
		return;

	angle = actarg_angle(mo, st->arg, 7); // angle
	flags = actarg_raw(mo, st->arg, 8, 0); // flags

	if(!(flags & SXF_ABSOLUTEANGLE))
		angle += mo->angle;

	ss = finesine[angle >> ANGLETOFINESHIFT];
	cc = finecosine[angle >> ANGLETOFINESHIFT];

	xx = actarg_fixed(mo, st->arg, 1, 0); // xofs
	yy = actarg_fixed(mo, st->arg, 2, 0); // yofs

	if(flags & SXF_ABSOLUTEPOSITION)
	{
		x = mo->x + xx;
		y = mo->y + yy;
	} else
	{
		x = FixedMul(xx, cc);
		x += FixedMul(yy, ss);
		y = FixedMul(xx, ss);
		y -= FixedMul(yy, cc);
		x += mo->x;
		y += mo->y;
	}

	xx = actarg_fixed(mo, st->arg, 4, 0); // xvel
	yy = actarg_fixed(mo, st->arg, 5, 0); // yvel

	if(!(flags & SXF_ABSOLUTEVELOCITY))
	{
		mx = FixedMul(xx, cc);
		mx += FixedMul(yy, ss);
		my = FixedMul(xx, ss);
		my -= FixedMul(yy, cc);
	} else
	{
		mx = xx;
		my = yy;
	}

	xx = actarg_fixed(mo, st->arg, 3, 0); // zofs
	yy = actarg_magic(mo, st->arg, 0, 0); // missile

	th = P_SpawnMobj(x, y, mo->z + xx, yy);
	th->momx = mx;
	th->momy = my;
	th->momz = actarg_fixed(mo, st->arg, 6, 0); // zvel
	th->angle = angle;
	th->special.tid = actarg_integer(mo, st->arg, 10, 0); // tid

	if(flags & SXF_MULTIPLYSPEED)
	{
		fixed_t speed = projectile_speed(th->info);
		th->momx = FixedMul(mx, speed);
		th->momy = FixedMul(my, speed);
		th->momz = FixedMul(th->momz, speed);
	}

	if(flags & SXF_TRANSFERPITCH)
		th->pitch = mo->pitch;

	if(!(th->flags2 & MF2_DONTTRANSLATE))
	{
		if(flags & SXF_TRANSFERTRANSLATION)
			th->translation = mo->translation;
		else
		if(flags & SXF_USEBLOODCOLOR)
			th->translation = mo->info->blood_trns;
	}

	if(flags & SXF_TRANSFERPOINTERS)
	{
		th->target = mo->target;
		th->master = mo->master;
		th->tracer = mo->tracer;
	}

	origin = mo;
	if(!(flags & SXF_ORIGINATOR))
	{
		while(origin && (origin->flags & MF_MISSILE || origin->info->flags & MF_MISSILE))
			origin = origin->target;
	}

	if(flags & SXF_TELEFRAG)
		mobj_telestomp(th, x, y);

	if(th->flags1 & MF1_ISMONSTER)
	{
		if(!(flags & (SXF_NOCHECKPOSITION | SXF_TELEFRAG)) && !P_CheckPosition(th, x, y))
		{
			mobj_remove(th);
			return;
		}
		// TODO: friendly monster stuff should be here
	} else
	if(!(flags & SXF_TRANSFERPOINTERS))
		th->target = origin ? origin : mo;

	if(flags & SXF_NOPOINTERS)
	{
		th->target = NULL;
		th->master = NULL;
		th->tracer = NULL;
	}

	if(flags & SXF_SETMASTER)
		th->master = origin;

	if(flags & SXF_SETTARGET)
		th->target = origin;

	if(flags & SXF_SETTRACER)
		th->tracer = origin;

	if(flags & SXF_TRANSFERAMBUSHFLAG)
	{
		th->flags &= ~MF_AMBUSH;
		th->flags |= mo->flags & MF_AMBUSH;
	}

	if(flags & SXF_CLEARCALLERTID)
		mo->special.tid = 0;

	if(flags & SXF_ISTARGET)
		mo->target = th;

	if(flags & SXF_ISMASTER)
		mo->master = th;

	if(flags & SXF_ISTRACER)
		mo->tracer = th;

	if(th->flags & MF_MISSILE)
	{
		th->iflags |= MFI_MARKED; // skip most of missile stuff
		missile_stuff(th, mo, NULL, 0, angle, 0, 0);
	} else
	{
		while(mo && !(mo->flags & MF_SOLID))
			mo = mo->inside;
		th->inside = mo;
	}
}

//
// A_DropItem

static __attribute((regparm(2),no_caller_saved_registers))
void A_DropItem(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mobj_t *item;
	uint32_t chance, type;

	// dropamount is skipped and should be zero

	chance = actarg_integer(mo, st->arg, 2, 255); // chance
	if(chance < 255 && chance <= P_Random())
		return;

	type = actarg_magic(mo, st->arg, 0, 0); // item

	item = P_SpawnMobj(mo->x, mo->y, mo->z + (8 << FRACBITS), type);
	item->inside = mo;
	item->flags |= MF_DROPPED;
	item->angle = P_Random() << 24;
	item->momx = FRACUNIT - (P_Random() << 9);
	item->momy = FRACUNIT - (P_Random() << 9);
	item->momz = (4 << 16) + (P_Random() << 10);

	while(mo && !(mo->flags & MF_SOLID))
		mo = mo->inside;
	item->inside = mo;
}

//
// A_GiveInventory

static __attribute((regparm(2),no_caller_saved_registers))
void A_GiveInventory(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t amount, type;

	mo = actarg_pointer(mo, st->arg, 2, AAPTR_DEFAULT); // giveto
	if(!mo)
		return;

	type = actarg_magic(mo, st->arg, 0, 0); // type
	amount = actarg_integer(mo, st->arg, 1, 0); // amount

	if(!amount)
		amount++;

	mobj_give_inventory(mo, type, amount);
}

//
// A_TakeInventory

static __attribute((regparm(2),no_caller_saved_registers))
void A_TakeInventory(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t amount, type;

	// flags are skipped and should be zero

	mo = actarg_pointer(mo, st->arg, 3, AAPTR_DEFAULT); // takefrom
	if(!mo)
		return;

	type = actarg_magic(mo, st->arg, 0, 0); // type
	amount = actarg_integer(mo, st->arg, 1, 0); // amount

	if(!amount)
		amount = INV_MAX_COUNT;

	inventory_take(mo, type, amount);
}

//
// A_SelectWeapon

static __attribute((regparm(2),no_caller_saved_registers))
void A_SelectWeapon(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t type;

	if(!mo->player)
		return;

	type = actarg_magic(mo, st->arg, 0, 0); // whichweapon

	if(mo->player->readyweapon == mobjinfo + type)
		return;

	if(mobjinfo[type].extra_type != ETYPE_WEAPON)
		return;

	if(!inventory_check(mo, type))
		return;

	mo->player->pendingweapon = mobjinfo + type;
}

//
// text

static __attribute((regparm(2),no_caller_saved_registers))
void A_Print(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	fixed_t tics;
	uint32_t font;
	uint32_t lines;
	uint8_t *text;

	text = actarg_string(mo, st->arg, 0, &lines); // text
	if(!text)
		return;

	tics = actarg_fixed(mo, st->arg, 1, 3 * FRACUNIT); // time
	tics *= 35;
	tics >>= FRACBITS;
	tics += leveltime;

	font = actarg_magic(mo, st->arg, 2, 0); // font

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		player_t *pl = players + i;
		if(pl->camera == mo || pl->mo == mo)
		{
			pl->text.tic = tics;
			pl->text.font = font;
			pl->text.text = text;
			pl->text.lines = lines;
		}
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_PrintBold(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t tics;
	uint32_t font;
	uint32_t lines;
	uint8_t *text;

	text = actarg_string(mo, st->arg, 0, &lines); // text
	if(!text)
		return;

	tics = actarg_fixed(mo, st->arg, 1, 3 * FRACUNIT); // time
	tics *= 35;
	tics >>= FRACBITS;
	tics += leveltime;

	font = actarg_magic(mo, st->arg, 2, 0); // font

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		player_t *pl = players + i;
		pl->text.tic = tics;
		pl->text.font = font;
		pl->text.text = text;
		pl->text.lines = lines;
	}
}

//
// A_SetTranslation

__attribute((regparm(2),no_caller_saved_registers))
void A_SetTranslation(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t idx = actarg_magic(mo, st->arg, 0, 0); // transname
	mo->translation = render_translation + 256 * idx;
}

//
// A_SetScale

__attribute((regparm(2),no_caller_saved_registers))
void A_SetScale(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mo->scale = actarg_fixed(mo, st->arg, 0, FRACUNIT); // scale
}

//
// A_SetRenderStyle

__attribute((regparm(2),no_caller_saved_registers))
void A_SetRenderStyle(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	fixed_t alpha;

	alpha = actarg_fixed(mo, st->arg, 0, FRACUNIT); // alpha
	mo->render_style = actarg_raw(mo, st->arg, 1, RS_NORMAL); // style

	if(alpha >= FRACUNIT)
		mo->render_alpha = 255;
	else
	if(alpha <= 0)
		mo->render_alpha = 0;
	else
		mo->render_alpha = alpha >> 8;
}

//
// A_FadeOut

__attribute((regparm(2),no_caller_saved_registers))
void A_FadeOut(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	fixed_t sub;
	fixed_t alpha;

	sub = actarg_fixed(mo, st->arg, 0, FRACUNIT / 10); // reduce_amount

	if(mo->render_style != RS_TRANSLUCENT && mo->render_style != RS_ADDITIVE)
		mo->render_style = RS_TRANSLUCENT;

	alpha = mo->render_alpha << 8;
	alpha -= sub;

	if(alpha < 0)
	{
		mobj_remove(mo);
		return;
	}

	mo->render_alpha = alpha >> 8;
}

//
// A_FadeIn

__attribute((regparm(2),no_caller_saved_registers))
void A_FadeIn(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	fixed_t add;
	fixed_t alpha;

	add = actarg_fixed(mo, st->arg, 0, FRACUNIT / 10); // increase_amount

	if(mo->render_style != RS_TRANSLUCENT && mo->render_style != RS_ADDITIVE)
		mo->render_style = RS_TRANSLUCENT;

	alpha = mo->render_alpha << 8;
	alpha += add;

	if(alpha >= 255)
		mo->render_alpha = 255;
	else
		mo->render_alpha = alpha >> 8;
}

//
// A_CheckPlayerDone

__attribute((regparm(2),no_caller_saved_registers))
void A_CheckPlayerDone(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(mo->player)
		return;

	mo->tics = 1;
	mobj_remove(mo);
}

//
// A_AlertMonsters

__attribute((regparm(2),no_caller_saved_registers))
void A_AlertMonsters(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(mo->player)
	{
		// assuming use from weapon or custom inventory
		P_NoiseAlert(mo, mo);
		return;
	}

	if(!mo->target)
		return;

	if(!mo->target->player)
		return;

	P_NoiseAlert(mo->target, mo);
}

//
// A_SetAngle

static __attribute((regparm(2),no_caller_saved_registers))
void A_SetAngle(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mobj_t *mm;
	angle_t angle;

	// flags are skipped and should be zero

	mm = actarg_pointer(mo, st->arg, 2, AAPTR_DEFAULT); // ptr
	if(!mm)
		return;

	mm->angle = actarg_angle(mo, st->arg, 0); // angle
}

//
// A_SetPitch

static __attribute((regparm(2),no_caller_saved_registers))
void A_SetPitch(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mobj_t *mm;
	angle_t angle;

	// flags are skipped and should be zero

	mm = actarg_pointer(mo, st->arg, 2, AAPTR_DEFAULT); // ptr
	if(!mm)
		return;

	mm->pitch = -actarg_angle(mo, st->arg, 0); // angle
}

//
// A_ChangeFlag

static __attribute((regparm(2),no_caller_saved_registers))
void A_ChangeFlag(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t *fs;
	uint32_t bits, offs;

	offs = actarg_moflag(mo, st->arg, 0, &bits); // flagname
	if(!offs)
		return;

	fs = (void*)mo + offs;

	if(actarg_raw(mo, st->arg, 1, 0)) // value
		*fs = *fs | bits;
	else
		*fs = *fs & ~bits;
}

//
// A_ChangeVelocity

static __attribute((regparm(2),no_caller_saved_registers))
void A_ChangeVelocity(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	angle_t ang;
	fixed_t x, y;
	fixed_t ax, ay;
	uint32_t flags;

	mo = actarg_pointer(mo, st->arg, 4, AAPTR_DEFAULT); // ptr
	if(!mo)
		return;

	ax = actarg_fixed(mo, st->arg, 0, 0); // x
	ay = actarg_fixed(mo, st->arg, 1, 0); // y

	flags = actarg_raw(mo, st->arg, 3, 0); // flags

	if(flags & CVF_REPLACE)
	{
		mo->momx = 0;
		mo->momy = 0;
		mo->momz = 0;
	}

	ang = mo->angle;

	if(flags & CVF_RELATIVE)
	{
		fixed_t cc, ss;
		ang >>= ANGLETOFINESHIFT;
		ss = finesine[ang];
		cc = finecosine[ang];
		x = FixedMul(ax, cc);
		x -= FixedMul(ay, ss);
		y = FixedMul(ax, ss);
		y += FixedMul(ay, cc);
	} else
	{
		x = ax;
		y = ay;
	}

	mo->momx += x;
	mo->momy += y;
	mo->momz += actarg_fixed(mo, st->arg, 2, 0); // z
}

//
// A_ScaleVelocity

static __attribute((regparm(2),no_caller_saved_registers))
void A_ScaleVelocity(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	fixed_t scale;

	mo = actarg_pointer(mo, st->arg, 1, AAPTR_DEFAULT); // pointer
	if(!mo)
		return;

	scale = actarg_fixed(mo, st->arg, 0, 0); // scale

	mo->momx = FixedMul(mo->momx, scale);
	mo->momy = FixedMul(mo->momy, scale);
	mo->momz = FixedMul(mo->momz, scale);
}

//
// A_Stop

static __attribute((regparm(2),no_caller_saved_registers))
void A_Stop(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mo->momx = 0;
	mo->momy = 0;
	mo->momz = 0;
}

//
// A_SetTics

static __attribute((regparm(2),no_caller_saved_registers))
void A_SetTics(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mo->tics = actarg_integer(mo, st->arg, 0, 0);
}

//
// A_RearrangePointers

static __attribute((regparm(2),no_caller_saved_registers))
void A_RearrangePointers(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mobj_t *target = mo->target;
	mobj_t *master = mo->master;
	mobj_t *tracer = mo->tracer;

	if(actarg_raw(mo, st->arg, 3, 0) != PTROP_NOSAFEGUARDS) // flags; forced value
		engine_error("DECORATE", "Missing %s in '%s'.", "PTROP_NOSAFEGUARDS", "a_rearrangepointers");

	switch(actarg_raw(mo, st->arg, 0, 0)) // target
	{
		case AAPTR_TRACER:
			mo->target = tracer;
		break;
		case AAPTR_MASTER:
			mo->target = master;
		break;
		case AAPTR_NULL:
			mo->target = NULL;
		break;
	}

	switch(actarg_raw(mo, st->arg, 1, 0)) // master
	{
		case AAPTR_TARGET:
			mo->master = target;
		break;
		case AAPTR_TRACER:
			mo->master = tracer;
		break;
		case AAPTR_NULL:
			mo->master = NULL;
		break;
	}

	switch(actarg_raw(mo, st->arg, 2, 0)) // tracer
	{
		case AAPTR_TARGET:
			mo->tracer = target;
		break;
		case AAPTR_MASTER:
			mo->tracer = master;
		break;
		case AAPTR_NULL:
			mo->tracer = NULL;
		break;
	}
}

//
// A_BrainDie

static __attribute((regparm(2),no_caller_saved_registers))
void A_BrainDie(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	secretexit = 0;
	gameaction = ga_completed;
	map_start_id = 0;
}

//
// A_RaiseSelf

static __attribute((regparm(2),no_caller_saved_registers))
void A_RaiseSelf(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t ret;
	uint32_t flags;

	if(mo->player)
		return;

	if(!mo->info->state_raise)
		return;

	if(!(mo->flags & MF_CORPSE))
		return;

	if(mo->tics != -1 && !(mo->frame & FF_CANRAISE))
		return;

	flags = mo->flags;

	mo->momx = 0;
	mo->momy = 0;

	mo->height <<= 2;
	mo->flags |= MF_SOLID;
	ret = P_CheckPosition(mo, mo->x, mo->y);
	mo->flags = flags;
	mo->height >>= 2;

	if(!ret)
		return;

	S_StartSound(mo, 31);

	mobj_set_animation(mo, ANIM_RAISE);

	mo->height <<= 2;
	mo->flags = mo->info->flags;
	mo->health = mo->info->spawnhealth;
	mo->target = NULL;
}

//
// A_KeenDie

static __attribute((regparm(2),no_caller_saved_registers))
void A_KeenDie(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	for(thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		mobj_t *mt;

		if(th->function != (void*)P_MobjThinker)
			continue;

		mt = (mobj_t*)th;

		if(mt->type != mo->type)
			continue;

		if(mt->health > 0)
			return;
	}

	spec_special = 12;
	spec_arg[0] = actarg_integer(mo, st->arg, 0, 666); // tag
	spec_arg[1] = 16;
	spec_arg[2] = 0;
	spec_arg[3] = 0;
	spec_arg[4] = 0;

	spec_activate(NULL, mo, 0);
}

//
// A_Warp

static __attribute((regparm(2),no_caller_saved_registers))
void A_Warp(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mobj_t *target;
	fixed_t x, y, z;
	fixed_t bx, by, bz;
	fixed_t xx, yy;
	uint32_t flags;
	angle_t angle;

	target = actarg_pointer(mo, st->arg, 0, AAPTR_DEFAULT); // ptr_destination
	if(!target || target == mo)
		return;

	xx = actarg_fixed(mo, st->arg, 1, 0); // xofs
	yy = actarg_fixed(mo, st->arg, 2, 0); // yofs
	z = actarg_fixed(mo, st->arg, 3, 0); // zofs
	angle = actarg_angle(mo, st->arg, 4); // angle
	flags = actarg_raw(mo, st->arg, 5, 0); // flags

	if(flags & WARPF_MOVEPTR)
	{
		mobj_t *tmp = target;
		target = mo;
		mo = tmp;
	}

	bx = mo->x;
	by = mo->y;
	bz = mo->z;

	if(flags & WARPF_ABSOLUTEANGLE)
		mo->angle = angle;
	else
	if(flags & WARPF_USECALLERANGLE)
		mo->angle += angle;
	else
		mo->angle = target->angle + angle;

	if(flags & WARPF_STOP)
	{
		mo->momx = 0;
		mo->momy = 0;
		mo->momz = 0;
	} else
	if(flags & WARPF_COPYVELOCITY)
	{
		mo->momx = target->momx;
		mo->momy = target->momy;
		mo->momz = target->momz;
	}

	if(flags & WARPF_COPYPITCH)
		mo->pitch = target->pitch;

	if(flags & WARPF_ABSOLUTEPOSITION)
	{
		x = xx;
		y = yy;
	} else
	{
		if(flags & WARPF_ABSOLUTEOFFSET)
		{
			x = target->x + xx;
			y = target->y + yy;
		} else
		{
			fixed_t cc, ss;
			angle_t ang = mo->angle >> ANGLETOFINESHIFT;
			ss = finesine[ang];
			cc = finecosine[ang];
			x = FixedMul(xx, cc);
			x += FixedMul(yy, ss);
			y = FixedMul(xx, ss);
			y -= FixedMul(yy, cc);
			x += target->x;
			y += target->y;
		}
	}

	P_UnsetThingPosition(mo);
	mo->x = x;
	mo->y = y;

	if(flags & WARPF_ABSOLUTEPOSITION)
		mo->z = z;
	else
		mo->z = target->z + z;

	P_SetThingPosition(mo);

	if(flags & WARPF_NOCHECKPOSITION)
		return;

	if(P_TryMove(mo, mo->x, mo->y))
		return;

	P_UnsetThingPosition(mo);
	mo->x = bx;
	mo->y = by;
	mo->z = bz;
	P_SetThingPosition(mo);
}

//
// A_SetArg

static __attribute((regparm(2),no_caller_saved_registers))
void A_SetArg(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t idx;

	idx = actarg_integer(mo, st->arg, 0, 0); // position
	if(idx > 4)
		return;

	mo->special.arg[idx] = actarg_integer(mo, st->arg, 1, 0); // value
}

//
// A_DamageTarget

static void do_act_damage(mobj_t *mo, mobj_t *target, void *data)
{
	uint32_t damage = actarg_integer(mo, data, 0, 0);
	damage |= DAMAGE_SKIP_ARMOR;
	mobj_damage(target, mo, mo, damage, 0);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_DamageTarget(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->target)
		return;
	do_act_damage(mo, mo->target, st->arg);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_DamageTracer(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->tracer)
		return;
	do_act_damage(mo, mo->tracer, st->arg);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_DamageMaster(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	if(!mo->master)
		return;
	do_act_damage(mo, mo->master, st->arg);
}

//
// A_SetHealth

static __attribute((regparm(2),no_caller_saved_registers))
void A_SetHealth(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t amount;

	mo = actarg_pointer(mo, st->arg, 1, AAPTR_DEFAULT); // pointer
	if(!mo)
		return;

	amount = actarg_integer(mo, st->arg, 0, 0); // health
	if(!amount)
		amount++;

	mo->health = amount;
	if(mo->player)
	{
		mo->player->health = amount;
		mo->player->mo->health = amount;
	}
}

//
// A_Explode
__attribute((regparm(2),no_caller_saved_registers))
void A_Explode(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	int32_t xl, xh, yl, yh;
	fixed_t	dist;
	uint32_t alert;

	bombdmgtype = -1;

	bombdamage = actarg_integer(mo, st->arg, 0, 128); // damage
	bombdist = actarg_fixed(mo, st->arg, 1, 128 * FRACUNIT); // distance
	bombflags = actarg_raw(mo, st->arg, 2, XF_HURTSOURCE); // flags
	alert = actarg_raw(mo, st->arg, 3, 0); // alert
	bombfist = actarg_fixed(mo, st->arg, 4, 0); // fulldamagedistance

	dist = bombdist + MAXRADIUS;
	yh = (mo->y + dist - bmaporgy) >> MAPBLOCKSHIFT;
	yl = (mo->y - dist - bmaporgy) >> MAPBLOCKSHIFT;
	xh = (mo->x + dist - bmaporgx) >> MAPBLOCKSHIFT;
	xl = (mo->x - dist - bmaporgx) >> MAPBLOCKSHIFT;

	bombspot = mo;

	if(bombfist > bombdist)
		bombfist = bombdist;

	if(bombflags & XF_NOTMISSILE)
		bombsource = mo;
	else
		bombsource = mo->target;

	for(int32_t y = yl; y <= yh; y++)
		for(int32_t x = xl; x <= xh; x++)
			P_BlockThingsIterator(x, y, PIT_Explode);

	if(alert && bombsource && bombsource->player)
		P_NoiseAlert(bombsource, mo);

	if(bombflags & XF_NOSPLASH)
		return;

	terrain_explosion_splash(mo, bombdist);
}

//
// A_Jump

static __attribute((regparm(2),no_caller_saved_registers))
void A_Jump(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint16_t *argoffs;
	uint32_t idx;
	uint32_t next, extra;

	next = actarg_integer(mo, st->arg, 0, 0); // chance

	if(!next)
		return;

	if(next < 256 && P_Random() >= next)
		return;

	argoffs = st->arg;
	next = *argoffs - 1;

	idx = 1;
	if(next > 1)
		idx += P_Random() % next;

	next = actarg_state(mo, st->arg, idx, &extra); // offset/state

	stfunc(mo, next, extra);
}

//
// A_JumpIf

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIf(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t next, extra;

	next = actarg_raw(mo, st->arg, 0, 0); // expression
	if(!next)
		return;

	next = actarg_state(mo, st->arg, 1, &extra); // offset/state

	stfunc(mo, next, extra);
}

//
// A_JumpIfInventory

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfInventory(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mobjinfo_t *info;
	uint32_t now, check, limit;
	uint32_t next, extra;
	mobj_t *targ;

	next = actarg_magic(mo, st->arg, 0, 0); // inventorytype
	info = mobjinfo + next;

	if(info->extra_type == ETYPE_POWERUP_BASE)
	{
		if(!mo->player)
			return;
		if(!mo->player->powers[info->powerup.type])
			return;
		goto do_jump;
	}

	if(!inventory_is_valid(info))
		return;

	targ = actarg_pointer(mo, st->arg, 3, AAPTR_DEFAULT); // owner
	if(!targ)
		return;

	extra = actarg_integer(mo, st->arg, 1, 0); // amount

	now = inventory_check(targ, next);

	if(	info->extra_type == ETYPE_AMMO &&
		targ->player &&
		targ->player->backpack
	)
		limit = info->ammo.max_count;
	else
		limit = info->inventory.max_count;

	if(extra)
		check = extra;
	else
		check = limit;

	if(now < check && check <= limit)
		return;

do_jump:
	next = actarg_state(mo, st->arg, 2, &extra); // offset/state

	stfunc(mo, next, extra);
}

//
// A_JumpIfHealthLower

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfHealthLower(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t next, extra;
	int32_t check;
	mobj_t *mm;

	mm = actarg_pointer(mo, st->arg, 2, AAPTR_DEFAULT); // pointer
	if(!mm)
		return;

	check = actarg_integer(mo, st->arg, 0, 0); // health

	if(mm->health >= check)
		return;

	next = actarg_state(mo, st->arg, 1, &extra); // offset/state

	stfunc(mo, next, extra);
}

//
// A_JumpIf*Closer

static void do_dist_check(mobj_t *mo, mobj_t *target, void *data, stfunc_t stfunc)
{
	uint32_t next, extra;
	fixed_t range;

	range = actarg_fixed(mo, data, 0, 0); // distance
	extra = actarg_raw(mo, data, 2, 0); // noz

	if(!mobj_range_check(mo, target, range, !extra))
		return;

	next = actarg_state(mo, data, 1, &extra); // offset/state

	stfunc(mo, next, extra);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfCloser(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	// TODO: handle player weapon or inventory
	if(mo->player)
		return;

	do_dist_check(mo, mo->target, st->arg, stfunc);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfTracerCloser(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	do_dist_check(mo, mo->tracer, st->arg, stfunc);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfMasterCloser(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	do_dist_check(mo, mo->master, st->arg, stfunc);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfTargetInsideMeleeRange(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t next, extra;

	if(!mobj_check_melee_range(mo))
		return;

	next = actarg_state(mo, st->arg, 0, &extra); // offset/state

	stfunc(mo, next, extra);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfTargetOutsideMeleeRange(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t next, extra;

	if(mobj_check_melee_range(mo))
		return;

	next = actarg_state(mo, st->arg, 0, &extra); // offset/state

	stfunc(mo, next, extra);
}

//
// A_CheckFloor

static __attribute((regparm(2),no_caller_saved_registers))
void A_CheckFloor(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t next, extra;

	if(mo->z > mo->floorz)
		return;

	next = actarg_state(mo, st->arg, 0, &extra); // offset/state

	stfunc(mo, next, extra);
}

//
// A_CheckFlag

static __attribute((regparm(2),no_caller_saved_registers))
void A_CheckFlag(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t next, extra;
	uint32_t *fs;
	uint32_t bits, offs;
	mobj_t *mm;

	mm = actarg_pointer(mo, st->arg, 2, AAPTR_DEFAULT); // check_pointer
	if(!mm)
		return;

	offs = actarg_moflag(mo, st->arg, 0, &bits); // flagname
	if(!offs)
		return;

	fs = (void*)mm + offs;

	if(!(*fs & bits))
		return;

	next = actarg_state(mo, st->arg, 1, &extra); // offset/state

	stfunc(mo, next, extra);
}

//
// A_MonsterRefire

static __attribute((regparm(2),no_caller_saved_registers))
void A_MonsterRefire(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t next, extra;

	next = actarg_integer(mo, st->arg, 0, 0); // chancecontinue

	if(P_Random() < next)
		return;

	if(	mo->target &&
		mo->target->health > 0 &&
		P_CheckSight(mo, mo->target)
	)
		return;

	next = actarg_state(mo, st->arg, 1, &extra); // offset/state

	stfunc(mo, next, extra);
}

//
// A_CountdownArg

static __attribute((regparm(2),no_caller_saved_registers))
void A_CountdownArg(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t idx;

	idx = actarg_integer(mo, st->arg, 0, 0); // arg

	if(idx > 4)
		return;

	if(mo->special.arg[idx]--)
		return;

	if(mo->flags & MF_MISSILE)
	{
		mobj_hit_thing = NULL;
		mobj_explode_missile(mo);
		return;
	}

	if(mo->flags & MF_SHOOTABLE)
	{
		mobj_damage(mo, NULL, NULL, 1000002, NULL);
		return;
	}

	stfunc(mo, 0xFFFFFFFF, STATE_SET_ANIMATION | ANIM_DEATH);
}

//
// chunks

static __attribute((regparm(2),no_caller_saved_registers))
void A_Burst(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t type;

	if(mo->player)
		engine_error("DECORATE", "A_Burst with player connected!");

	type = actarg_magic(mo, st->arg, 0, 0); // classname

	shatter_spawn(mo, type);

	// hide, remove later - keep sound playing
	mo->render_style = RS_INVISIBLE;
	mo->state = states + STATE_UNKNOWN_ITEM;
	mo->tics = 35 * 4;

	P_UnsetThingPosition(mo);

	mo->flags &= ~(MF_SOLID | MF_SHOOTABLE);
	mo->flags |= MF_NOBLOCKMAP | MF_NOSECTOR;
	mo->special.tid = 0;
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_SkullPop(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t type;
	mobj_t *th;

	if(!mo->player)
		return;

	type = actarg_magic(mo, st->arg, 0, 0); // classname

	if(mo->player->attacker == mo)
		mo->player->attacker = NULL;

	mo->player->viewheight = 6 * FRACUNIT;

	th = P_SpawnMobj(mo->x, mo->y, mo->z + mo->info->player.view_height, type);
	th->momx = (P_Random() - P_Random()) << 9;
	th->momy = (P_Random() - P_Random()) << 9;
	th->momz = FRACUNIT*3 + (P_Random() << 6);
	th->angle = mo->angle;
	th->player = mo->player;
	th->player->mo = th;
	th->player->camera = th;
	th->target = mo->target;
	th->inventory = mo->inventory;
	mo->player = NULL;
	mo->inventory = NULL;
}

//
// freeze death

static __attribute((regparm(2),no_caller_saved_registers))
void A_FreezeDeath(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mo->height <<= 2;
	mo->flags |= MF_NOBLOOD | MF_SHOOTABLE | MF_SOLID | MF_SLIDE;
	mo->flags1 |= MF1_PUSHABLE | MF1_TELESTOMP;
	mo->flags2 |= MF2_ICECORPSE;
	mo->iflags |= MFI_CRASHED;
	mo->tics = 75 + P_Random() + P_Random();
	mo->render_style = RS_NORMAL;

	if(!(mo->flags1 & MF1_DONTFALL))
		mo->flags &= ~MF_NOGRAVITY;

	S_StartSound(mo, SFX_FREEZE);

	if(mo->player)
	{
		mo->player->bonuscount = 0;
		mo->player->damagecount = 0;
		mo->player->flags |= PF_IS_FROZEN;
	}
}

__attribute((regparm(2),no_caller_saved_registers))
void A_GenericFreezeDeath(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	A_FreezeDeath(mo, st, stfunc);
	mo->frame &= ~FF_FULLBRIGHT;
	mo->translation = render_translation + TRANSLATION_ICE * 256;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_FreezeDeathChunks(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mobj_t *th;

	if(!(mo->iflags & MFI_SHATTERING) && (mo->momx || mo->momy || mo->momz))
	{
		mo->tics = 105;
		return;
	}

	S_StartSound(mo, SFX_ICEBREAK);

	// drop items
	A_NoBlocking(mo, st, stfunc);

	if(mo->player)
	{
		if(mo->player->attacker == mo)
			mo->player->attacker = NULL;

		mo->player->viewheight = 6 * FRACUNIT;

		th = P_SpawnMobj(mo->x, mo->y, mo->z + mo->info->player.view_height, MOBJ_IDX_ICE_CHUNK_HEAD);
		th->momx = (P_Random() - P_Random()) << 9;
		th->momy = (P_Random() - P_Random()) << 9;
		th->momz = FRACUNIT*2 + (P_Random() << 6);
		th->angle = mo->angle;
		th->player = mo->player;
		th->player->mo = th;
		th->player->camera = th;
		th->target = mo->target;
		th->inventory = mo->inventory;
		mo->player = NULL;
		mo->inventory = NULL;
	}

	shatter_spawn(mo, MOBJ_IDX_ICE_CHUNK);

	// hide, remove later - keep sound playing
	mo->render_style = RS_INVISIBLE;
	mo->state = states + STATE_UNKNOWN_ITEM;
	mo->tics = 35 * 4;

	P_UnsetThingPosition(mo);

	mo->flags &= ~(MF_SOLID | MF_SHOOTABLE);
	mo->flags |= MF_NOBLOCKMAP | MF_NOSECTOR;
	mo->special.tid = 0;
}

__attribute((regparm(2),no_caller_saved_registers))
void A_IceSetTics(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	mo->tics = 70 + (P_Random() & 63);
	switch(mo->subsector->sector->extra->damage.type & 0x7F)
	{
		case DAMAGE_ICE:
			mo->tics <<= 1;
		break;
		case DAMAGE_FIRE:
			mo->tics >>= 2;
		break;
	}
}

//
// activate line special

static __attribute((regparm(2),no_caller_saved_registers))
void A_LineSpecial(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	int32_t arg[5];
	uint32_t spec;
	uint32_t succ;
	line_t *line;
	mobj_t *act;

	arg[0] = spec_arg[0];
	arg[1] = spec_arg[1];
	arg[2] = spec_arg[2];
	arg[3] = spec_arg[3];
	arg[4] = spec_arg[4];
	spec = spec_special;
	succ = spec_success;
	line = spec_line;
	act = spec_activator;

	spec_arg[0] = actarg_integer(mo, st->arg, 0, 0);
	spec_arg[1] = actarg_integer(mo, st->arg, 1, 0);
	spec_arg[2] = actarg_integer(mo, st->arg, 2, 0);
	spec_arg[3] = actarg_integer(mo, st->arg, 3, 0);
	spec_arg[4] = actarg_integer(mo, st->arg, 4, 0);
	spec_special = ((uint16_t*)st->arg)[6];
	spec_activate(NULL, mo, 0);

	spec_arg[0] = arg[0];
	spec_arg[1] = arg[1];
	spec_arg[2] = arg[2];
	spec_arg[3] = arg[3];
	spec_arg[4] = arg[4];
	spec_special = spec;
	spec_success = succ;
	spec_line = line;
	spec_activator = act;
}

//
// parser

static uint32_t parse_value(uint8_t *kw, int32_t *val)
{
	uint_fast8_t have_dot = 0;
	const uint8_t *ptr = kw;
	const math_var_t *var;
	const math_con_t *con;

	if(!kw[1])
	{
		if(kw[0] == '|')
			return MATH_OR;
		if(kw[0] == '^')
			return MATH_XOR;
		if(kw[0] == '&')
			return MATH_AND;
		if(kw[0] == '<')
			return MATH_LESS;
		if(kw[0] == '>')
			return MATH_GREAT;
		if(kw[0] == '+')
			return MATH_ADD;
		if(kw[0] == '-')
			return MATH_SUB;
		if(kw[0] == '*')
			return MATH_MUL;
		if(kw[0] == '/')
			return MATH_DIV;
		if(kw[0] == '%')
			return MATH_MOD;
	}

	if(kw[1] == '=')
	{
		if(kw[0] == '<')
			return MATH_LEQ;
		if(kw[0] == '>')
			return MATH_GEQ;
		if(kw[0] == '=')
			return MATH_EQ;
		if(kw[0] == '!')
			return MATH_NEQ;
	}

	if(kw[0] == '|' && kw[1] == '|')
		return MATH_LOR;
	if(kw[0] == '&' && kw[1] == '&')
		return MATH_LAND;

	while(*ptr)
	{
		if(*ptr == '.')
		{
			if(have_dot)
				// invalid
				goto try_text;
			have_dot = 1;
		} else
		if(*ptr < '0' || *ptr > '9')
			// invalid
			goto try_text;
		ptr++;
	}

	if(tp_parse_fixed(kw, val, MATHFRAC))
		return MATH_INVALID;

	return MATH_VALUE;

try_text:
	// handle non-numeric stuff
	strupr(kw);

	// the only function call (right now)
	if(!strcmp("RANDOM", kw))
	{
		uint32_t v0, v1;

		if(!tp_must_get("("))
			return MATH_INVALID;

		kw = tp_get_keyword();
		if(!kw)
			return MATH_INVALID;
		if(doom_sscanf(kw, "%u", &v0) != 1 || v0 > 65535)
			engine_error("DECORATE", "Unable to parse number '%s' for action '%s' in '%s'!", kw, action_name, parse_actor_name);

		if(!tp_must_get(","))
			return MATH_INVALID;

		kw = tp_get_keyword();
		if(!kw)
			return MATH_INVALID;
		if(doom_sscanf(kw, "%u", &v1) != 1 || v1 > 65535)
			engine_error("DECORATE", "Unable to parse number '%s' for action '%s' in '%s'!", kw, action_name, parse_actor_name);

		if(!tp_must_get(")"))
			return MATH_INVALID;

		*val = v0 | (v1 << 16);
		return MATH_RANDOM;
	}

	// check constants
	con = math_constant;
	while(con->name)
	{
		if(!strcmp(con->name, kw))
		{
			*val = con->value;
			return MATH_VALUE;
		}
		con++;
	}

	// check variables
	var = math_variable;
	while(var->name)
	{
		if(!strcmp(var->name, kw))
		{
			uint32_t ret;
			ret = var - math_variable;
			ret += MATH_VARIABLE;
			return ret;
		}
		var++;
	}

	return MATH_INVALID;
}

static uint32_t handle_string(uint8_t *kw)
{
	uint32_t len, line;
	uint8_t *ptr = kw;
	act_str_t *as;

	len = 0;
	line = 1;
	while(*ptr)
	{
		if(*ptr == '\n')
			line++;
		len++;
		ptr++;
	}

	as = dec_es_alloc(sizeof(uint32_t) + len + 1);
	as->value = len;
	as->lines = line;
	strcpy(as->text, kw);

	return (void*)as - parse_action_arg;
}

static uint32_t handle_state(uint8_t *kw)
{
	act_state_t *st;
	const dec_anim_t *anim;

	strlwr(kw);

	st = dec_es_alloc(sizeof(act_state_t));

	// find animation
	anim = dec_find_animation(kw);
	if(anim)
	{
		// actual animation
		st->extra = STATE_SET_ANIMATION | anim->idx;
		st->next = 0xFFFFFFFF;
	} else
	{
		// custom state
		st->extra = STATE_SET_CUSTOM;
		st->next = tp_hash32(kw);
	}

	return (void*)st - parse_action_arg;
}

static uint32_t handle_mobjtype(uint8_t *kw)
{
	// negative return = fail
	return mobj_check_type(tp_hash64(kw));
}

static uint32_t handle_mobjflag(uint8_t *kw)
{
	act_moflag_t *arf;
	const dec_flag_t *flag;
	uint32_t offset;

	strlwr(kw);

	offset = offsetof(mobj_t, flags);
	flag = find_mobj_flag(kw, mobj_flags0);
	if(!flag)
	{
		offset = offsetof(mobj_t, flags1);
		flag = find_mobj_flag(kw, mobj_flags1);
		if(!flag)
		{
			offset = offsetof(mobj_t, flags2);
			flag = find_mobj_flag(kw, mobj_flags2);
			if(!flag)
				return -1;
		}
	} else
	{
		if(flag->bits & (MF_NOBLOCKMAP | MF_NOSECTOR))
			return -1;
	}

	arf = dec_es_alloc(sizeof(act_moflag_t));
	arf->bits = flag->bits;
	arf->offset = offset;

	return (void*)arf - parse_action_arg;
}

static uint32_t handle_dmgtype(uint8_t *kw)
{
	strlwr(kw);
	return dec_get_custom_damage(kw, NULL);
}

static uint32_t handle_translation(uint8_t *kw)
{
	strlwr(kw);
	return r_translation_find(kw);
}

static uint32_t handle_sound(uint8_t *kw)
{
	return sfx_by_alias(tp_hash64(kw));
}

static uint32_t handle_lump(uint8_t *kw)
{
	return wad_get_lump(kw);
}

void *act_handle_arg(int32_t idx)
{
	// this implementation uses unaligned 32bit access
	uint8_t *kw;
	int32_t value;
	uint32_t type;
	uint8_t buffer[256];
	uint8_t *ptr = buffer;
	uint32_t last_type = MATH_INVALID;
	uint16_t *argoffs;
	uint8_t str_type = STR_NONE;

	kw = tp_get_keyword();
	if(!kw)
		return kw;

	if(idx >= 0)
	{
		// action arg
		argoffs = parse_action_arg;
		argoffs += 1 + idx;

		if(action_parsed->func == (void*)A_Jump)
		{
			// special case; only first argument is numeric
			if(idx)
				str_type = STR_STATE;
		} else
		for(uint32_t i = 0; i < MAX_STR_ARG; i++)
		{
			register uint8_t tmp = action_parsed->strarg[i];
			if((tmp >> 4) == idx)
			{
				str_type = tmp & 15;
				break;
			}
		}
	} else
	{
		// decorate property
		parse_action_arg = dec_es_alloc(2 * sizeof(uint16_t));
		argoffs = parse_action_arg;
		*argoffs++ = 1; // fake 1 argument
	}

	if(tp_is_string)
	{
		// strings are special
		switch(str_type)
		{
			case STR_NORMAL:
				type = handle_string(kw);
			break;
			case STR_STATE:
				type = handle_state(kw);
			break;
			case STR_MOBJ_TYPE:
				type = handle_mobjtype(kw);
			break;
			case STR_MOBJ_FLAG:
				type = handle_mobjflag(kw);
			break;
			case STR_DAMAGE_TYPE:
				type = handle_dmgtype(kw);
			break;
			case STR_TRANSLATION:
				type = handle_translation(kw);
			break;
			case STR_SOUND:
				type = handle_sound(kw);
			break;
			case STR_LUMP:
				type = handle_lump(kw);
			break;
			default:
				// this can NOT be a string
				return NULL;
		}

		if(type > ARG_MAGIC_MASK)
			return NULL;
		type |= ARG_DEF_MAGIC;

		*argoffs = type;

		return tp_get_keyword();
	} else
	if(str_type > STR_STATE)
		// this HAS to be a string
		return NULL;

	while(1)
	{
		// "tokenize"
		type = parse_value(kw, &value);
		if(type == MATH_INVALID)
			return NULL;

		if(type == MATH_SUB && last_type < MATH_VALUE)
		{
			// add negation if last entry is operator
			if(ptr + 6 > buffer + sizeof(buffer))
				// overflow
				return NULL;
			type = MATH_NEG;
			// zero
			*ptr++ = MATH_VALUE;
			*((int32_t*)ptr) = 0;
			ptr += sizeof(int32_t);
			// priority minus
			*ptr++ = type;
		} else
		{
			if(ptr + 5 > buffer + sizeof(buffer))
				// overflow
				return NULL;

			*ptr++ = type;

			if(type >= MATH_VALUE && !(type & MATH_VARIABLE))
			{
				*((int32_t*)ptr) = value;
				ptr += sizeof(int32_t);
			}
		}

		kw = tp_get_keyword();
		if(!kw || kw[0] == ',' || kw[0] == ')')
			break;

		last_type = type;

		if(tp_is_string)
			return NULL;
	}

	// validate equation
	// this will try to replay all math operations
	// since mobj does not exists, all variables are replaced with dummy value
	// equation might be simplified to single result (value, variable, random())
	parse_math_res = MATH_VALUE;
	value = math_calculate(NULL, buffer, ptr - buffer);

//	doom_printf("result is %d (%d)\n", value, parse_math_res);

	if(parse_math_res < 0)
		return NULL;

	if(parse_math_res & MATH_VARIABLE)
	{
		// simplified to single variable
		type = ARG_DEF_VAR | (parse_math_res & MATH_VAR_MASK);
	} else
	if(parse_math_res >= MATH_VALUE)
	{
		// simplified to value or random()
		int32_t *val = dec_es_alloc(sizeof(int32_t));
		*val = value;
		type = (void*)val - parse_action_arg;
		type |= (parse_math_res & 0x0F) << 11;
	} else
	{
		// unable simplify
		uint16_t *val;
		type = ptr - buffer;
		val = dec_es_alloc(sizeof(uint16_t) + type);
		*val = type;
		memcpy(val + 1, buffer, type);
		type = (void*)val - parse_action_arg;
		type |= ARG_DEF_MATH;
	}

	*argoffs = type;

	if(idx < 0)
	{
		if(kw[0] != ')')
			return NULL;
		return parse_action_arg;
	}

	return kw;
}

uint8_t *action_parser(uint8_t *name)
{
	// Argument layout in memory:
	// uint16_t argument_count;
	// uint16_t data_offset_and_type[max_argument_count];
	// uint8_t buffer_with_data[];
	// offset is relative to &argument_count
	uint8_t *kw;
	const dec_action_t *act;
	const dec_linespec_t *spec;

	// find action
	act = mobj_action;
	while(act->name)
	{
		if(!strcmp(act->name, name))
			break;
		act++;
	}

	if(!act->name)
	{
		// try line specials
		uint64_t alias = tp_hash32(name);

		spec = special_action;
		while(spec->special)
		{
			if(spec->alias == alias)
				break;
			spec++;
		}

		if(!spec->special)
			// not found
			engine_error("DECORATE", "Unknown action '%s' in '%s'!", name, parse_actor_name);

		act = &line_action;
	}

	// next keyword
	kw = tp_get_keyword();
	if(!kw)
		return NULL;

	// action function
	parse_action_func = act->func;
	parse_action_arg = NULL;
	parse_arg_idx = 0;
	action_name = kw;
	action_parsed = act;

	if(kw[0] == '(')
	{
		while(1)
		{
			if(!parse_action_arg)
			{
				uint32_t count;

				if(act == &line_action)
					count = 6;
				else
					count = act->maxarg;

				count++;

				parse_action_arg = dec_es_alloc(count * sizeof(uint16_t));
				if(act == &line_action)
				{
					// line special hack
					uint16_t *ac = parse_action_arg;
					ac[6] = spec->special; // no extra markings
				}
			}

			kw = act_handle_arg(parse_arg_idx);
			if(!kw || (kw[0] != ',' && kw[0] != ')'))
				engine_error("DECORATE", "Failed to parse arg[%d] for action '%s' in '%s'!", parse_arg_idx, name, parse_actor_name);

			parse_arg_idx++;

			if(kw[0] == ')')
			{
				uint16_t *ac = parse_action_arg;
				*ac = parse_arg_idx;
				kw = tp_get_keyword();
				break;
			}

			if(parse_arg_idx >= act->maxarg)
				engine_error("DECORATE", "Too many arguments for action '%s' in '%s'!", name, parse_actor_name);
		}
	} else
	if(act == &line_action)
	{
		// empty line special hack
		uint16_t *ac;
		parse_action_arg = dec_es_alloc(7 * sizeof(uint16_t));
		ac = parse_action_arg;
		ac[0] = 0;
		ac[6] = spec->special;
	}

	if(parse_arg_idx < act->minarg)
		engine_error("DECORATE", "Missing arg[%d] for action '%s' in '%s'!", parse_arg_idx, name, parse_actor_name);

	return kw;
}

//
// table of actions

static const dec_action_t mobj_action[] =
{
	// weapon
	{"a_lower", A_Lower, 1, 0}, // lowerspeed
	{"a_raise", A_Raise, 1, 0}, // raisespeed
	{"a_gunflash", A_GunFlash, 2, -1, { STRARG(0,STR_STATE) }}, // "flashlabel", flags
	{"a_checkreload", A_CheckReload},
	{"a_light0", A_Light0},
	{"a_light1", A_Light1},
	{"a_light2", A_Light2},
	{"a_weaponready", A_WeaponReady, 1, -1}, // flags
	{"a_refire", A_ReFire, 1, -1, { STRARG(0,STR_STATE) }}, // "state"
	// sound
	{"a_pain", A_Pain},
	{"a_scream", A_Scream},
	{"a_xscream", A_XScream},
	{"a_playerscream", A_PlayerScream},
	{"a_activesound", A_ActiveSound},
	{"a_brainpain", A_BrainPain},
	{"a_startsound", A_StartSound, 5, 1, { STRARG(0,STR_SOUND) }}, // "whattoplay", slot, flags, volume, attenuation
	// basic control
	{"a_facetarget", A_FaceTarget},
	{"a_facetracer", A_FaceTracer},
	{"a_facemaster", A_FaceMaster},
	{"a_noblocking", A_NoBlocking},
	// "AI"
	{"a_look", A_Look},
	{"a_chase", A_Chase},
	{"a_vilechase", A_VileChase},
	// enemy attack
	{"a_spawnprojectile", A_SpawnProjectile, 7, 1, { STRARG(0,STR_MOBJ_TYPE) }}, // "missiletype", spawnheight, spawnofs_xy, angle, flags, pitch, ptr
	{"a_custombulletattack", A_CustomBulletAttack, 7, 4, { STRARG(4,STR_MOBJ_TYPE) }}, // horz_spread, vert_spread, numbullets, damageperbullet, "pufftype", range, flags
	{"a_custommeleeattack", A_CustomMeleeAttack, 5, 1, { STRARG(1,STR_SOUND), STRARG(2,STR_SOUND), STRARG(3,STR_DAMAGE_TYPE) }}, // damage, "meleesound", "misssound", "damagetype", bleed
	{"a_viletarget", A_VileTarget, 1, 0, { STRARG(0,STR_MOBJ_TYPE) }}, // "type"
	{"a_vileattack", A_VileAttack, 7, 0, { STRARG(0,STR_SOUND), STRARG(5,STR_DAMAGE_TYPE) }}, // "sound", initialdamage, blastdamage, blastradius, thrustfactor, "damagetype", flags
	// player attack
	{"a_fireprojectile", A_FireProjectile, 7, 1, { STRARG(0,STR_MOBJ_TYPE) }}, // "missiletype", angle, useammo, spawnofs_xy, spawnheight, flags, pitch
	{"a_firebullets", A_FireBullets, 7, 4, { STRARG(4,STR_MOBJ_TYPE) }}, // spread_horz, spread_vert, numbullets, damage, "pufftype", flags, range
	{"a_custompunch", A_CustomPunch, 5, 1, { STRARG(3,STR_MOBJ_TYPE) }}, // damage, norandom, flags, "pufftype", range
	// other attack
	{"a_bfgspray", A_BFGSpray, 7, -1, { STRARG(0,STR_MOBJ_TYPE) }}, // "spraytype", numrays, damagecnt, ang, distance, vrange, defdamage
	{"a_seekermissile", A_SeekerMissile, 5, 2}, // threshold, maxturnangle, flags, chance, distance
	// spawn
	{"a_spawnitemex", A_SpawnItemEx, 11, 1, { STRARG(0,STR_MOBJ_TYPE) }}, // "missile", xofs, yofs, zofs, xvel, yvel, zvel, angle, flags, failchance, tid
	{"a_dropitem", A_DropItem, 3, 1, { STRARG(0,STR_MOBJ_TYPE) }}, // "item", dropamount, chance
	// chunks
	{"a_burst", A_Burst, 1, 1, { STRARG(0,STR_MOBJ_TYPE) }}, // "classname"
	{"a_skullpop", A_SkullPop, 1, 1, { STRARG(0,STR_MOBJ_TYPE) }}, // "classname"
	// freeze death
	{"a_freezedeath", A_FreezeDeath},
	{"a_genericfreezedeath", A_GenericFreezeDeath},
	{"a_freezedeathchunks", A_FreezeDeathChunks},
	// render
	{"a_settranslation", A_SetTranslation, 1, 1, { STRARG(0,STR_TRANSLATION) }}, // "transname"
	{"a_setscale", A_SetScale, 1, 1}, // scale
	{"a_setrenderstyle", A_SetRenderStyle, 2, 2}, // alpha, style
	{"a_fadeout", A_FadeOut, 1, 1}, // reduce_amount
	{"a_fadein", A_FadeIn, 1, 1}, // increase_amount
	// misc
	{"a_checkplayerdone", A_CheckPlayerDone},
	{"a_alertmonsters", A_AlertMonsters},
	{"a_setangle", A_SetAngle, 3, 1}, // angle, flags, ptr
	{"a_setpitch", A_SetPitch, 3, 1}, // pitch, flags, ptr
	{"a_changeflag", A_ChangeFlag, 2, 2, { STRARG(0,STR_MOBJ_FLAG) }}, // "flagname", value
	{"a_changevelocity", A_ChangeVelocity, 5, 3}, // x, y, z, flags, ptr
	{"a_scalevelocity", A_ScaleVelocity, 2, 1}, // scale, pointer
	{"a_stop", A_Stop},
	{"a_settics", A_SetTics, 1, 1}, // duration
	{"a_rearrangepointers", A_RearrangePointers, 4, 4}, // target, master, tracer, flags
	{"a_brainawake", doom_A_BrainAwake},
	{"a_brainspit", doom_A_BrainSpit},
	{"a_spawnfly", doom_A_SpawnFly},
	{"a_braindie", A_BrainDie},
	{"a_raiseself", A_RaiseSelf},
	{"a_keendie", A_KeenDie, 1, -1}, // tag
	{"a_warp", A_Warp, 6, 1}, // ptr_destination, xofs, yofs, zofs, angle, flags
	{"a_setarg", A_SetArg, 2, 2}, // position, value
	// damage
	{"a_damagetarget", A_DamageTarget, 2, 1, { STRARG(1,STR_DAMAGE_TYPE) }}, // amount, "damagetype"
	{"a_damagetracer", A_DamageTracer, 2, 1, { STRARG(1,STR_DAMAGE_TYPE) }}, // amount, "damagetype"
	{"a_damagemaster", A_DamageMaster, 2, 1, { STRARG(1,STR_DAMAGE_TYPE) }}, // amount, "damagetype"
	{"a_explode", A_Explode, 5, 1}, // damage, distance, flags, alert, fulldamagedistance
	// health
	{"a_sethealth", A_SetHealth, 2, 1}, // health, pointer
	// inventory
	{"a_giveinventory", A_GiveInventory, 3, 1, { STRARG(0,STR_MOBJ_TYPE) }}, // "type", amount, giveto
	{"a_takeinventory", A_TakeInventory, 4, 1, { STRARG(0,STR_MOBJ_TYPE) }}, // "type", amount, flags, takefrom
	{"a_selectweapon", A_SelectWeapon, 1, 1, { STRARG(0,STR_MOBJ_TYPE) }}, // "whichweapon"
	// text
	{"a_print", A_Print, 3, 1, { STRARG(0,STR_NORMAL), STRARG(2,STR_LUMP) }}, // "text", time, "fontname"
	{"a_printbold", A_PrintBold, 3, 1, { STRARG(0,STR_NORMAL), STRARG(2,STR_LUMP) }}, // "text", time, "fontname"
	// jumps
	{"a_jump", A_Jump, 10, 2}, // chance, "offset/state", ... // TODO: somehow make dynamic
	{"a_jumpif", A_JumpIf, 2, 2, { STRARG(1,STR_STATE) }}, // expression, "offset/state"
	{"a_jumpifinventory", A_JumpIfInventory, 4, 3, { STRARG(0,STR_MOBJ_TYPE), STRARG(2,STR_STATE) }}, // "inventorytype", amount, "offset/state", owner
	{"a_jumpifhealthlower", A_JumpIfHealthLower, 3, 2, { STRARG(1,STR_STATE) }}, //  health, "offset/state", pointer
	{"a_jumpifcloser", A_JumpIfCloser, 3, 2, { STRARG(1,STR_STATE) }}, // distance, "offset/state", noz
	{"a_jumpiftracercloser", A_JumpIfTracerCloser, 3, 2, { STRARG(1,STR_STATE) }}, // distance, "offset/state", noz
	{"a_jumpifmastercloser", A_JumpIfMasterCloser, 3, 2, { STRARG(1,STR_STATE) }}, // distance, "offset/state", noz
	{"a_jumpiftargetinsidemeleerange", A_JumpIfTargetInsideMeleeRange, 1, 1, { STRARG(0,STR_STATE) }}, // "offset/state"
	{"a_jumpiftargetoutsidemeleerange", A_JumpIfTargetOutsideMeleeRange, 1, -1, { STRARG(0,STR_STATE) }}, // "offset/state"
	{"a_checkfloor", A_CheckFloor, 1, 1, { STRARG(0,STR_STATE) }}, // "offset/state"
	{"a_checkflag", A_CheckFlag, 3, 2, { STRARG(0,STR_MOBJ_FLAG), STRARG(1,STR_STATE) }}, // "flagname", "offset/state", check_pointer
	{"a_monsterrefire", A_MonsterRefire, 2, 2, { STRARG(1,STR_STATE) }}, // chancecontinue, "offset/state"
	{"a_countdownarg", A_CountdownArg, 1, 1}, // arg
	// terminator
	{NULL}
};

//
// table of line specials

static const dec_linespec_t special_action[] =
{
	{2, 0xD877EE73}, // polyobj_rotateleft
	{3, 0xE996A972}, // polyobj_rotateright
	{4, 0xECA9BF65}, // polyobj_move
	{7, 0x44356C3F}, // polyobj_doorswing
	{8, 0x6C33AC3D}, // polyobj_doorslide
	{10, 0xDE1BCF04}, // door_close
	{12, 0x9E1BD7D0}, // door_raise
	{13, 0x4B31BE7F}, // door_lockedraise
	{15, 0x533D5021}, // autosave
	{19, 0xC337C323}, // thing_stop
	{20, 0xC37E579D}, // floor_lowerbyvalue
	{23, 0xB6E06E7C}, // floor_raisebyvalue
	{26, 0x2902D8F7}, // stairs_builddown
	{27, 0x107509F7}, // stairs_buildup
	{28, 0xCEC47CF0}, // floor_raiseandcrush
	{31, 0x290E3EA4}, // stairs_builddownsync
	{32, 0x75450939}, // stairs_buildupsync
	{35, 0xFBC5CF4D}, // floor_raisebyvaluetimes8
	{36, 0x8E5BF6AC}, // floor_lowerbyvaluetimes8
	{37, 0xC7AA99E9}, // floor_movetovalue
	{40, 0xA3792C24}, // ceiling_lowerbyvalue
	{41, 0x40E73B7D}, // ceiling_raisebyvalue
	{44, 0xE0F7256D}, // ceiling_crushstop
	{47, 0xEF9E6C69}, // ceiling_movetovalue
	{53, 0x760116CF}, // line_settextureoffset
	{55, 0x30F4F914}, // line_setblocking
	{62, 0x479A8B91}, // plat_downwaitupstay
	{64, 0x51F51150}, // plat_upwaitdownstay
	{70, 0xD0149077}, // teleport
	{71, 0x238A5F6F}, // teleport_nofog
	{72, 0x60DC0351}, // thrustthing
	{73, 0x348DF105}, // damagething
	{74, 0x828BDD2B}, // teleport_newmap
	{76, 0x551B8263}, // teleportother
	{97, 0xE245A737}, // ceiling_lowerandcrushdist
	{99, 0x01D47C23}, // floor_raiseandcrushdoom
	{110, 0xB0FCEF36}, // light_raisebyvalue
	{111, 0xC562D6D7}, // light_lowerbyvalue
	{112, 0xA2C2D323}, // light_changetovalue
	{113, 0xD564762B}, // light_fade
	{114, 0xD1EF423B}, // light_glow
	{116, 0xF7F2236F}, // light_strobe
	{117, 0xD02F237B}, // light_stop
	{119, 0xB7759647}, // thing_damage
	{127, 0xF3FEC436}, // thing_setspecial
	{128, 0x60DC0339}, // thrustthingz
	{130, 0xA56DCA06}, // thing_activate
	{131, 0x92B9D076}, // thing_deactivate
	{132, 0xA4F58726}, // thing_remove
	{133, 0xE22B9F6E}, // thing_destroy
	{134, 0x81758F1F}, // thing_projectile
	{135, 0x22F9D323}, // thing_spawn
	{136, 0x882D9A0D}, // thing_projectilegravity
	{137, 0x3E085C1B}, // thing_spawnnofog
	{139, 0x1A69123C}, // thing_spawnfacing
	{172, 0xEB1D2D3A}, // plat_upnearestwaitdownstay
	{173, 0x405E870E}, // noisealert
	{176, 0xB4FBE637}, // thing_changetid
	{179, 0x84313F32}, // changeskill
	{191, 0x397204E7}, // setplayerproperty
	{195, 0x3DC6716A}, // ceiling_crushraiseandstaya
	{196, 0x881F352F}, // ceiling_crushandraisea
	{198, 0x1AF42FAF}, // ceiling_raisebyvaluetimes8
	{199, 0xF96A38F6}, // ceiling_lowerbyvaluetimes8
	{200, 0xA1128F49}, // generic_floor
	{201, 0xC3241445}, // generic_ceiling
	{206, 0x4EAA8B95}, // plat_downwaitupstaylip
	{212, 0xBB76B09B}, // sector_setcolor
	{213, 0xEA5438A3}, // sector_setfade
	{237, 0x66751637}, // changecamera
	{239, 0xE2812E7A}, // floor_raisebyvaluetxty
	{243, 0x5F1DDEF6}, // exit_normal
	{244, 0x9E029A50}, // exit_secret
	// terminator
	{0}
};

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace 'PIT_VileCheck'
	{0x000281B0, CODE_HOOK | HOOK_UINT32, (uint32_t)PIT_VileCheck},
};

