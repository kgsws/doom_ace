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

#define MAKE_FLAG(x)	{.name = #x, .bits = x}

enum
{
	AAPTR_DEFAULT,
	AAPTR_TARGET,
	AAPTR_TRACER,
	AAPTR_MASTER,
	AAPTR_PLAYER1,
	AAPTR_PLAYER2,
	AAPTR_PLAYER3,
	AAPTR_PLAYER4,
	AAPTR_NULL,
	//
	NUM_AAPTRS
};

typedef struct
{
	const uint8_t *name;
	uint32_t bits;
} dec_arg_flag_t;

typedef struct dec_arg_s
{
	uint8_t *(*handler)(uint8_t*,const struct dec_arg_s*);
	uint16_t offset;
	uint16_t optional;
	union
	{
		const dec_arg_flag_t *flags;
		uint8_t *string;
		uint32_t extra;
	};
} dec_arg_t;

typedef struct
{
	uint32_t size;
	const void *def;
	dec_arg_t arg[];
} dec_args_t;

typedef struct
{
	const uint8_t *name;
	void *func;
	const dec_args_t *args;
//	void (*check)(const void*);
} dec_action_t;

typedef struct
{
	uint8_t special; // so far u8 is enough but ZDoom has types > 255
	uint64_t alias;
} __attribute__((packed)) dec_linespec_t; // 'packed' for DOS use only

//

static const uint8_t *action_name;

void *parse_action_func;
void *parse_action_arg;

uint32_t act_cc_tick;

static uint32_t bombflags;
static fixed_t bombdist;
static fixed_t bombfist;

static mobj_t *enemy_look_pick;
static mobj_t *enemy_looker;

static const dec_action_t mobj_action[];
static const dec_linespec_t special_action[];

// pointers
static const uint8_t *pointer_name[NUM_AAPTRS] =
{
	[AAPTR_DEFAULT] = "aaptr_default",
	[AAPTR_TARGET] = "aaptr_target",
	[AAPTR_TRACER] = "aaptr_tracer",
	[AAPTR_MASTER] = "aaptr_master",
	[AAPTR_NULL] = "aaptr_null",
	[AAPTR_PLAYER1] = "aaptr_player1",
	[AAPTR_PLAYER2] = "aaptr_player2",
	[AAPTR_PLAYER3] = "aaptr_player3",
	[AAPTR_PLAYER4] = "aaptr_player4",
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

mobj_t *resolve_ptr(mobj_t *mo, uint32_t ptr)
{
	switch(ptr)
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

fixed_t reslove_fixed_rng(fixed_t value)
{
	fixed_t ret;

	if(!(value & 0x80000000))
	{
		value |= (value & 0x40000000) << 1;
		return value;
	}

	ret = (value >> 4) & 0x01FFF000;
	if(value & 0x20000000)
		ret = -ret;

	if(value & 0x40000000)
		ret -= P_Random() * ((value << 5) & 0x001FFFE0);
	else
		ret += P_Random() * ((value << 5) & 0x001FFFE0);

	return ret;
}

//
// argument parsers

static uint8_t *handle_sound(uint8_t *kw, const dec_arg_t *arg)
{
	*((uint16_t*)(parse_action_arg + arg->offset)) = sfx_by_alias(tp_hash64(kw));
	return tp_get_keyword();
}

static uint8_t *handle_mobjtype(uint8_t *kw, const dec_arg_t *arg)
{
	int32_t type;

	type = mobj_check_type(tp_hash64(kw));
	if(type < 0)
		type = MOBJ_IDX_UNKNOWN;

	*((uint16_t*)(parse_action_arg + arg->offset)) = type;

	return tp_get_keyword();
}

static uint8_t *handle_mobjtype_power(uint8_t *kw, const dec_arg_t *arg)
{
	uint16_t *ptr = (uint16_t*)(parse_action_arg + arg->offset);
	uint8_t *ret;

	ret = handle_mobjtype(kw, arg);
	if(!ret)
		return NULL;

	if(*ptr == MOBJ_IDX_UNKNOWN && !strncmp(kw, "Power", 5))
	{
		int32_t power;

		kw += 5;
		strlwr(kw);

		power = dec_get_powerup_type(kw);
		if(power >= 0)
			*ptr = 0xFFF0 + power;
	}

	return ret;
}

static uint8_t *handle_bool(uint8_t *kw, const dec_arg_t *arg)
{
	uint32_t tmp;

	if(!strcmp(kw, "true"))
		tmp = 1;
	else
	if(!strcmp(kw, "false"))
		tmp = 0;
	else
	if(doom_sscanf(kw, "%d", &tmp) != 1)
		return NULL;

	*((uint8_t*)(parse_action_arg + arg->offset)) = !!tmp;

	return tp_get_keyword();
}

static uint8_t *handle_bool_invert(uint8_t *kw, const dec_arg_t *arg)
{
	kw = handle_bool(kw, arg);

	*((uint8_t*)(parse_action_arg + arg->offset)) ^= 1;

	return kw;
}

static uint8_t *handle_skip(uint8_t *kw, const dec_arg_t *arg)
{
	return tp_get_keyword();
}

static uint8_t *handle_skip_zero(uint8_t *kw, const dec_arg_t *arg)
{
	if(strcmp(kw, "0"))
		return NULL;
	return tp_get_keyword();
}

static uint8_t *handle_forced_string(uint8_t *kw, const dec_arg_t *arg)
{
	if(arg->offset <= 1 && tp_is_string != arg->offset)
		return NULL;
	if(strcmp(kw, arg->string))
		return NULL;
	return tp_get_keyword();
}

static uint8_t *handle_s8(uint8_t *kw, const dec_arg_t *arg)
{
	uint32_t tmp;

	uint_fast8_t negate;

	if(kw[0] == '-')
	{
		kw = tp_get_keyword();
		if(!kw)
			return NULL;
		negate = 1;
	} else
		negate = 0;

	if(doom_sscanf(kw, "%u", &tmp) != 1 || tmp > 127)
		return NULL;

	if(negate)
		tmp = -tmp;

	*((int8_t*)(parse_action_arg + arg->offset)) = tmp;

	return tp_get_keyword();
}

static uint8_t *handle_u16(uint8_t *kw, const dec_arg_t *arg)
{
	uint32_t tmp;

	if(doom_sscanf(kw, "%u", &tmp) != 1 || tmp > 65535)
		return NULL;

	*((uint16_t*)(parse_action_arg + arg->offset)) = tmp;

	return tp_get_keyword();
}

static uint8_t *handle_fixed(uint8_t *kw, const dec_arg_t *arg)
{
	fixed_t value;
	uint_fast8_t negate;

	if(kw[0] == '-')
	{
		kw = tp_get_keyword();
		if(!kw)
			return NULL;
		negate = 1;
	} else
		negate = 0;

	if(tp_parse_fixed(kw, &value))
		return NULL;

	if(negate)
		value = -value;

	*((fixed_t*)(parse_action_arg + arg->offset)) = value;

	return tp_get_keyword();
}

static uint8_t *handle_fixed_rng(uint8_t *kw, const dec_arg_t *arg)
{
	fixed_t v0, v1;
	uint32_t tmp;
	uint_fast8_t negate = 0;
	uint_fast8_t subtract = 0;

	if(!strcmp(kw, "random"))
	{
		v0 = 0;
		goto skip_v0;
	}

	if(kw[0] == '-')
	{
		kw = tp_get_keyword();
		if(!kw)
			return NULL;
		if(!strcmp(kw, "random"))
		{
			v0 = 0;
			subtract = 1;
			goto skip_v0;
		}
		negate = 1;
	}

	if(tp_parse_fixed(kw, &v0))
		return NULL;

	kw = tp_get_keyword_lc();
	if(!kw)
		return NULL;

	if(kw[0] == ',' || kw[0] == ')')
	{
		// just a value
		if(negate)
			v0 = -v0;
		*((fixed_t*)(parse_action_arg + arg->offset)) = v0 & 0x7FFFFFFF;
		return kw;
	}

	// there must be specific format now
	// v0 + random(0, 255) * v1
	// v0 - random(0, 255) * v1
	// v1 can't be negative

	if(kw[0] == '-')
		subtract = 1;
	else
	if(kw[0] != '+')
		return NULL;

	if(!tp_must_get_lc("random"))
		return NULL;

skip_v0:

	if(!tp_must_get("("))
		return NULL;

	if(!tp_must_get("0"))
		return NULL;

	if(!tp_must_get(","))
		return NULL;

	if(!tp_must_get("255"))
		return NULL;

	if(!tp_must_get(")"))
		return NULL;

	kw = tp_get_keyword();
	if(!kw)
		return NULL;
	if(kw[0] == '*')
	{
		kw = tp_get_keyword();
		if(!kw)
			return NULL;

		if(tp_parse_fixed(kw, &v1))
			return NULL;

		kw = tp_get_keyword();
	} else
	if(kw[0] == ',')
		v1 = FRACUNIT;
	else
		return NULL;

	*((fixed_t*)(parse_action_arg + arg->offset)) = 0x80000000 | (subtract << 30) | (negate << 29) | ((v0 & 0x01FFF000) << 4) | ((v1 & 0x001FFFE0) >> 5);

	return kw;
}

static uint8_t *handle_angle(uint8_t *kw, const dec_arg_t *arg)
{
	kw = handle_fixed(kw, arg);

	if(kw)
	{
		fixed_t value = *((fixed_t*)(parse_action_arg + arg->offset));
		value = (11930464 * (int64_t)value) >> 16;
		*((fixed_t*)(parse_action_arg + arg->offset)) = value;
	}

	return kw;
}

static uint8_t *handle_angle_rel(uint8_t *kw, const dec_arg_t *arg)
{
	uint32_t flags = 0;

	if(!strcasecmp(kw, "angle"))
	{
		flags = 1;

		kw = tp_get_keyword(kw, arg);
		if(!kw)
			return NULL;

		if(kw[0] == ',')
		{
			*((fixed_t*)(parse_action_arg + arg->offset)) = 0x80000000;
			return kw;
		}

		if(kw[0] == '-')
			flags |= 2;

		if(kw[0] == '-' || kw[0] == '+')
		{
			kw = tp_get_keyword(kw, arg);
			if(!kw)
				return NULL;
		}
	}

	kw = handle_fixed(kw, arg);
	if(kw)
	{
		fixed_t value = *((fixed_t*)(parse_action_arg + arg->offset));
		value = (11930464 * (int64_t)value) >> 16;
		if(flags & 2)
			value = -value;
		value >>= 1;
		if(flags & 1)
			value |= 0x80000000;
		*((fixed_t*)(parse_action_arg + arg->offset)) = value;
	}

	return kw;
}

static uint8_t *handle_pointer(uint8_t *kw, const dec_arg_t *arg)
{
	strlwr(kw);

	for(uint32_t i = 0; i < NUM_AAPTRS; i++)
	{
		if(!strcmp(kw, pointer_name[i]))
		{
			*((uint8_t*)(parse_action_arg + arg->offset)) = i;
			return tp_get_keyword();
		}
	}

	return NULL;
}

static uint8_t *handle_state(uint8_t *kw, const dec_arg_t *arg)
{
	args_singleState_t *state = (args_singleState_t*)(parse_action_arg + arg->offset);
	uint32_t tmp;

	if(tp_is_string)
	{
		const dec_anim_t *anim;

		strlwr(kw);

		// find animation
		anim = dec_find_animation(kw);
		if(anim)
		{
			// actual animation
			state->state.extra = STATE_SET_ANIMATION | anim->idx;
			state->state.next = 0xFFFFFFFF;
		} else
		{
			// custom state
			state->state.extra = STATE_SET_CUSTOM;
			state->state.next = tp_hash32(kw);
		}
	} else
	if(!arg->extra)
	{
		if(doom_sscanf(kw, "%u", &tmp) != 1 || tmp > 65535)
			return NULL;
		// offset jump
		state->state.extra = STATE_SET_OFFSET;
		state->state.next = tmp;
	} else
		return NULL;

	return tp_get_keyword();
}

static uint8_t *handle_translation(uint8_t *kw, const dec_arg_t *arg)
{
	void *ptr = r_translation_by_name(strlwr(kw));
	*((void**)(parse_action_arg + arg->offset)) = ptr;
	return tp_get_keyword();
}

static uint8_t *handle_mobj_flag(uint8_t *kw, const dec_arg_t *arg)
{
	act_moflag_t *arf;
	const dec_flag_t *flag;
	uint32_t offset;
	uint32_t more = 0;

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
				return NULL;
		}
	} else
	{
		if(flag->bits & (MF_NOBLOCKMAP | MF_NOSECTOR))
			return NULL;
	}

	arf = (void*)(parse_action_arg + arg->offset);
	arf->bits = flag->bits;
	arf->offset = offset;

	return tp_get_keyword();
}

static uint8_t *handle_damage(uint8_t *kw, const dec_arg_t *arg)
{
	uint32_t lo, hi, mul, add;

	lo = 0;
	hi = 0;
	mul = 0;
	add = 0;

	if(!strcmp(kw, "random"))
	{
		// function entry
		kw = tp_get_keyword();
		if(!kw)
			return NULL;
		if(kw[0] != '(')
			return NULL;

		// low
		kw = tp_get_keyword();
		if(!kw)
			return NULL;

		if(doom_sscanf(kw, "%u", &lo) != 1 || lo > 511)
			return NULL;

		// comma
		kw = tp_get_keyword();
		if(!kw)
			return NULL;
		if(kw[0] != ',')
			return NULL;

		// high
		kw = tp_get_keyword();
		if(!kw)
			return NULL;

		if(doom_sscanf(kw, "%u", &hi) != 1 || hi > 511)
			return NULL;

		// function end
		kw = tp_get_keyword();
		if(!kw)
			return NULL;
		if(kw[0] != ')')
			return NULL;

		// check range
		if(lo > hi)
			return NULL;
		if(hi - lo > 255)
			return NULL;

		// next
		kw = tp_get_keyword();
		if(!kw)
			return NULL;

		if(kw[0] == '*')
		{
			// multiply
			kw = tp_get_keyword();
			if(!kw)
				return NULL;

			if(doom_sscanf(kw, "%u", &mul) != 1 || !mul || mul > 16)
				return NULL;

			mul--;

			// next
			kw = tp_get_keyword();
			if(!kw)
				return NULL;
		}

		if(kw[0] == '+')
		{
			// add
			kw = tp_get_keyword();
			if(!kw)
				return NULL;

			if(doom_sscanf(kw, "%u", &add) != 1 || add > 511)
				return NULL;

			// next
			kw = tp_get_keyword();
			if(!kw)
				return NULL;
		}

		add = DAMAGE_CUSTOM(lo, hi, mul, add);
	} else
	{
		// direct value
		if(doom_sscanf(kw, "%u", &add) != 1 || add > 1000000)
			return NULL;
		kw = tp_get_keyword();
	}

	*((uint32_t*)(parse_action_arg + arg->offset)) = add;

	return kw;
}

static uint8_t *handle_damage_type(uint8_t *kw, const dec_arg_t *arg)
{
	if(!tp_is_string)
		return NULL;

	strlwr(kw);

	*((uint8_t*)(parse_action_arg + arg->offset)) = dec_get_custom_damage(kw);

	return tp_get_keyword();
}

static uint8_t *handle_flags(uint8_t *kw, const dec_arg_t *arg)
{
	const dec_arg_flag_t *flag;
	uint32_t is_16bit = !(arg->flags->bits & 0xFFFF0000);
	num32_t *value;

	value = parse_action_arg + arg->offset;

	if(is_16bit)
		value->u16[0] = 0;
	else
		value->u32 = 0;

	while(1)
	{
		if(kw[0] != '0')
		{
			flag = arg->flags;
			while(flag->name)
			{
				if(!strcmp(flag->name, kw))
					break;
				flag++;
			}

			if(!flag->name)
				engine_error("DECORATE", "Unknown flag '%s' for action '%s' in '%s'!", kw, action_name, parse_actor_name);

			if(is_16bit)
				value->u16[0] |= flag->bits;
			else
				value->u32 |= flag->bits;
		}

		kw = tp_get_keyword(kw, arg);
		if(!kw)
			return NULL;

		if(kw[0] == ',' || kw[0] == ')')
			return kw;

		if(kw[0] != '|')
			engine_error("DECORATE", "Unable to parse flags for action '%s' in '%s'!", action_name, parse_actor_name);

		kw = tp_get_keyword(kw, arg);
		if(!kw)
			return NULL;
	}
}

static uint8_t *handle_ftics(uint8_t *kw, const dec_arg_t *arg)
{
	fixed_t value;
	uint_fast8_t negate;

	if(kw[0] == '-')
	{
		kw = tp_get_keyword();
		if(!kw)
			return NULL;
		negate = 1;
	} else
		negate = 0;

	if(tp_parse_fixed(kw, &value))
		return NULL;

	if(negate)
		value = -value;

	value *= 35;
	value >>= FRACBITS;

	if(!value)
		value = 35 * 3;

	*((uint32_t*)(parse_action_arg + arg->offset)) = value;

	return tp_get_keyword();
}

static uint8_t *handle_font(uint8_t *kw, const dec_arg_t *arg)
{
	int32_t lump;
	uint32_t magic;

	lump = wad_get_lump(kw);

	wad_read_lump(&magic, lump, sizeof(uint32_t));
	if(magic != FONT_MAGIC_ID)
		engine_error("FONT", "Invalid font %.8s.", lumpinfo[lump].name);

	*((uint16_t*)(parse_action_arg + arg->offset)) = lump;

	return tp_get_keyword(kw, arg);
}

static uint8_t *handle_print_text(uint8_t *kw, const dec_arg_t *arg)
{
	uint32_t len;
	uint8_t *dst;
	print_text_t *pt = parse_action_arg;

	if(!tp_is_string)
		return NULL;

	len = strlen(kw);
	len += 4;
	len &= ~3;

	dst = dec_es_alloc(len);
	strcpy(dst, kw);

	len = 1;
	while(*dst)
	{
		if(*dst == '\n')
			len++;
		dst++;
	}
	pt->lines = len;

	return tp_get_keyword(kw, arg);
}

//
// enemy search

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t PIT_EnemySearch(mobj_t *thing)
{
	if(thing == enemy_looker)
		return 1;

	if(enemy_looker->flags1 & MF1_SEEKERMISSILE && thing == enemy_looker->target)
		return 1;

	if(!(thing->flags & MF_SHOOTABLE))
		return 1;

	if(thing->flags1 & MF1_ISMONSTER)
	{
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

	if(pl->info_flags & PLF_AUTO_AIM || map_level_info->flags & MAP_FLAG_NO_FREELOOK)
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
	count -= count >> 2;
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
// common args

static const dec_args_t args_SingleMobjtype =
{
	.size = sizeof(args_singleType_t),
	.arg =
	{
		{handle_mobjtype, offsetof(args_singleType_t, type)},
		// terminator
		{NULL}
	}
};

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
		pl->psprites[1].state = states + state;
		pl->psprites[1].tics = 0;
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
	state_t *state;
	angle_t angle;

	if(sound == 4)
		count = 2;
	else
		count = 1;

	if(!inventory_take(mo, ammo, count))
		return;

	state = states + pl->readyweapon->st_weapon.flash;

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

	// mobj to 'missile' animation
	if(mo->animation != ANIM_MISSILE && mo->info->state_missile)
		mobj_set_animation(mo, ANIM_MISSILE);

	pl->psprites[1].state = state;
	pl->psprites[1].tics = 0;

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
// weapon (logic)

const args_singleU16_t def_LowerRaise =
{
	.value = 6
};

static const dec_args_t args_LowerRaise =
{
	.size = sizeof(args_singleU16_t),
	.def = &def_LowerRaise,
	.arg =
	{
		{handle_u16, offsetof(args_singleFixed_t, value), 2},
		// terminator
		{NULL}
	}
};

__attribute((regparm(2),no_caller_saved_registers))
void A_Lower(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	int32_t value;

	if(!pl)
		return;

	if(mo->custom_inventory)
		return;

	if(st->arg)
	{
		const args_singleU16_t *arg = st->arg;
		value = arg->value;
	} else
		value = def_LowerRaise.value;

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

	if(st->arg)
	{
		const args_singleU16_t *arg = st->arg;
		value = arg->value;
	} else
		value = def_LowerRaise.value;

	pl->weapon_ready = 0;
	pl->psprites[1].sy -= value;

	if(pl->psprites[1].sy > WEAPONTOP)
		return;

	pl->psprites[1].sy = WEAPONTOP;

	stfunc(mo, pl->readyweapon->st_weapon.ready, 0);
}

//
// A_GunFlash

static const dec_arg_flag_t flags_GunFlash[] =
{
	MAKE_FLAG(GFF_NOEXTCHANGE),
	// terminator
	{NULL}
};

static const dec_args_t args_GunFlash =
{
	.size = sizeof(args_GunFlash_t),
	.arg =
	{
		{handle_state, offsetof(args_GunFlash_t, state), 2, .extra = 1},
		{handle_flags, offsetof(args_GunFlash_t, flags), 1, flags_GunFlash},
		// terminator
		{NULL}
	}
};

__attribute((regparm(2),no_caller_saved_registers))
void A_GunFlash(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	uint32_t state, extra;
	uint32_t flags;

	if(!pl)
		return;

	if(mo->custom_inventory)
		return;

	if(st->arg)
	{
		const args_GunFlash_t *arg = st->arg;
		extra = arg->state.extra;
		state = arg->state.next;
		flags = arg->flags;
	} else
	{
		extra = 0;
		state = 0;
		flags = 0;
	}

	// mobj to 'missile' animation
	if(!(flags & GFF_NOEXTCHANGE) && mo->animation != ANIM_MISSILE && mo->info->state_missile)
		mobj_set_animation(mo, ANIM_MISSILE);

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

static const dec_arg_flag_t flags_WeaponReady[] =
{
	MAKE_FLAG(WRF_NOBOB),
	MAKE_FLAG(WRF_NOFIRE),
	MAKE_FLAG(WRF_NOSWITCH),
	MAKE_FLAG(WRF_DISABLESWITCH),
	MAKE_FLAG(WRF_NOPRIMARY),
	MAKE_FLAG(WRF_NOSECONDARY),
	// terminator
	{NULL}
};

static const dec_args_t args_WeaponReady =
{
	.size = sizeof(args_singleFlags_t),
	.arg =
	{
		{handle_flags, offsetof(args_singleFlags_t, flags), 2, flags_WeaponReady},
		// terminator
		{NULL}
	}
};

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

	if(st->arg)
	{
		const args_singleFlags_t *arg = st->arg;
		flags = arg->flags;
	} else
		flags = 0;

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

static const dec_args_t args_ReFire =
{
	.size = sizeof(args_singleState_t),
	.arg =
	{
		{handle_state, offsetof(args_singleState_t, state), 2, .extra = 1},
		// terminator
		{NULL}
	}
};

__attribute((regparm(2),no_caller_saved_registers))
void A_ReFire(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	uint32_t btn, state, extra;

	if(!pl)
		return;

	if(mo->custom_inventory)
		return;

	if(st->arg)
	{
		const args_singleState_t *arg = st->arg;
		extra = arg->state.extra;
		state = arg->state.next;
	} else
	{
		extra = 0;
		state = 0;
	}

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

static const dec_arg_flag_t flags_StartSound[] =
{
	MAKE_FLAG(CHAN_BODY), // default, 0
	MAKE_FLAG(CHAN_WEAPON),
	MAKE_FLAG(CHAN_VOICE),
	// terminator
	{NULL}
};

static const dec_args_t args_StartSound =
{
	.size = sizeof(args_StartSound_t),
	.arg =
	{
		{handle_sound, offsetof(args_StartSound_t, sound)},
		{handle_flags, offsetof(args_StartSound_t, slot), 1, flags_StartSound},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_StartSound(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_StartSound_t *arg = st->arg;
	void *origin;

	switch(arg->slot)
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

	S_StartSound(origin, arg->sound);
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

		if(drop->chance < 255 && drop->chance > P_Random())
			continue;

		item = P_SpawnMobj(mo->x, mo->y, mo->z + (8 << FRACBITS), drop->type);
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
	mobj_t *th = mo->subsector->sector->soundtarget;

	// workaround for self-targetting and NOTARGET
	if(th == mo || (th && th->flags1 & MF1_NOTARGET))
		mo->subsector->sector->soundtarget = NULL;

	doom_A_Look(mo);

	mo->subsector->sector->soundtarget = th;
}

//
// A_Chase

static __attribute((regparm(2),no_caller_saved_registers))
void A_Chase(mobj_t *mo, state_t *st, stfunc_t stfunc)
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
	doom_A_Chase(mo);
	mo->info->speed = speed;

	// HACK - move other sound slots
	mo->sound_body.x = mo->x;
	mo->sound_body.y = mo->y;
	mo->sound_weapon.x = mo->x;
	mo->sound_weapon.y = mo->y;
}

//
// A_SpawnProjectile

static const args_SpawnProjectile_t def_SpawnProjectile =
{
	.spawnheight = 32 * FRACUNIT,
	.ptr = AAPTR_TARGET,
};

static const dec_arg_flag_t flags_SpawnProjectile[] =
{
	MAKE_FLAG(CMF_AIMOFFSET),
	MAKE_FLAG(CMF_AIMDIRECTION),
	MAKE_FLAG(CMF_TRACKOWNER),
	MAKE_FLAG(CMF_CHECKTARGETDEAD),
	MAKE_FLAG(CMF_ABSOLUTEPITCH),
	MAKE_FLAG(CMF_OFFSETPITCH),
	MAKE_FLAG(CMF_SAVEPITCH),
	MAKE_FLAG(CMF_ABSOLUTEANGLE),
	// terminator
	{NULL}
};

static const dec_args_t args_SpawnProjectile =
{
	.size = sizeof(args_SpawnProjectile_t),
	.def = &def_SpawnProjectile,
	.arg =
	{
		{handle_mobjtype, offsetof(args_SpawnProjectile_t, missiletype)},
		{handle_fixed, offsetof(args_SpawnProjectile_t, spawnheight), 1},
		{handle_fixed, offsetof(args_SpawnProjectile_t, spawnofs_xy), 1},
		{handle_angle, offsetof(args_SpawnProjectile_t, angle), 1},
		{handle_flags, offsetof(args_SpawnProjectile_t, flags), 1, flags_SpawnProjectile},
		{handle_angle, offsetof(args_SpawnProjectile_t, pitch), 1},
		{handle_pointer, offsetof(args_SpawnProjectile_t, ptr), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_SpawnProjectile(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	angle_t angle = mo->angle;
	fixed_t pitch = 0;
	int32_t dist = 0;
	fixed_t x, y, z, speed;
	mobj_t *th, *target;
	const args_SpawnProjectile_t *arg = st->arg;

	x = mo->x;
	y = mo->y;
	z = mo->z + arg->spawnheight;

	if(arg->flags & CMF_TRACKOWNER && mo->flags & MF_MISSILE)
	{
		mo = mo->target;
		if(!mo)
			return;
	}

	if(arg->flags & CMF_AIMDIRECTION)
		target = NULL;
	else
	{
		target = resolve_ptr(mo, arg->ptr);
		if(!target)
			return;
	}

	if(target && arg->flags & CMF_CHECKTARGETDEAD && target->health <= 0)
		return;

	if(target && arg->flags & CMF_AIMOFFSET)
	{
		angle = R_PointToAngle2(x, y, target->x, target->y);
		dist = P_AproxDistance(target->x - x, target->y - y);
	}

	if(arg->spawnofs_xy)
	{
		angle_t a = (mo->angle - ANG90) >> ANGLETOFINESHIFT;
		x += FixedMul(arg->spawnofs_xy, finecosine[a]);
		y += FixedMul(arg->spawnofs_xy, finesine[a]);
	}

	if(!(arg->flags & (CMF_AIMOFFSET|CMF_AIMDIRECTION)))
	{
		angle = R_PointToAngle2(x, y, target->x, target->y);
		dist = P_AproxDistance(target->x - x, target->y - y);
	}

	if(arg->flags & CMF_ABSOLUTEANGLE)
		angle = arg->angle;
	else
	{
		angle += arg->angle;
		if(target && target->flags & MF_SHADOW)
			angle += (P_Random() - P_Random()) << 20;
	}

	th = P_SpawnMobj(x, y, z, arg->missiletype);

	speed = projectile_speed(th->info);

	if(arg->flags & (CMF_AIMDIRECTION|CMF_ABSOLUTEPITCH))
		pitch = -arg->pitch;
	else
	if(speed)
	{
		dist /= speed;
		if(dist <= 0)
			dist = 1;
		dist = ((target->z + 32 * FRACUNIT) - z) / dist; // TODO: maybe aim for the middle?
		th->momz = FixedDiv(dist, speed);
		if(arg->flags & CMF_OFFSETPITCH)
			pitch = -arg->pitch;
	}

	missile_stuff(th, mo, target, speed, angle, pitch, th->momz);
}

//
// A_CustomBulletAttack

static const dec_arg_flag_t flags_CustomBulletAttack[] =
{
	MAKE_FLAG(CBAF_AIMFACING),
	MAKE_FLAG(CBAF_NORANDOM),
	MAKE_FLAG(CBAF_NORANDOMPUFFZ),
	// terminator
	{NULL}
};

static const dec_args_t args_CustomBulletAttack =
{
	.size = sizeof(args_BulletAttack_t),
	.arg =
	{
		{handle_angle, offsetof(args_BulletAttack_t, spread_hor)},
		{handle_angle, offsetof(args_BulletAttack_t, spread_ver)},
		{handle_s8, offsetof(args_BulletAttack_t, blt_count)},
		{handle_damage, offsetof(args_BulletAttack_t, damage)},
		{handle_mobjtype, offsetof(args_BulletAttack_t, pufftype), 1},
		{handle_fixed, offsetof(args_BulletAttack_t, range), 1},
		{handle_flags, offsetof(args_BulletAttack_t, flags), 1, flags_CustomBulletAttack},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_CustomBulletAttack(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	uint32_t damage;
	angle_t angle;
	fixed_t slope;
	fixed_t range;
	const args_BulletAttack_t *arg = st->arg;

	if(!mo->target)
		return;

	if(arg->blt_count <= 0)
		return;

	if(!(arg->flags & CBAF_AIMFACING))
	{
		angle = R_PointToAngle2(mo->x, mo->y, mo->target->x, mo->target->y);
		mo->angle = angle;
	} else
		angle = mo->angle;

	if(arg->pufftype)
		mo_puff_type = arg->pufftype;

	if(mobjinfo[mo_puff_type].replacement)
		mo_puff_type = mobjinfo[mo_puff_type].replacement;
	else
		mo_puff_type = mo_puff_type;

	mo_puff_flags = !!(arg->flags & CBAF_NORANDOMPUFFZ);

	damage = arg->damage;
	if(damage & DAMAGE_IS_CUSTOM)
		damage = mobj_calc_damage(damage);

	if(!arg->range)
		range = 2048 * FRACUNIT;
	else
		range = arg->range;

	slope = P_AimLineAttack(mo, angle, range);

	S_StartSound(SOUND_CHAN_WEAPON(mo), mo->info->attacksound);

	for(uint32_t i = 0; i < arg->blt_count; i++)
	{
		angle_t aaa;
		fixed_t sss;
		uint32_t dmg;

		if(!(arg->flags & CBAF_NORANDOM))
			dmg = damage * (P_Random() % 3) + 1;
		else
			dmg = damage;

		aaa = angle;
		sss = slope;

		if(arg->spread_hor)
		{
			aaa -= arg->spread_hor;
			aaa += (arg->spread_hor >> 7) * P_Random();
		}
		if(arg->spread_ver)
		{
			fixed_t tmp;
			tmp = -arg->spread_ver;
			tmp += (arg->spread_ver >> 7) * P_Random();
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

static const dec_args_t args_CustomMeleeAttack =
{
	.size = sizeof(args_MeleeAttack_t),
	.arg =
	{
		{handle_damage, offsetof(args_MeleeAttack_t, damage)},
		{handle_sound, offsetof(args_MeleeAttack_t, sound_hit), 1},
		{handle_sound, offsetof(args_MeleeAttack_t, sound_miss), 1},
		{handle_damage_type, offsetof(args_MeleeAttack_t, damage_type), 1},
		{handle_bool, offsetof(args_MeleeAttack_t, sacrifice), 1}, // ignored
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_CustomMeleeAttack(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_MeleeAttack_t *arg = st->arg;
	uint32_t damage;

	if(!mo->target)
		return;

	A_FaceTarget(mo, NULL, NULL);

	if(!mobj_check_melee_range(mo))
	{
		S_StartSound(mo, arg->sound_miss);
		return;
	}

	S_StartSound(mo, arg->sound_hit);

	damage = arg->damage;
	if(damage & DAMAGE_IS_CUSTOM)
		damage = mobj_calc_damage(damage);

	damage = DAMAGE_WITH_TYPE(damage, arg->damage_type);

	mobj_damage(mo->target, mo, mo, damage, NULL);
}

//
// A_FireProjectile

static const dec_arg_flag_t flags_FireProjectile[] =
{
	MAKE_FLAG(FPF_AIMATANGLE),
	MAKE_FLAG(FPF_TRANSFERTRANSLATION),
	MAKE_FLAG(FPF_NOAUTOAIM),
	// terminator
	{NULL}
};

static const dec_args_t args_FireProjectile =
{
	.size = sizeof(args_SpawnProjectile_t),
	.arg =
	{
		{handle_mobjtype, offsetof(args_SpawnProjectile_t, missiletype)},
		{handle_angle, offsetof(args_SpawnProjectile_t, angle), 1},
		{handle_bool_invert, offsetof(args_SpawnProjectile_t, noammo), 1},
		{handle_fixed, offsetof(args_SpawnProjectile_t, spawnofs_xy), 1},
		{handle_fixed, offsetof(args_SpawnProjectile_t, spawnheight), 1},
		{handle_flags, offsetof(args_SpawnProjectile_t, flags), 1, flags_FireProjectile},
		{handle_angle, offsetof(args_SpawnProjectile_t, pitch), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_FireProjectile(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	const args_SpawnProjectile_t *arg = st->arg;
	mobj_t *th;
	angle_t angle, pitch;
	fixed_t slope;
	fixed_t x, y, z;

	if(!pl)
		return;

	if(	!mo->custom_inventory &&
		!arg->noammo &&
		remove_ammo(mo)
	)
		return;

	pitch = -arg->pitch;
	angle = mo->angle;

	if(arg->flags & FPF_AIMATANGLE)
		angle += arg->angle;

	if(!player_aim(pl, &angle, &slope, mobjinfo[arg->missiletype].flags1 & MF1_SEEKERMISSILE))
		pitch += mo->pitch;

	if(arg->angle && !(arg->flags & FPF_AIMATANGLE))
		angle = mo->angle + arg->angle;

	x = mo->x;
	y = mo->y;
	z = mo->z;
	z += mo->height / 2;
	z += mo->info->player.attack_offs;
	z += mo->player->viewz - mo->z - mo->info->player.view_height;
	z += arg->spawnheight;

	if(arg->spawnofs_xy)
	{
		angle_t a = (mo->angle - ANG90) >> ANGLETOFINESHIFT;
		x += FixedMul(arg->spawnofs_xy, finecosine[a]);
		y += FixedMul(arg->spawnofs_xy, finesine[a]);
	}

	th = P_SpawnMobj(x, y, z, arg->missiletype);
	if(th->flags & MF_MISSILE)
		missile_stuff(th, mo, linetarget, projectile_speed(th->info), angle, pitch, slope);

	if(arg->flags & FPF_TRANSFERTRANSLATION)
		th->translation = mo->translation;
}

//
// A_FireBullets

static const args_BulletAttack_t def_FireBullets =
{
	.pufftype = 37,
	.flags = FBF_USEAMMO,
	.range = 8192 * FRACUNIT,
};

static const dec_arg_flag_t flags_FireBullets[] =
{
	MAKE_FLAG(FBF_USEAMMO),
	MAKE_FLAG(FBF_NOFLASH),
	MAKE_FLAG(FBF_NORANDOM),
	MAKE_FLAG(FBF_NORANDOMPUFFZ),
	// terminator
	{NULL}
};

static const dec_args_t args_FireBullets =
{
	.size = sizeof(args_BulletAttack_t),
	.def = &def_FireBullets,
	.arg =
	{
		{handle_angle, offsetof(args_BulletAttack_t, spread_hor)},
		{handle_angle, offsetof(args_BulletAttack_t, spread_ver)},
		{handle_s8, offsetof(args_BulletAttack_t, blt_count)},
		{handle_damage, offsetof(args_BulletAttack_t, damage)},
		{handle_mobjtype, offsetof(args_BulletAttack_t, pufftype), 1},
		{handle_flags, offsetof(args_BulletAttack_t, flags), 1, flags_FireBullets},
		{handle_fixed, offsetof(args_BulletAttack_t, range), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_FireBullets(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	const args_BulletAttack_t *arg = st->arg;
	uint32_t spread, count, damage;
	angle_t angle;
	fixed_t slope;

	if(!pl)
		return;

	if(	!mo->custom_inventory &&
		arg->flags & FBF_USEAMMO &&
		remove_ammo(mo)
	)
		return;

	if(mobjinfo[arg->pufftype].replacement)
		mo_puff_type = mobjinfo[arg->pufftype].replacement;
	else
		mo_puff_type = arg->pufftype;
	mo_puff_flags = !!(arg->flags & FBF_NORANDOMPUFFZ);

	if(arg->blt_count < 0)
	{
		count = -arg->blt_count;
		spread = 1;
	} else
	{
		count = arg->blt_count;
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

	if(!(arg->flags & FBF_NOFLASH) && mo->info->state_missile)
		mobj_set_animation(mo, ANIM_MISSILE);

	angle = mo->angle;
	if(!player_aim(pl, &angle, &slope, 0))
		slope = finetangent[(pl->mo->pitch + ANG90) >> ANGLETOFINESHIFT];

	damage = arg->damage;
	if(damage & DAMAGE_IS_CUSTOM)
		damage = mobj_calc_damage(damage);

	S_StartSound(SOUND_CHAN_WEAPON(mo), pl->readyweapon->attacksound);

	for(uint32_t i = 0; i < count; i++)
	{
		angle_t aaa;
		fixed_t sss;
		uint32_t dmg;

		if(!(arg->flags & FBF_NORANDOM))
			dmg = damage * (P_Random() % 3) + 1;
		else
			dmg = damage;

		aaa = angle;
		sss = slope;

		if(spread)
		{
			if(arg->spread_hor)
			{
				aaa -= arg->spread_hor;
				aaa += (arg->spread_hor >> 7) * P_Random();
			}
			if(arg->spread_ver)
			{
				fixed_t tmp;
				tmp = -arg->spread_ver;
				tmp += (arg->spread_ver >> 7) * P_Random();
				sss += tmp >> 14;
			}
		}

		P_LineAttack(mo, aaa, arg->range, sss, damage);
	}

	// must restore original puff!
	mo_puff_type = 37;
	mo_puff_flags = 0;
}

//
// A_CustomPunch

static const args_PunchAttack_t def_CustomPunch =
{
	.pufftype = 37,
	.flags = CPF_USEAMMO,
	.range = 64 * FRACUNIT,
};

static const dec_arg_flag_t flags_CustomPunch[] =
{
	MAKE_FLAG(CPF_USEAMMO),
	MAKE_FLAG(CPF_PULLIN),
	MAKE_FLAG(CPF_NORANDOMPUFFZ),
	MAKE_FLAG(CPF_NOTURN),
	// terminator
	{NULL}
};

static const dec_args_t args_CustomPunch =
{
	.size = sizeof(args_PunchAttack_t),
	.def = &def_CustomPunch,
	.arg =
	{
		{handle_damage, offsetof(args_PunchAttack_t, damage)},
		{handle_bool, offsetof(args_PunchAttack_t, norandom), 1},
		{handle_flags, offsetof(args_PunchAttack_t, flags), 1, flags_CustomPunch},
		{handle_mobjtype, offsetof(args_PunchAttack_t, pufftype), 1},
		{handle_fixed, offsetof(args_PunchAttack_t, range), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_CustomPunch(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	const args_PunchAttack_t *arg = st->arg;
	uint32_t damage;
	angle_t angle;
	fixed_t slope;

	if(!pl)
		return;

	if(	!mo->custom_inventory &&
		arg->flags & CPF_USEAMMO &&
		!weapon_has_ammo(mo, pl->readyweapon, pl->attackdown)
	)
		return;

	if(mobjinfo[arg->pufftype].replacement)
		mo_puff_type = mobjinfo[arg->pufftype].replacement;
	else
		mo_puff_type = arg->pufftype;
	mo_puff_flags = !!(arg->flags & CPF_NORANDOMPUFFZ);

	angle = mo->angle;
	if(!player_aim(pl, &angle, &slope, 0))
		slope = finetangent[(pl->mo->pitch + ANG90) >> ANGLETOFINESHIFT];

	angle += (P_Random() - P_Random()) << 18;

	damage = arg->damage;
	if(damage & DAMAGE_IS_CUSTOM)
		damage = mobj_calc_damage(damage);

	linetarget = NULL;
	P_LineAttack(mo, angle, arg->range, slope, damage);

	if(linetarget)
	{
		if(arg->flags & CPF_PULLIN)
			mo->flags |= MF_JUSTATTACKED;
		if(!(arg->flags & CPF_NOTURN))
			mo->angle = R_PointToAngle2(mo->x, mo->y, linetarget->x, linetarget->y);
		if(!mo->custom_inventory && arg->flags & CPF_USEAMMO)
			remove_ammo(mo);
		S_StartSound(SOUND_CHAN_WEAPON(mo), pl->readyweapon->attacksound);
	}

	// must restore original puff!
	mo_puff_type = 37;
	mo_puff_flags = 0;
}

//
// A_BFGSpray

static const args_BFGSpray_t def_BFGSpray =
{
	.spraytype = 42, // MT_EXTRABFG
	.numrays = 40,
	.damagecnt = 15,
	.angle = ANG90,
	.range = 1024 * FRACUNIT
};

static const dec_args_t args_BFGSpray =
{
	.size = sizeof(args_BFGSpray_t),
	.def = &def_BFGSpray,
	.arg =
	{
		{handle_mobjtype, offsetof(args_BFGSpray_t, spraytype), 2},
		{handle_u16, offsetof(args_BFGSpray_t, numrays), 1},
		{handle_u16, offsetof(args_BFGSpray_t, damagecnt), 1},
		{handle_angle, offsetof(args_BFGSpray_t, angle), 1},
		{handle_fixed, offsetof(args_BFGSpray_t, range), 1},
		{handle_forced_string, 0, 0, .string = "32"},
		{handle_damage, offsetof(args_BFGSpray_t, damage)},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_BFGSpray(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_BFGSpray_t *arg = st->arg;
	mobj_t *source = mo->target;
	angle_t angle, astep;
	uint32_t damage;

	if(!source)
		return;

	if(!arg)
		arg = &def_BFGSpray;

	if(!arg->numrays)
		return;

	if(arg->damage)
	{
		damage = arg->damage;
		if(damage & DAMAGE_IS_CUSTOM)
			damage = mobj_calc_damage(damage);
	} else
		damage = 0;

	astep = arg->angle / arg->numrays;

	angle = mo->angle;
	angle -= arg->angle / 2;
	angle += astep / 2;

	for(uint32_t i = 0; i < arg->numrays; i++, angle += astep)
	{
		uint32_t dmg = damage;

		P_AimLineAttack(source, angle, arg->range);

		if(!linetarget)
			continue;

		P_SpawnMobj(linetarget->x, linetarget->y, linetarget->z + (linetarget->height >> 2), arg->spraytype);

		if(!dmg)
		{
			for(uint32_t j = 0; j < arg->damagecnt; j++)
				dmg += (P_Random() & 7) + 1;
		}

		dmg = DAMAGE_WITH_TYPE(dmg, mobjinfo[arg->spraytype].damage_type);

		mobj_damage(linetarget, source, source, dmg, NULL);
	}
}

//
// A_SeekerMissile

static const dec_arg_flag_t flags_SeekerMissile[] =
{
	MAKE_FLAG(SMF_LOOK),
	MAKE_FLAG(SMF_PRECISE),
	// terminator
	{NULL}
};

static const dec_args_t args_SeekerMissile =
{
	.size = sizeof(args_SeekerMissile_t),
	.arg =
	{
		{handle_angle, offsetof(args_SeekerMissile_t, threshold)},
		{handle_angle, offsetof(args_SeekerMissile_t, maxturn)},
		{handle_flags, offsetof(args_SeekerMissile_t, flags), 1, flags_SeekerMissile},
		{handle_u16, offsetof(args_SeekerMissile_t, chance), 1},
		{handle_u16, offsetof(args_SeekerMissile_t, range), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_SeekerMissile(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_SeekerMissile_t *arg = st->arg;
	mobj_t *target = mo->tracer;
	uint32_t sub = 0;
	angle_t dir, delta;
	fixed_t speed, dist;

	speed = projectile_speed(mo->info);
	if(!speed)
		return;

	if(arg->flags & SMF_LOOK && !target)
	{
		uint16_t chance = arg->chance;
		if(!chance)
			chance = 50;

		if(chance < 256 && P_Random() >= chance)
			return;

		dist = arg->range;
		if(!dist)
			dist = 10;

		target = look_for_monsters(mo, dist);
		mo->tracer = target;
	}

	if(!target)
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
		if(dir == mo->angle)
			return;
		delta = ANG180;
	}

	if(delta > arg->threshold)
	{
		delta /= 2;
		if(delta > arg->maxturn)
			delta = arg->maxturn;
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

	if(arg->flags & SMF_PRECISE)
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

static const dec_arg_flag_t flags_SpawnItemEx[] =
{
	MAKE_FLAG(SXF_ISTRACER), // first flag must be > 65535 to indicate 32bits
	MAKE_FLAG(SXF_ISMASTER),
	MAKE_FLAG(SXF_ISTARGET),
	MAKE_FLAG(SXF_ORIGINATOR),
	MAKE_FLAG(SXF_NOPOINTERS),
	MAKE_FLAG(SXF_SETTRACER),
	MAKE_FLAG(SXF_SETTARGET),
	MAKE_FLAG(SXF_CLEARCALLERTID),
	MAKE_FLAG(SXF_USEBLOODCOLOR),
	MAKE_FLAG(SXF_TRANSFERPOINTERS),
	MAKE_FLAG(SXF_TRANSFERPITCH),
	MAKE_FLAG(SXF_TRANSFERAMBUSHFLAG),
	MAKE_FLAG(SXF_TELEFRAG),
	MAKE_FLAG(SXF_NOCHECKPOSITION),
	MAKE_FLAG(SXF_SETMASTER),
	MAKE_FLAG(SXF_ABSOLUTEVELOCITY),
	MAKE_FLAG(SXF_ABSOLUTEANGLE),
	MAKE_FLAG(SXF_ABSOLUTEPOSITION),
	MAKE_FLAG(SXF_TRANSFERTRANSLATION),
	// terminator
	{NULL}
};

static const dec_args_t args_SpawnItemEx =
{
	.size = sizeof(args_SpawnItemEx_t),
	.arg =
	{
		{handle_mobjtype, offsetof(args_SpawnItemEx_t, type), 1},
		{handle_fixed_rng, offsetof(args_SpawnItemEx_t, ox), 1},
		{handle_fixed_rng, offsetof(args_SpawnItemEx_t, oy), 1},
		{handle_fixed_rng, offsetof(args_SpawnItemEx_t, oz), 1},
		{handle_fixed_rng, offsetof(args_SpawnItemEx_t, vx), 1},
		{handle_fixed_rng, offsetof(args_SpawnItemEx_t, vy), 1},
		{handle_fixed_rng, offsetof(args_SpawnItemEx_t, vz), 1},
		{handle_angle, offsetof(args_SpawnItemEx_t, angle), 1},
		{handle_flags, offsetof(args_SpawnItemEx_t, flags), 1, flags_SpawnItemEx},
		{handle_u16, offsetof(args_SpawnItemEx_t, fail), 1},
		{handle_u16, offsetof(args_SpawnItemEx_t, tid), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_SpawnItemEx(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	player_t *pl = mo->player;
	const args_SpawnItemEx_t *arg = st->arg;
	mobj_t *th, *origin;
	angle_t angle;
	fixed_t ss, cc;
	fixed_t x, y;
	fixed_t mx, my;

	if(arg->fail && P_Random() < arg->fail)
		return;

	angle = arg->angle;

	if(!(arg->flags & SXF_ABSOLUTEANGLE))
		angle += mo->angle;

	ss = finesine[angle >> ANGLETOFINESHIFT];
	cc = finecosine[angle >> ANGLETOFINESHIFT];

	if(arg->flags & SXF_ABSOLUTEPOSITION)
	{
		x = mo->x + reslove_fixed_rng(arg->ox);
		y = mo->y + reslove_fixed_rng(arg->oy);
	} else
	{
		fixed_t ox, oy;
		ox = reslove_fixed_rng(arg->ox);
		oy = reslove_fixed_rng(arg->oy);
		x = FixedMul(ox, cc);
		x += FixedMul(oy, ss);
		y = FixedMul(ox, ss);
		y -= FixedMul(oy, cc);
		x += mo->x;
		y += mo->y;
	}

	if(!(arg->flags & SXF_ABSOLUTEVELOCITY))
	{
		fixed_t vx, vy;
		vx = reslove_fixed_rng(arg->vx);
		vy = reslove_fixed_rng(arg->vy);
		mx = FixedMul(vx, cc);
		mx += FixedMul(vy, ss);
		my = FixedMul(vx, ss);
		my -= FixedMul(vy, cc);
	} else
	{
		mx = reslove_fixed_rng(arg->vx);
		my = reslove_fixed_rng(arg->vy);
	}

	th = P_SpawnMobj(x, y, mo->z + reslove_fixed_rng(arg->oz), arg->type);
	th->momx = mx;
	th->momy = my;
	th->momz = reslove_fixed_rng(arg->vz);
	th->angle = mo->angle;
	th->special.tid = arg->tid;

	if(arg->flags & SXF_TRANSFERPITCH)
		th->pitch = mo->pitch;

	if(!(th->flags2 & MF2_DONTTRANSLATE))
	{
		if(arg->flags & SXF_TRANSFERTRANSLATION)
			th->translation = mo->translation;
		else
		if(arg->flags & SXF_USEBLOODCOLOR)
			th->translation = mo->info->blood_trns;
	}

	if(arg->flags & SXF_TRANSFERPOINTERS)
	{
		th->target = mo->target;
		th->master = mo->master;
		th->tracer = mo->tracer;
	}

	origin = mo;
	if(!(arg->flags & SXF_ORIGINATOR))
	{
		while(origin && (origin->flags & MF_MISSILE || origin->info->flags & MF_MISSILE))
			origin = origin->target;
	}

	if(arg->flags & SXF_TELEFRAG)
		mobj_telestomp(th, x, y);

	if(th->flags1 & MF1_ISMONSTER)
	{
		if(!(arg->flags & (SXF_NOCHECKPOSITION | SXF_TELEFRAG)) && !P_CheckPosition(th, x, y))
		{
			mobj_remove(th);
			return;
		}
		// TODO: friendly monster stuff should be here
	} else
	if(!(arg->flags & SXF_TRANSFERPOINTERS))
		th->target = origin ? origin : mo;

	if(arg->flags & SXF_NOPOINTERS)
	{
		th->target = NULL;
		th->master = NULL;
		th->tracer = NULL;
	}

	if(arg->flags & SXF_SETMASTER)
		th->master = origin;

	if(arg->flags & SXF_SETTARGET)
		th->target = origin;

	if(arg->flags & SXF_SETTRACER)
		th->tracer = origin;

	if(arg->flags & SXF_TRANSFERAMBUSHFLAG)
	{
		th->flags &= ~MF_AMBUSH;
		th->flags |= mo->flags & MF_AMBUSH;
	}

	if(arg->flags & SXF_CLEARCALLERTID)
		mo->special.tid = 0;

	if(arg->flags & SXF_ISTARGET)
		mo->target = th;

	if(arg->flags & SXF_ISMASTER)
		mo->master = th;

	if(arg->flags & SXF_ISTRACER)
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

const args_DropItem_t def_DropItem =
{
	.chance = 256
};

static const dec_args_t args_DropItem =
{
	.size = sizeof(args_DropItem_t),
	.def = &def_DropItem,
	.arg =
	{
		{handle_mobjtype, offsetof(args_DropItem_t, type)},
		{handle_skip_zero, 0, 1},
		{handle_u16, offsetof(args_DropItem_t, chance), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_DropItem(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_DropItem_t *arg = st->arg;
	mobj_t *item;

	if(arg->chance < 255 && arg->chance > P_Random())
		return;

	item = P_SpawnMobj(mo->x, mo->y, mo->z + (8 << FRACBITS), arg->type);
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

static const dec_args_t args_GiveInventory =
{
	.size = sizeof(args_GiveInventory_t),
	.arg =
	{
		{handle_mobjtype, offsetof(args_GiveInventory_t, type)},
		{handle_u16, offsetof(args_GiveInventory_t, amount), 1},
		{handle_pointer, offsetof(args_GiveInventory_t, ptr), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_GiveInventory(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_GiveInventory_t *arg = st->arg;
	uint32_t count = arg->amount;
	mo = resolve_ptr(mo, arg->ptr);
	if(!mo)
		return;
	if(!count)
		count++;
	mobj_give_inventory(mo, arg->type, count);
}

//
// A_TakeInventory

static const dec_args_t args_TakeInventory =
{
	.size = sizeof(args_GiveInventory_t),
	.arg =
	{
		{handle_mobjtype, offsetof(args_GiveInventory_t, type)},
		{handle_u16, offsetof(args_GiveInventory_t, amount), 1},
		{handle_s8, offsetof(args_GiveInventory_t, sacrifice), 1}, // ignored
		{handle_pointer, offsetof(args_GiveInventory_t, ptr), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_TakeInventory(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_GiveInventory_t *arg = st->arg;
	uint32_t count = arg->amount;
	mo = resolve_ptr(mo, arg->ptr);
	if(!mo)
		return;
	if(!count)
		count = INV_MAX_COUNT;
	inventory_take(mo, arg->type, count);
}

//
// text

static const dec_args_t args_Print =
{
	.size = sizeof(print_text_t),
	.arg =
	{
		{handle_print_text, 0},
		{handle_ftics, offsetof(print_text_t, tics), 1},
		{handle_font, offsetof(print_text_t, font), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_Print(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		player_t *pl = players + i;
		if(pl->camera == mo || pl->mo == mo)
		{
			pl->text_data = st->arg;
			pl->text_tics = 0;
		}
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_PrintBold(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		player_t *pl = players + i;
		pl->text_data = st->arg;
		pl->text_tics = 0;
	}
}

//
// A_SetTranslation

static const dec_args_t args_SetTranslation =
{
	.size = sizeof(args_singlePointer_t),
	.arg =
	{
		{handle_translation, offsetof(args_singlePointer_t, value)},
		// terminator
		{NULL}
	}
};

__attribute((regparm(2),no_caller_saved_registers))
void A_SetTranslation(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_singlePointer_t *arg = st->arg;
	mo->translation = arg->value;
}

//
// A_SetScale

static const dec_args_t args_SetScale =
{
	.size = sizeof(args_singleFixed_t),
	.arg =
	{
		{handle_fixed, offsetof(args_singleFixed_t, value)},
		// terminator
		{NULL}
	}
};

__attribute((regparm(2),no_caller_saved_registers))
void A_SetScale(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_singleFixed_t *arg = st->arg;
	mo->scale = arg->value;
}

//
// A_SetRenderStyle

static const dec_arg_flag_t flags_SetRenderStyle[] =
{
	{.name = "STYLE_Normal", .bits = RS_NORMAL},
	{.name = "STYLE_Fuzzy", .bits = RS_FUZZ},
	{.name = "STYLE_Shadow", .bits = RS_SHADOW},
	{.name = "STYLE_Translucent", .bits = RS_TRANSLUCENT},
	{.name = "STYLE_Add", .bits = RS_ADDITIVE},
	{.name = "STYLE_None", .bits = RS_INVISIBLE},
	// terminator
	{NULL}
};

static const dec_args_t args_SetRenderStyle =
{
	.size = sizeof(args_SetRenderStyle_t),
	.arg =
	{
		{handle_fixed, offsetof(args_SetRenderStyle_t, alpha), 0},
		{handle_flags, offsetof(args_SetRenderStyle_t, flags), 0, flags_SetRenderStyle},
		// terminator
		{NULL}
	}
};

__attribute((regparm(2),no_caller_saved_registers))
void A_SetRenderStyle(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_SetRenderStyle_t *arg = st->arg;
	mo->render_style = arg->flags;
	if(arg->alpha >= FRACUNIT)
		mo->render_alpha = 255;
	else
	if(arg->alpha <= 0)
		mo->render_alpha = 0;
	else
		mo->render_alpha = arg->alpha >> 8;
}

//
// A_FadeOut

static const dec_args_t args_FadeOut =
{
	.size = sizeof(args_singleFixed_t),
	.arg =
	{
		{handle_fixed, offsetof(args_singleFixed_t, value), 2},
		// terminator
		{NULL}
	}
};

__attribute((regparm(2),no_caller_saved_registers))
void A_FadeOut(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_singleFixed_t *arg = st->arg;
	fixed_t sub;
	fixed_t alpha;

	if(arg)
		sub = arg->value;
	else
		sub = FRACUNIT / 10;

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

static const dec_args_t args_SetAngle =
{
	.size = sizeof(args_SetAngle_t),
	.arg =
	{
		{handle_angle_rel, offsetof(args_SetAngle_t, angle)},
		{handle_s8, offsetof(args_SetAngle_t, sacrifice), 1}, // ignored
		{handle_pointer, offsetof(args_SetAngle_t, ptr), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_SetAngle(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_SetAngle_t *arg = st->arg;
	angle_t angle = arg->angle << 1;

	if(arg->angle & 0x80000000)
		angle += mo->angle;

	mo = resolve_ptr(mo, arg->ptr);
	if(!mo)
		return;

	mo->angle = angle;
}

//
// A_SetPitch

static __attribute((regparm(2),no_caller_saved_registers))
void A_SetPitch(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_SetAngle_t *arg = st->arg;

	if(arg->angle & 0x80000000)
		// invalid
		return;

	mo = resolve_ptr(mo, arg->ptr);
	if(!mo)
		return;

	mo->pitch = -(arg->angle << 1);
}

//
// A_ChangeFlag

static const dec_args_t args_ChangeFlag =
{
	.size = sizeof(args_ChangeFlag_t),
	.arg =
	{
		{handle_mobj_flag, offsetof(args_ChangeFlag_t, moflag)},
		{handle_bool, offsetof(args_ChangeFlag_t, set)},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_ChangeFlag(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_ChangeFlag_t *arg = st->arg;
	uint32_t *fs;

	fs = (void*)mo + arg->moflag.offset;
	if(arg->set)
		*fs = *fs | arg->moflag.bits;
	else
		*fs = *fs & ~arg->moflag.bits;
}

//
// A_ChangeVelocity

static const dec_arg_flag_t flags_ChangeVelocity[] =
{
	MAKE_FLAG(CVF_RELATIVE),
	MAKE_FLAG(CVF_REPLACE),
	// terminator
	{NULL}
};

static const dec_args_t args_ChangeVelocity =
{
	.size = sizeof(args_ChangeVelocity_t),
	.arg =
	{
		{handle_fixed, offsetof(args_ChangeVelocity_t, x)},
		{handle_fixed, offsetof(args_ChangeVelocity_t, y)},
		{handle_fixed, offsetof(args_ChangeVelocity_t, z)},
		{handle_flags, offsetof(args_ChangeVelocity_t, flags), 1, flags_ChangeVelocity},
		{handle_pointer, offsetof(args_ChangeVelocity_t, ptr), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_ChangeVelocity(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_ChangeVelocity_t *arg = st->arg;
	fixed_t x, y;

	mo = resolve_ptr(mo, arg->ptr);
	if(!mo)
		return;

	if(arg->flags & CVF_REPLACE)
	{
		mo->momx = 0;
		mo->momy = 0;
		mo->momz = 0;
	}

	if(arg->flags & CVF_RELATIVE)
	{
		angle_t a = mo->angle >> ANGLETOFINESHIFT;
		x = FixedMul(arg->x, finecosine[a]);
		x -= FixedMul(arg->y, finesine[a]);
		y = FixedMul(arg->x, finesine[a]);
		y += FixedMul(arg->y, finecosine[a]);
	} else
	{
		x = arg->x;
		y = arg->y;
	}

	mo->momx += x;
	mo->momy += y;
	mo->momz += arg->z;
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

static const dec_args_t args_SetTics =
{
	.size = sizeof(args_singleDamage_t),
	.arg =
	{
		{handle_damage, offsetof(args_singleDamage_t, damage)}, // hijack 'random damage' feature
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_SetTics(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_singleDamage_t *arg = st->arg;
	uint32_t damage;

	damage = arg->damage;
	if(damage & DAMAGE_IS_CUSTOM)
		damage = mobj_calc_damage(damage);

	mo->tics = damage;
}

//
// A_RearrangePointers

static const dec_args_t args_RearrangePointers =
{
	.size = sizeof(args_RearrangePointers_t),
	.arg =
	{
		{handle_pointer, offsetof(args_RearrangePointers_t, target)},
		{handle_pointer, offsetof(args_RearrangePointers_t, master)},
		{handle_pointer, offsetof(args_RearrangePointers_t, tracer)},
		{handle_forced_string, 0, 0, .string = "PTROP_NOSAFEGUARDS"},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_RearrangePointers(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_RearrangePointers_t *arg = st->arg;
	mobj_t *target = mo->target;
	mobj_t *master = mo->master;
	mobj_t *tracer = mo->tracer;

	switch(arg->target)
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

	switch(arg->master)
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

	switch(arg->tracer)
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
// A_DamageTarget

static const dec_args_t args_DamageTarget =
{
	.size = sizeof(args_singleDamage_t),
	.arg =
	{
		{handle_damage, offsetof(args_singleDamage_t, damage)},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_DamageTarget(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_singleDamage_t *arg = st->arg;
	uint32_t damage;

	if(!mo->target)
		return;

	damage = arg->damage;
	if(damage & DAMAGE_IS_CUSTOM)
		damage = mobj_calc_damage(damage);

	mo_dmg_skip_armor = 1; // technically, in rare cases, this is broken
	mobj_damage(mo->target, mo, mo, damage, 0);
	mo_dmg_skip_armor = 0;
}

//
// A_Explode

const args_Explode_t def_Explode =
{
	.flags = XF_HURTSOURCE,
};

static const dec_arg_flag_t flags_Explode[] =
{
	MAKE_FLAG(XF_HURTSOURCE),
	MAKE_FLAG(XF_NOTMISSILE),
	MAKE_FLAG(XF_THRUSTZ),
	MAKE_FLAG(XF_NOSPLASH),
	// terminator
	{NULL}
};

static const dec_args_t args_Explode =
{
	.size = sizeof(args_Explode_t),
	.def = &def_Explode,
	.arg =
	{
		{handle_damage, offsetof(args_Explode_t, damage)},
		{handle_fixed, offsetof(args_Explode_t, distance)},
		{handle_flags, offsetof(args_Explode_t, flags), 1, flags_Explode},
		{handle_bool, offsetof(args_Explode_t, alert), 1},
		{handle_fixed, offsetof(args_Explode_t, fistance), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t PIT_Explode(mobj_t *thing)
{
	fixed_t dx, dy, dz;
	fixed_t dist;

	if(!(thing->flags & MF_SHOOTABLE))
		return 1;

	if(thing->flags1 & MF1_NORADIUSDMG)
		return 1;

	if(thing == bombsource && !(bombflags & XF_HURTSOURCE))
		return 1;

	dx = abs(thing->x - bombspot->x);
	dy = abs(thing->y - bombspot->y);

	dist = dx > dy ? dx : dy;

	dz = thing->z + thing->height;

	if(	!(thing->flags2 & MF2_OLDRADIUSDMG) &&
		!(bombsource->flags2 & MF2_OLDRADIUSDMG) &&
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

	mobj_damage(thing, bombspot, bombsource, dist, NULL);

	if(!(bombflags & XF_THRUSTZ))
		return 1;

	if(thing->flags1 & MF1_DONTTHRUST)
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
void A_Explode(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_Explode_t *arg = st->arg;
	int32_t xl, xh, yl, yh;
	fixed_t	dist;

	bombflags = arg->flags;

	dist = arg->distance + MAXRADIUS;
	yh = (mo->y + dist - bmaporgy) >> MAPBLOCKSHIFT;
	yl = (mo->y - dist - bmaporgy) >> MAPBLOCKSHIFT;
	xh = (mo->x + dist - bmaporgx) >> MAPBLOCKSHIFT;
	xl = (mo->x - dist - bmaporgx) >> MAPBLOCKSHIFT;

	bombspot = mo;
	bombdist = arg->distance;
	bombfist = arg->fistance;
	if(bombfist > bombdist)
		bombfist = bombdist;

	bombdamage = arg->damage;
	if(bombdamage & DAMAGE_IS_CUSTOM)
		bombdamage = mobj_calc_damage(bombdamage);

	if(bombflags & XF_NOTMISSILE)
		bombsource = mo;
	else
		bombsource = mo->target;

	for(int32_t y = yl; y <= yh; y++)
		for(int32_t x = xl; x <= xh; x++)
			P_BlockThingsIterator(x, y, PIT_Explode);

	if(arg->alert && bombsource && bombsource->player)
		P_NoiseAlert(bombsource, mo);
}

//
// A_Jump

static const dec_args_t args_Jump =
{
	.size = sizeof(args_Jump_t),
	.arg =
	{
		{handle_u16, offsetof(args_Jump_t, chance)},
		{handle_state, offsetof(args_Jump_t, state0)},
		{handle_state, offsetof(args_Jump_t, state1), 1},
		{handle_state, offsetof(args_Jump_t, state2), 1},
		{handle_state, offsetof(args_Jump_t, state3), 1},
		{handle_state, offsetof(args_Jump_t, state4), 1},
		{handle_state, offsetof(args_Jump_t, state5), 1},
		{handle_state, offsetof(args_Jump_t, state6), 1},
		{handle_state, offsetof(args_Jump_t, state7), 1},
		{handle_state, offsetof(args_Jump_t, state8), 1},
		{handle_state, offsetof(args_Jump_t, state9), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_Jump(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_Jump_t *arg = st->arg;
	uint32_t idx;

	if(!arg->chance)
		return;

	if(arg->chance < 256 && P_Random() >= arg->chance)
		return;

	idx = P_Random() % arg->count;
	stfunc(mo, arg->states[idx].next, arg->states[idx].extra);
}

//
// A_JumpIfInventory

static const dec_args_t args_JumpIfInventory =
{
	.size = sizeof(args_JumpIfInventory_t),
	.arg =
	{
		{handle_mobjtype_power, offsetof(args_JumpIfInventory_t, type)},
		{handle_u16, offsetof(args_JumpIfInventory_t, amount)},
		{handle_state, offsetof(args_JumpIfInventory_t, state)},
		{handle_pointer, offsetof(args_JumpIfInventory_t, ptr), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfInventory(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_JumpIfInventory_t *arg = st->arg;
	mobjinfo_t *info = mobjinfo + arg->type;
	uint32_t now, check, limit;
	mobj_t *targ;

	if(arg->type >= 0xFFF0)
	{
		// powerup check
		if(!mo->player)
			return;
		if(!mo->player->powers[arg->type - 0xFFF0])
			return;
		stfunc(mo, arg->state.next, arg->state.extra);
		return;
	}

	if(!inventory_is_valid(info))
		return;

	targ = resolve_ptr(mo, arg->ptr);
	if(!targ)
		return;

	now = inventory_check(targ, arg->type);

	if(	info->extra_type == ETYPE_AMMO &&
		targ->player &&
		targ->player->backpack
	)
		limit = info->ammo.max_count;
	else
		limit = info->inventory.max_count;

	if(arg->amount)
		check = arg->amount;
	else
		check = limit;

	if(now >= check || check > limit)
		stfunc(mo, arg->state.next, arg->state.extra);
}

//
// A_JumpIfHealthLower

static const dec_args_t args_JumpIfHealthLower =
{
	.size = sizeof(args_JumpIfHealthLower_t),
	.arg =
	{
		{handle_u16, offsetof(args_JumpIfHealthLower_t, amount)},
		{handle_state, offsetof(args_JumpIfHealthLower_t, state)},
		{handle_pointer, offsetof(args_JumpIfHealthLower_t, ptr), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfHealthLower(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_JumpIfHealthLower_t *arg = st->arg;
	uint32_t now, check, limit;
	mobj_t *targ;

	targ = resolve_ptr(mo, arg->ptr);
	if(!targ)
		return;

	if(targ->health < arg->amount)
		stfunc(mo, arg->state.next, arg->state.extra);
}

//
// A_JumpIf*Closer

static const dec_args_t args_JumpIfCloser =
{
	.size = sizeof(args_JumpIfCloser_t),
	.arg =
	{
		{handle_fixed, offsetof(args_JumpIfCloser_t, range)},
		{handle_state, offsetof(args_JumpIfCloser_t, state)},
		{handle_bool, offsetof(args_JumpIfCloser_t, no_z), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfCloser(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	// TODO: handle player weapon or inventory
	const args_JumpIfCloser_t *arg = st->arg;
	if(mobj_range_check(mo, mo->target, arg->range, !arg->no_z))
		stfunc(mo, arg->state.next, arg->state.extra);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfTracerCloser(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_JumpIfCloser_t *arg = st->arg;
	if(mobj_range_check(mo, mo->tracer, arg->range, !arg->no_z))
		stfunc(mo, arg->state.next, arg->state.extra);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfMasterCloser(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_JumpIfCloser_t *arg = st->arg;
	if(mobj_range_check(mo, mo->master, arg->range, !arg->no_z))
		stfunc(mo, arg->state.next, arg->state.extra);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfTargetInsideMeleeRange(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_singleState_t *arg = st->arg;
	if(mobj_check_melee_range(mo))
		stfunc(mo, arg->state.next, arg->state.extra);
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_JumpIfTargetOutsideMeleeRange(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_singleState_t *arg = st->arg;
	if(!mobj_check_melee_range(mo))
		stfunc(mo, arg->state.next, arg->state.extra);
}

//
// A_CheckFlag

static const dec_args_t args_CheckFlag =
{
	.size = sizeof(args_CheckFlag_t),
	.arg =
	{
		{handle_mobj_flag, offsetof(args_CheckFlag_t, moflag)},
		{handle_state, offsetof(args_CheckFlag_t, state)},
		{handle_pointer, offsetof(args_CheckFlag_t, ptr), 1},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_CheckFlag(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_CheckFlag_t *arg = st->arg;
	uint32_t *flags;
	mobj_t *targ;

	targ = resolve_ptr(mo, arg->ptr);
	if(!targ)
		return;

	flags = (void*)targ + arg->moflag.offset;
	if(*flags & arg->moflag.bits)
		stfunc(mo, arg->state.next, arg->state.extra);
}

//
// A_MonsterRefire

static const dec_args_t args_MonsterRefire =
{
	.size = sizeof(args_MonsterRefire_t),
	.arg =
	{
		{handle_u16, offsetof(args_MonsterRefire_t, chance)},
		{handle_state, offsetof(args_MonsterRefire_t, state)},
		// terminator
		{NULL}
	}
};

static __attribute((regparm(2),no_caller_saved_registers))
void A_MonsterRefire(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_MonsterRefire_t *arg = st->arg;

	if(P_Random() < arg->chance)
		return;

	if(	!mo->target ||
		mo->target->health <= 0 ||
		!P_CheckSight(mo, mo->target)
	)
		stfunc(mo, arg->state.next, arg->state.extra);
}

//
// chunks

static __attribute((regparm(2),no_caller_saved_registers))
void A_Burst(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_singleType_t *arg = st->arg;

	if(mo->player)
		engine_error("DECORATE", "A_Burst with player connected!");

	shatter_spawn(mo, arg->type);

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
	const args_singleType_t *arg = st->arg;
	mobj_t *th;

	if(!mo->player)
		return;

	if(mo->player->attacker == mo)
		mo->player->attacker = NULL;

	mo->player->viewheight = 6 * FRACUNIT;

	th = P_SpawnMobj(mo->x, mo->y, mo->z + mo->info->player.view_height, arg->type);
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
}

//
// activate line special

static int32_t parse_arg_value(mobj_t *mo, const arg_special_t *value)
{
	int32_t ret;

	ret = value->value;

	if(value->info >= 1 && value->info <= 5)
		ret += (int32_t)mo->special.arg[value->info - 1];
	else
	if(value->info == 5)
	{
		if(mo->player)
			ret += mo->player - players;
		else
			ret--;
	}

	return ret;
}

static __attribute((regparm(2),no_caller_saved_registers))
void A_LineSpecial(mobj_t *mo, state_t *st, stfunc_t stfunc)
{
	const args_lineSpecial_t *arg = st->arg;

	spec_special = arg->special;
	spec_arg[0] = parse_arg_value(mo, arg->arg + 0);
	spec_arg[1] = parse_arg_value(mo, arg->arg + 1);
	spec_arg[2] = parse_arg_value(mo, arg->arg + 2);
	spec_arg[3] = parse_arg_value(mo, arg->arg + 3);
	spec_arg[4] = parse_arg_value(mo, arg->arg + 4);

	spec_activate(NULL, mo, 0);
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
	{
		// try line specials
		const dec_linespec_t *spec = special_action;
		uint64_t alias = tp_hash64(name);

		while(spec->special)
		{
			if(spec->alias == alias)
				break;
			spec++;
		}

		if(spec->special)
		{
			args_lineSpecial_t *arg;
			uint32_t idx = 0;

			// set defaults
			arg = dec_es_alloc(sizeof(args_lineSpecial_t));
			arg->special = spec->special;
			arg->arg[0].w = 0;
			arg->arg[1].w = 0;
			arg->arg[2].w = 0;
			arg->arg[3].w = 0;
			arg->arg[4].w = 0;

			// enter function
			kw = tp_get_keyword();
			if(!kw || kw[0] != '(')
				return NULL;

			// parse
			while(1)
			{
				uint32_t value = 0;
				uint32_t tmp = 0;
				uint_fast8_t flags = 0;

				// get value
				kw = tp_get_keyword_lc();
				if(!kw)
					return NULL;

				if(idx >= 5)
					engine_error("DECORATE", "Too many arguments for action '%s' in '%s'!", name, parse_actor_name);

				// check for 'args' and 'PlayerNumber'
				if(!strcmp(kw, "args"))
				{
					kw = tp_get_keyword();
					if(!kw || kw[0] != '[')
						return NULL;

					kw = tp_get_keyword();
					if(!kw)
						return NULL;

					if(doom_sscanf(kw, "%u", &tmp) != 1 || tmp > 4)
						engine_error("DECORATE", "Unable to parse number '%s' for action '%s' in '%s'!", kw, name, parse_actor_name);

					kw = tp_get_keyword();
					if(!kw || kw[0] != ']')
						return NULL;

					// convert to arg[x]
					tmp++;

					// value is optional now
					flags = 8;

					// sign or number
					kw = tp_get_keyword();
					if(!kw)
						return NULL;
				} else
				if(!strcmp(kw, "playernumber"))
				{
					kw = tp_get_keyword();
					if(!kw || kw[0] != '(')
						return NULL;
					kw = tp_get_keyword();
					if(!kw || kw[0] != ')')
						return NULL;

					// value is optional now
					flags = 8;

					// special value
					tmp = 5;

					// sign or number
					kw = tp_get_keyword();
					if(!kw)
						return NULL;
				}

				if(kw[0] == '-')
					flags = 1 | 2 | 4;
				else
				if(kw[0] == '+')
					flags = 2 | 4;
				else
				if(flags & 8)
					tp_push_keyword(kw);
				else
					flags = 4;

				if(flags & 2)
				{
					// number
					kw = tp_get_keyword();
					if(!kw)
						return NULL;
				}

				if(flags & 4)
				{
					// parse numeric value
					if(doom_sscanf(kw, "%u", &value) != 1 || value > 0x7FFF)
						engine_error("DECORATE", "Unable to parse number '%s' for action '%s' in '%s'!", kw, name, parse_actor_name);
				}

				arg->arg[idx].value = value;
				if(flags & 1)
					arg->arg[idx].value = -arg->arg[idx].value;
				arg->arg[idx].info = tmp;
				idx++;

				// get comma or end
				kw = tp_get_keyword();
				if(!kw)
					return NULL;

				if(kw[0] == ')')
					break;

				if(kw[0] != ',')
					return NULL;
			}

			parse_action_func = A_LineSpecial;
			parse_action_arg = arg;

			// return next keyword
			return tp_get_keyword();
		}

		// not found
		engine_error("DECORATE", "Unknown action '%s' in '%s'!", name, parse_actor_name);
	}

	// action function
	parse_action_func = act->func;

	// next keyword
	kw = tp_get_keyword();
	if(!kw)
		return NULL;

	if(act->args)
	{
		const dec_arg_t *arg = act->args->arg;

		parse_action_arg = dec_es_alloc(act->args->size);

		if(act->args->def)
			memcpy(parse_action_arg, act->args->def, act->args->size);
		else
			memset(parse_action_arg, 0, act->args->size);

		action_name = name;

		if(kw[0] != '(')
		{
			if(!arg->optional)
				engine_error("DECORATE", "Missing arg[%d] for action '%s' in '%s'!", 0, name, parse_actor_name);
			if(arg->optional > 1)
			{
				parse_action_arg = NULL;
				dec_es_ptr -= act->args->size;
			}
		} else
		{
			while(arg->handler)
			{
				kw = tp_get_keyword();
				if(!kw)
					return NULL;

				if(kw[0] == ')')
				{
args_end:
					if(arg->handler && !arg->optional)
						engine_error("DECORATE", "Missing arg[%d] for action '%s' in '%s'!", arg - act->args->arg, name, parse_actor_name);
					// extra stuff
					if(parse_action_func == A_Jump)
					{
						// this is very specific handling for A_Jump
						args_Jump_t *aaa = parse_action_arg;
						uint32_t count = 0;
						for(uint32_t i = 1; i < 10; i++)
						{
							if(aaa->states[i].next || aaa->states[i].extra)
								count = i;
						}
						count++;
						aaa->count = count;
						dec_es_ptr -= sizeof(state_jump_t) * (10 - count);
					}
					// return next keyword
					return tp_get_keyword();
				}

				kw = arg->handler(kw, arg);
				if(!kw || (kw[0] != ',' && kw[0] != ')'))
					engine_error("DECORATE", "Failed to parse arg[%d] for action '%s' in '%s'!", arg - act->args->arg, name, parse_actor_name);

				arg++;

				if(kw[0] == ')')
					goto args_end;

				if(!arg->handler)
					engine_error("DECORATE", "Too many arguments for action '%s' in '%s'!", name, parse_actor_name);
			}
		}
	}

	return kw;
}

//
// table of actions

static const dec_action_t mobj_action[] =
{
	// weapon
	{"a_lower", A_Lower, &args_LowerRaise},
	{"a_raise", A_Raise, &args_LowerRaise},
	{"a_gunflash", A_GunFlash, &args_GunFlash},
	{"a_checkreload", A_CheckReload},
	{"a_light0", A_Light0},
	{"a_light1", A_Light1},
	{"a_light2", A_Light2},
	{"a_weaponready", A_WeaponReady, &args_WeaponReady},
	{"a_refire", A_ReFire, &args_ReFire},
	// sound
	{"a_pain", A_Pain},
	{"a_scream", A_Scream},
	{"a_xscream", A_XScream},
	{"a_playerscream", A_PlayerScream},
	{"a_activesound", A_ActiveSound},
	{"a_brainpain", A_BrainPain},
	{"a_startsound", A_StartSound, &args_StartSound},
	// basic control
	{"a_facetarget", A_FaceTarget},
	{"a_facetracer", A_FaceTracer},
	{"a_facemaster", A_FaceMaster},
	{"a_noblocking", A_NoBlocking},
	// "AI"
	{"a_look", A_Look},
	{"a_chase", A_Chase},
	// enemy attack
	{"a_spawnprojectile", A_SpawnProjectile, &args_SpawnProjectile},
	{"a_custombulletattack", A_CustomBulletAttack, &args_CustomBulletAttack},
	{"a_custommeleeattack", A_CustomMeleeAttack, &args_CustomMeleeAttack},
	// player attack
	{"a_fireprojectile", A_FireProjectile, &args_FireProjectile},
	{"a_firebullets", A_FireBullets, &args_FireBullets},
	{"a_custompunch", A_CustomPunch, &args_CustomPunch},
	// other attack
	{"a_bfgspray", A_BFGSpray, &args_BFGSpray},
	{"a_seekermissile", A_SeekerMissile, &args_SeekerMissile},
	// spawn
	{"a_spawnitemex", A_SpawnItemEx, &args_SpawnItemEx},
	{"a_dropitem", A_DropItem, &args_DropItem},
	// chunks
	{"a_burst", A_Burst, &args_SingleMobjtype},
	{"a_skullpop", A_SkullPop, &args_SingleMobjtype},
	// freeze death
	{"a_freezedeath", A_FreezeDeath},
	{"a_genericfreezedeath", A_GenericFreezeDeath},
	{"a_freezedeathchunks", A_FreezeDeathChunks},
	// render
	{"a_settranslation", A_SetTranslation, &args_SetTranslation},
	{"a_setscale", A_SetScale, &args_SetScale},
	{"a_setrenderstyle", A_SetRenderStyle, &args_SetRenderStyle},
	{"a_fadeout", A_FadeOut, &args_FadeOut},
	// misc
	{"a_checkplayerdone", A_CheckPlayerDone},
	{"a_alertmonsters", A_AlertMonsters},
	{"a_setangle", A_SetAngle, &args_SetAngle},
	{"a_setpitch", A_SetPitch, &args_SetAngle}, // using 'SetAngle' is not ideal
	{"a_changeflag", A_ChangeFlag, &args_ChangeFlag},
	{"a_changevelocity", A_ChangeVelocity, &args_ChangeVelocity},
	{"a_stop", A_Stop},
	{"a_settics", A_SetTics, &args_SetTics},
	{"a_rearrangepointers", A_RearrangePointers, &args_RearrangePointers},
	{"a_braindie", A_BrainDie},
	// damage
	{"a_damagetarget", A_DamageTarget, &args_DamageTarget},
	{"a_explode", A_Explode, &args_Explode},
	// inventory
	{"a_giveinventory", A_GiveInventory, &args_GiveInventory},
	{"a_takeinventory", A_TakeInventory, &args_TakeInventory},
	// text
	{"a_print", A_Print, &args_Print},
	{"a_printbold", A_PrintBold, &args_Print},
	// jumps
	{"a_jump", A_Jump, &args_Jump},
	{"a_jumpifinventory", A_JumpIfInventory, &args_JumpIfInventory},
	{"a_jumpifhealthlower", A_JumpIfHealthLower, &args_JumpIfHealthLower},
	{"a_jumpifcloser", A_JumpIfCloser, &args_JumpIfCloser},
	{"a_jumpiftracercloser", A_JumpIfTracerCloser, &args_JumpIfCloser},
	{"a_jumpifmastercloser", A_JumpIfMasterCloser, &args_JumpIfCloser},
	{"a_jumpiftargetinsidemeleerange", A_JumpIfTargetInsideMeleeRange, &args_ReFire},
	{"a_jumpiftargetoutsidemeleerange", A_JumpIfTargetOutsideMeleeRange, &args_ReFire},
	{"a_checkflag", A_CheckFlag, &args_CheckFlag},
	{"a_monsterrefire", A_MonsterRefire, &args_MonsterRefire},
	// terminator
	{NULL}
};

//
// table of line specials

static const dec_linespec_t special_action[] =
{
	{2, 0x4BF2738E392FBF77}, // polyobj_rotateleft
	{3, 0x4BF1348F08CFBF77}, // polyobj_rotateright
	{4, 0x6BED7EA8AFE6CB67}, // polyobj_move
	{7, 0xFBE47E8F157B383A}, // polyobj_doorswing
	{8, 0xFBE47E8D3D7DF83A}, // polyobj_doorslide
	{10, 0x0973BEC8DFCAFBE4}, // door_close
	{12, 0x0973A61C9FCAFBE4}, // door_raise
	{13, 0x496B8EFB88F09A2E}, // door_lockedraise
	{19, 0x0C2FD337E7BA9A34}, // thing_stop
	{20, 0x2977B7B240A342AD}, // floor_lowerbyvalue
	{23, 0x5CE98E5240A342AC}, // floor_raisebyvalue
	{26, 0xCA7589FC49DDF9A1}, // stairs_builddown
	{27, 0xCA7589FCF2AA28A1}, // stairs_buildup
	{28, 0x5CEB0BD23887D5A0}, // floor_raiseandcrush
	{31, 0xCAFB678F49DDF9A1}, // stairs_builddownsync
	{32, 0xCA758112159A28A1}, // stairs_buildupsync
	{35, 0x314ECE5240A3A195}, // floor_raisebyvaluetimes8
	{36, 0x44D0F7B240A3A194}, // floor_lowerbyvaluetimes8
	{40, 0x7B7B255A722A2BF4}, // ceiling_lowerbyvalue
	{41, 0x98E5255A722A3CAD}, // ceiling_raisebyvalue
	{44, 0x5CA37E4B16FE71AC}, // ceiling_crushstop
	{62, 0x1DED468DD0DF6F96}, // plat_downwaitupstay
	{64, 0x4A624779913FF4A3}, // plat_upwaitdownstay
	{70, 0x0000D32BF096C974}, // teleport
	{71, 0xFB9FD32BF09F26EE}, // teleport_nofog
	{72, 0x7BA9A34D33D72A36}, // thrustthing
	{73, 0x7BA9A3496786D866}, // damagething
	{74, 0x5B9FD32BF39EA4AA}, // teleport_newmap
	{76, 0x8D2FD32BF096FBE2}, // teleportother
	{97, 0x532322D83B6E256D}, // ceiling_lowerandcrushdist
	{99, 0xB37B0BD23887D77B}, // floor_raiseandcrushdoom
	{110, 0x5CE98E5246BFC3E6}, // light_raisebyvalue
	{111, 0x2977B7B246BFC3E7}, // light_lowerbyvalue
	{112, 0x7BA3FF5B73C98EFA}, // light_changetovalue
	{113, 0x09648667F4A27A6C}, // light_fade
	{114, 0x0DEFB277F4A27A6C}, // light_glow
	{116, 0x2BF2D337F4A27AFA}, // light_strobe
	{117, 0x0C2FD337F4A27A6C}, // light_stop
	{119, 0x786D8647E7BA9AA2}, // thing_damage
	{127, 0x0CF4973755A0F9A3}, // thing_setspecial
	{128, 0x7BA9A34D33D72ADE}, // thrustthingz
	{130, 0x6A748E17E7B3EEB3}, // thing_activate
	{131, 0x48E1964770F2EC93}, // thing_deactivate
	{132, 0x6BED9727E7BA9AA3}, // thing_remove
	{133, 0x2D339647E7BAA38B}, // thing_destroy
	{134, 0x5AAFCB077170EEBA}, // thing_projectile
	{135, 0xEDE1C337E7BA9A36}, // thing_spawn
	{136, 0x7375D7A07170ED27}, // thing_projectilegravity
	{137, 0xEDE1C3377943358E}, // thing_spawnnofog
	{139, 0xEDE1C3105D227BAE}, // thing_spawnfacing
	{172, 0x9FDBF624D6CEFB9C}, // plat_upnearestwaitdownstay
	{173, 0x0D3296C865CE9BEE}, // noisealert
	{176, 0x7BA1A237E5F0EEA2}, // thing_changetid
	{179, 0xCB29AF3967BA1A21}, // changeskill
	{191, 0x2C3297A1BFEA39CC}, // setplayerproperty
	{195, 0xB819670807D7B6E0}, // ceiling_crushraiseandstaya
	{196, 0x396DE6093B5AF1A4}, // ceiling_crushandraisea
	{198, 0xECE5255A7C19AA77}, // ceiling_raisebyvaluetimes8
	{199, 0x0F7B255A7C19BD2E}, // ceiling_lowerbyvaluetimes8
	{200, 0xFB267E3A7296DBD9}, // generic_floor
	{201, 0x99637E3A70ED40D5}, // generic_ceiling
	{206, 0x345D468DD0DF6F9A}, // plat_downwaitupstaylip
	{212, 0x3D25CDFCACF9D5CD}, // sector_setcolor
	{213, 0x6D25CDFCAFDB5DF5}, // sector_setfade
	{237, 0x296D863967BA1AA4}, // changecamera
	{239, 0x280ACE5240A342A2}, // floor_raisebyvaluetxty
	{243, 0xC86DCAFB9FD29E27}, // exit_normal
	{244, 0x49728E5CDFD29E26}, // exit_secret
	// terminator
	{0}
};

