// kgsws' ACE Engine
////
// Subset of DECORATE. Yeah!
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "dehacked.h"
#include "decorate.h"
#include "action.h"
#include "textpars.h"
#include "sound.h"
#include "map.h"

#include "decodoom.h"

#define NUM_INFO_HOOKS	5
#define NUM_STATE_HOOKS	7

enum
{
	DT_U16,
	DT_S32,
	DT_FIXED,
	DT_SOUND,
	DT_SKIP1,
	DT_MONSTER,
	DT_PROJECTILE,
	DT_DROPITEM,
	DT_STATES,
};

typedef struct
{
	const uint8_t *name;
	uint16_t type;
	uint16_t offset;
} dec_attr_t;

typedef struct
{
	const uint8_t *name;
	uint32_t bits;
} dec_flag_t;

typedef struct
{
	const uint8_t *name;
	uint32_t offset;
} dec_anim_t;

//

uint32_t mobj_netid;

uint32_t num_spr_names = 138;
uint32_t sprite_table[MAX_SPRITE_NAMES];

static uint32_t **spr_names;
static uint32_t *numsprites;

uint32_t num_mobj_types = NUMMOBJTYPES;
mobjinfo_t *mobjinfo;

uint32_t num_states = NUMSTATES;
state_t *states;

static hook_t hook_mobjinfo[NUM_INFO_HOOKS];
static hook_t hook_states[NUM_STATE_HOOKS];

static void *es_ptr;

uint8_t *parse_actor_name;

static void *parse_mobj_ptr;
static uint32_t *parse_next_state;
static uint32_t parse_last_anim;

static args_dropitem_t *dropitems;
static args_dropitem_t *dropitem_next;

static uint32_t parse_states();
static uint32_t parse_dropitem();

// default mobj
static const mobjinfo_t default_mobj =
{
	.doomednum = 0xFFFF,
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.mass = 100,
	.crushstate = 895,
};

// mobj animations
static const dec_anim_t mobj_anim[NUM_MOBJ_ANIMS] =
{
	[ANIM_SPAWN] = {"spawn", offsetof(mobjinfo_t, spawnstate)},
	[ANIM_SEE] = {"see", offsetof(mobjinfo_t, seestate)},
	[ANIM_PAIN] = {"pain", offsetof(mobjinfo_t, painstate)},
	[ANIM_MELEE] = {"melee", offsetof(mobjinfo_t, meleestate)},
	[ANIM_MISSILE] = {"missile", offsetof(mobjinfo_t, missilestate)},
	[ANIM_DEATH] = {"death", offsetof(mobjinfo_t, deathstate)},
	[ANIM_XDEATH] = {"xdeath", offsetof(mobjinfo_t, xdeathstate)},
	[ANIM_RAISE] = {"raise", offsetof(mobjinfo_t, raisestate)},
	[ANIM_CRUSH] = {"crush", offsetof(mobjinfo_t, crushstate)},
};

// mobj attributes
static const dec_attr_t mobj_attr[] =
{
	{"spawnid", DT_U16, offsetof(mobjinfo_t, spawnid)},
	//
	{"health", DT_S32, offsetof(mobjinfo_t, spawnhealth)},
	{"reactiontime", DT_S32, offsetof(mobjinfo_t, reactiontime)},
	{"painchance", DT_S32, offsetof(mobjinfo_t, painchance)},
	{"damage", DT_S32, offsetof(mobjinfo_t, damage)},
	{"speed", DT_FIXED, offsetof(mobjinfo_t, speed)},
	//
	{"radius", DT_FIXED, offsetof(mobjinfo_t, radius)},
	{"height", DT_FIXED, offsetof(mobjinfo_t, height)},
	{"mass", DT_S32, offsetof(mobjinfo_t, mass)},
	//
	{"dropitem", DT_DROPITEM},
	//
	{"activesound", DT_SOUND, offsetof(mobjinfo_t, activesound)},
	{"attacksound", DT_SOUND, offsetof(mobjinfo_t, attacksound)},
	{"deathsound", DT_SOUND, offsetof(mobjinfo_t, deathsound)},
	{"painsound", DT_SOUND, offsetof(mobjinfo_t, painsound)},
	{"seesound", DT_SOUND, offsetof(mobjinfo_t, seesound)},
	//
	{"monster", DT_MONSTER},
	{"projectile", DT_PROJECTILE},
	{"states", DT_STATES},
	//
	{"obituary", DT_SKIP1},
	{"hitobituary", DT_SKIP1},
	// terminator
	{NULL}
};

// mobj flags
static const dec_flag_t mobj_flag[] =
{
	// ignored flags
	{"floorclip", 0},
	{"noskin", 0},
	// flags 0
	{"special", MF_SPECIAL},
	{"solid", MF_SOLID},
	{"shootable", MF_SHOOTABLE},
	{"nosector", MF_NOSECTOR},
	{"noblockmap", MF_NOBLOCKMAP},
	{"ambush", MF_AMBUSH},
	{"justhit", MF_JUSTHIT},
	{"justattacked", MF_JUSTATTACKED},
	{"spawnceiling", MF_SPAWNCEILING},
	{"nogravity", MF_NOGRAVITY},
	{"dropoff", MF_DROPOFF},
	{"pickup", MF_PICKUP},
	{"noclip", MF_NOCLIP},
	{"slidesonwalls", MF_SLIDE},
	{"float", MF_FLOAT},
	{"teleport", MF_TELEPORT},
	{"missile", MF_MISSILE},
	{"dropped", MF_DROPPED},
	{"shadow", MF_SHADOW},
	{"noblood", MF_NOBLOOD},
	{"corpse", MF_CORPSE},
	{"countkill", MF_COUNTKILL},
	{"countitem", MF_COUNTITEM},
	{"skullfly", MF_SKULLFLY},
	{"notdmatch", MF_NOTDMATCH},
	// flags 1
	{"", 0}, // this is a separator
	{"ismonster", MF1_ISMONSTER},
	{"noteleport", MF1_NOTELEPORT},
	{"randomize", MF1_RANDOMIZE},
	{"telestomp", MF1_TELESTOMP},
	{"notarget", MF1_NOTARGET},
	{"quicktoretaliate", MF1_QUICKTORETALIATE},
	{"seekermissile", MF1_SEEKERMISSILE},
	{"noforwardfall", MF1_NOFORWARDFALL},
	// terminator
	{NULL}
};

// destination of different flags in mobjinfo_t
static const uint32_t flag_dest[] =
{
	offsetof(mobjinfo_t, flags),
	offsetof(mobjinfo_t, flags1),
};

//
// extra storage

void *dec_es_alloc(uint32_t size)
{
	void *ptr = es_ptr;

	es_ptr += size;
	if(es_ptr > EXTRA_STORAGE_PTR + EXTRA_STORAGE_SIZE)
		I_Error("[DECORATE] Extra storage allocation failed!");

	return ptr;
}

//
// funcs

static uint32_t spr_add_name(uint32_t name)
{
	uint32_t i = 0;

	for(i = 0; i < num_spr_names; i++)
	{
		if(sprite_table[i] == name)
			return i;
	}

	if(i == MAX_SPRITE_NAMES)
		I_Error("[DECORATE] Too many sprite names!");

	sprite_table[i] = name;

	num_spr_names++;

	return i;
}

static uint32_t parse_attr(uint32_t type, void *dest)
{
	num32_t num;
	uint8_t *kw;

	switch(type)
	{
		case DT_U16:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(doom_sscanf(kw, "%u", &num.u32) != 1 || num.u32 > 65535)
				return 1;
			*((uint16_t*)dest) = num.u32;
		break;
		case DT_S32:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(doom_sscanf(kw, "%d", &num.s32) != 1)
				return 1;
			*((int32_t*)dest) = num.s32;
		break;
		case DT_FIXED:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(tp_parse_fixed(kw, &num.s32))
				return 1;
			*((fixed_t*)dest) = num.s32;
		break;
		case DT_SOUND:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			*((uint16_t*)dest) = sfx_by_name(kw);
		break;
		case DT_SKIP1:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
		break;
		case DT_MONSTER:
			((mobjinfo_t*)dest)->flags |= MF_SHOOTABLE | MF_COUNTKILL | MF_SOLID;
			((mobjinfo_t*)dest)->flags1 |= MF1_ISMONSTER;
		break;
		case DT_PROJECTILE:
			((mobjinfo_t*)dest)->flags |= MF_NOBLOCKMAP | MF_NOGRAVITY | MF_DROPOFF | MF_MISSILE;
			((mobjinfo_t*)dest)->flags1 |= MF1_NOTELEPORT;
		break;
		case DT_DROPITEM:
			return parse_dropitem();
		break;
		case DT_STATES:
			num.u32 = parse_states();
			tp_enable_math = 0;
			tp_enable_newline = 0;
			return num.u32;
		break;
	}

	return 0;
}

static int32_t check_add_sprite(uint8_t *text)
{
	// make lowercase, get length
	uint32_t len = 0;
	num32_t sprname;

	while(1)
	{
		uint8_t in = *text;

		if(!in)
		{
			if(len != 4)
				return -1;
			break;
		}

		if(in >= 'a' && in <= 'z')
			sprname.u8[len] = in & ~0x20;
		else
			sprname.u8[len] = in;

		text++;
		len++;
	}

	return spr_add_name(sprname.u32);
}

static uint32_t add_states(uint32_t sprite, uint8_t *frames, int32_t tics, uint16_t flags)
{
	uint32_t tmp = num_states;

	num_states += strlen(frames);
	states = ldr_realloc(states, num_states * sizeof(state_t));

	for(uint32_t i = tmp; i < num_states; i++)
	{
		state_t *state = states + i;
		uint8_t frm;

		if(*frames < 'A' || *frames > ']')
			I_Error("[DECORATE] Invalid frame '%c' in '%s'!", *frames, parse_actor_name);

		if(parse_next_state)
			*parse_next_state = state - states;

		state->sprite = sprite;
		state->frame = (*frames - 'A') | flags;
		state->arg = parse_action_arg;
		state->tics = tics;
		state->action = parse_action_func;
		state->nextstate = 0;
		state->misc1 = 0;
		state->misc2 = 0;

		parse_next_state = &state->nextstate;

		frames++;
		state++;
	}
}

//
// dropitem parser

static uint32_t parse_dropitem()
{
	uint8_t *kw;
	int32_t tmp;
	args_dropitem_t *drop;

	// allocate space, setup destination
	drop = dec_es_alloc(sizeof(args_dropitem_t));
	if(dropitems)
	{
		if(drop != dropitem_next)
			I_Error("[DECORATE] Drop item list is not contiguous in '%s'!", parse_actor_name);
	} else
		dropitems = drop;

	dropitem_next = drop + 1;

	// defaults
	drop->chance = 255;
	drop->amount = 0;

	// get actor name
	kw = tp_get_keyword();
	if(!kw)
		return 1;

	tmp = mobj_check_type(tp_hash64(kw));
	if(tmp < 0)
		drop->type = UNKNOWN_MOBJ_IDX;
	else
		drop->type = tmp;

	// check for chance
	kw = tp_get_keyword();
	if(!kw)
		return 1;
	if(kw[0] != ',')
		goto finished;

	// parse chance
	kw = tp_get_keyword();
	if(!kw)
		return 1;
	if(doom_sscanf(kw, "%d", &tmp) != 1 || tmp < -32768 || tmp > 32767)
		return 1;

	drop->chance = tmp;

	// check for amount
	kw = tp_get_keyword();
	if(!kw)
		return 1;
	if(kw[0] != ',')
		goto finished;

	// parse amount
	kw = tp_get_keyword();
	if(!kw)
		return 1;
	if(doom_sscanf(kw, "%d", &tmp) != 1 || tmp < 0 || tmp > 65535)
		return 1;

	return 0;

finished:
	// this keyword has to be parsed again
	tp_push_keyword(kw);
	return 0;
}

//
// state parser

static uint32_t parse_states()
{
	uint8_t *kw;
	uint8_t *wk;
	int32_t sprite;
	int32_t tics;
	uint16_t flags;
	uint_fast8_t unfinished;
	uint32_t first_state = num_states;

	kw = tp_get_keyword();
	if(!kw || kw[0] != '{')
		return 1;

	// reset parser
	parse_last_anim = 0;
	parse_next_state = NULL;
	unfinished = 0;

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return 1;
have_keyword:
		if(kw[0] == '}')
			break;

		if(!strcmp(kw, "loop"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_state = parse_last_anim;
			parse_next_state = NULL;
			continue;
		} else
		if(!strcmp(kw, "stop"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_state = 0;
			parse_next_state = NULL;
			continue;
		} else
		if(!strcmp(kw, "wait"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_state = num_states - 1;
			parse_next_state = NULL;
			continue;
		}
		if(!strcmp(kw, "fail"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_state = 0;
			parse_next_state = NULL;
			continue;
		} else
		if(!strcmp(kw, "goto"))
		{
			uint32_t i;

			if(!parse_next_state)
				goto error_no_states;

			// enable math
			tp_enable_math = 1;

			// get state name
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;

			// find animation
			for(i = 0; i < NUM_MOBJ_ANIMS; i++)
			{
				if(!strcmp(kw, mobj_anim[i].name))
					break;
			}
			if(i >= NUM_MOBJ_ANIMS)
				I_Error("[DECORATE] Unknown animation '%s' in '%s'!", kw, parse_actor_name);

			// check for '+'
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			if(kw[0] == '+')
			{
				// get offset
				kw = tp_get_keyword_lc();
				if(!kw)
					return 1;

				if(doom_sscanf(kw, "%u", &tics) != 1 || tics < 0 || tics > 65535)
					I_Error("[DECORATE] Unable to parse number '%s' in '%s'!", kw, parse_actor_name);

				// next keyword
				kw = tp_get_keyword_lc();
				if(!kw)
					return 1;
			} else
				tics = 0;

			// use animation 'system'
			*parse_next_state = STATE_SET_ANIMATION(i, tics);

			// disable math
			tp_enable_math = 0;

			// done
			parse_next_state = NULL;

			// one extra keyword was already parsed
			goto have_keyword;
		}

		// sprite frames or state definition
		wk = tp_get_keyword_uc();
		if(!wk)
			return 1;

		if(wk[0] == ':')
		{
			// it's a new state
			uint32_t i;

			for(i = 0; i < NUM_MOBJ_ANIMS; i++)
			{
				if(!strcmp(kw, mobj_anim[i].name))
					break;
			}
			if(i >= NUM_MOBJ_ANIMS)
				I_Error("[DECORATE] Unknown animation '%s' in '%s'!", kw, parse_actor_name);

			parse_last_anim = num_states;
			*((uint32_t*)(parse_mobj_ptr + mobj_anim[i].offset)) = num_states;

			unfinished = 1;
			continue;
		}

		unfinished = 0;

		// it's a sprite
		sprite = check_add_sprite(kw);
		if(sprite < 0)
			I_Error("[DECORATE] Sprite name '%s' has wrong length in '%s'!", kw, parse_actor_name);

		// switch to line-based parsing
		tp_enable_newline = 1;
		// reset frame stuff
		flags = 0;
		parse_action_func = NULL;
		parse_action_arg = NULL;

		// get ticks
		kw = tp_get_keyword();
		if(!kw || kw[0] == '\n')
			return 1;

		if(doom_sscanf(kw, "%d", &tics) != 1)
			I_Error("[DECORATE] Unable to parse number '%s' in '%s'!", kw, parse_actor_name);

		// optional 'bright' or action
		kw = tp_get_keyword_lc();
		if(!kw)
			return 1;

		if(!strcmp(kw, "bright"))
		{
			// optional action
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			// set the flag
			flags |= 0x8000;
		}

		if(kw[0] != '\n')
		{
			// action
			tp_enable_math = 1;
			kw = action_parser(kw);
			if(!kw)
				return 1;
			tp_enable_math = 0;
		}

		// create states
		add_states(sprite, wk, tics, flags);

		if(kw[0] != '\n')
			I_Error("[DECORATE] Expected end of line, found '%s' in '%s'!", kw, parse_actor_name);

		// next
		tp_enable_newline = 0;
	}

	// sanity check
	if(unfinished)
		I_Error("[DECORATE] Unfinised animation in '%s'!", parse_actor_name);

	// create a loopback
	// this is how ZDoom behaves
	if(parse_next_state)
		*parse_next_state = first_state;

	// add limitation for animations
	((mobjinfo_t*)parse_mobj_ptr)->state_idx_first = first_state;
	((mobjinfo_t*)parse_mobj_ptr)->state_idx_limit = num_states;

	return 0;

error_no_states:
	I_Error("[DECORATE] Missing states for '%s' in '%s'!", kw, parse_actor_name);
}

//
// callbacks

static void cb_count_actors(lumpinfo_t *li)
{
	uint32_t idx;
	uint8_t *kw;
	uint64_t alias;

	tp_load_lump(li);

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return;

		// must get 'actor'
		if(strcmp(kw, "actor"))
			I_Error("[DECORATE] Expected 'actor', got '%s'!", kw);

		// actor name, case sensitive
		kw = tp_get_keyword();
		if(!kw)
			goto error_end;
		parse_actor_name = kw;

		// skip optional stuff
		while(1)
		{
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;

			if(kw[0] == '{')
				break;
		}

		if(tp_skip_code_block(1))
			goto error_end;

		// check for duplicate
		alias = tp_hash64(parse_actor_name);
		if(mobj_check_type(alias) >= 0)
			I_Error("[DECORATE] Multiple definitions of '%s'!", parse_actor_name);

		// add new type
		idx = num_mobj_types;
		num_mobj_types++;
		mobjinfo = ldr_realloc(mobjinfo, num_mobj_types * sizeof(mobjinfo_t));
		mobjinfo[idx].actor_name = alias;
	}

error_end:
	I_Error("[DECORATE] Incomplete definition!");
}

static void cb_parse_actors(lumpinfo_t *li)
{
	int32_t idx;
	uint8_t *kw;
	mobjinfo_t *info;
	uint64_t alias;

	tp_load_lump(li);

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return;

		// must get 'actor'
		if(strcmp(kw, "actor"))
			I_Error("[DECORATE] Expected 'actor', got '%s'!", kw);

		// actor name, case sensitive
		kw = tp_get_keyword();
		if(!kw)
			goto error_end;
		parse_actor_name = kw;

		// find mobj
		alias = tp_hash64(kw);
		idx = mobj_check_type(alias);
		if(idx < 0)
			I_Error("[DECORATE] Loading mismatch for '%s'!", kw);
		info = mobjinfo + idx;
		parse_mobj_ptr = info;

		// next, a few options
		kw = tp_get_keyword();
		if(!kw)
			goto error_end;

		if(kw[0] != '{')
		{
			// editor number
			if(doom_sscanf(kw, "%u", &idx) != 1 || (uint32_t)idx > 65535)
				goto error_numeric;

			// next keyword
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;
		}

		if(kw[0] != '{')
			I_Error("[DECORATE] Expected '{', got '%s'!", kw);

		// initialize mobj
		memcpy(info, &default_mobj, sizeof(mobjinfo_t));
		info->doomednum = idx;
		info->actor_name = alias;

		// reset stuff
		dropitems = NULL;
		dropitem_next = NULL;

		// reset extra storage
		es_ptr = EXTRA_STORAGE_PTR;

		// parse attributes
		while(1)
		{
			kw = tp_get_keyword_lc();
			if(!kw)
				goto error_end;

			// check for end
			if(kw[0] == '}')
				break;

			// check for flags
			if(kw[0] == '-' || kw[0] == '+')
			{
				const dec_flag_t *flag = mobj_flag;
				uint32_t *dest_offs = (uint32_t*)flag_dest;

				// find flag
				while(flag->name)
				{
					if(!flag->name[0])
					{
						// advance destination pointer
						dest_offs++;
					} else
					{
						if(!strcmp(flag->name, kw + 1))
							break;
					}
					flag++;
				}

				if(!flag->name)
					I_Error("[DECORATE] Unknown flag '%s'!", kw + 1);

				// destination
				dest_offs = (uint32_t*)((void*)info + *dest_offs);

				// change
				if(kw[0] == '+')
					*dest_offs |= flag->bits;
				else
					*dest_offs &= ~flag->bits;
			} else
			{
				const dec_attr_t *attr = mobj_attr;

				// find attribute
				while(attr->name)
				{
					if(!strcmp(attr->name, kw))
						break;
					attr++;
				}

				if(!attr->name)
					I_Error("[DECORATE] Unknown attribute '%s' in '%s'!", kw, parse_actor_name);

				// parse it
				if(parse_attr(attr->type, (void*)info + attr->offset))
					I_Error("[DECORATE] Unable to parse value of '%s' in '%s'!", kw, parse_actor_name);
			}
		}

		// save drop item list
		info->dropitems = dropitems;
		info->dropitem_end = dropitem_next;

		// process extra storage
		if(es_ptr != EXTRA_STORAGE_PTR)
		{
			// allocate extra space .. in states!
			uint32_t size;
			uint32_t idx = num_states;

			size = es_ptr - EXTRA_STORAGE_PTR;
			size += sizeof(state_t) - 1;
			size /= sizeof(state_t);

			num_states += size;
			states = ldr_realloc(states, num_states * sizeof(state_t));

			// copy all the stuff
			memcpy(states + idx, EXTRA_STORAGE_PTR, es_ptr - EXTRA_STORAGE_PTR);

			// relocation has to be done later
		}
	}

error_end:
	I_Error("[DECORATE] Incomplete definition!");
error_numeric:
	I_Error("[DECORATE] Unable to parse number '%s' in '%s'!", kw, parse_actor_name);
}

//
// API

int32_t mobj_check_type(uint64_t alias)
{
	for(uint32_t i = 0; i < num_mobj_types; i++)
	{
		if(mobjinfo[i].actor_name == alias)
			return i;
	}

	return -1;
}

void init_decorate()
{
	// To avoid unnecessary memory fragmentation, this function does multiple passes.
	doom_printf("[ACE] init DECORATE\n");
	ldr_alloc_message = "Decorate memory allocation failed!";

	// init sprite names
	for(uint32_t i = 0; i < num_spr_names; i++)
		sprite_table[i] = *spr_names[i];

	// mobjinfo
	mobjinfo = ldr_malloc(NUMMOBJTYPES * sizeof(mobjinfo_t));

	// copy original stuff
	for(uint32_t i = 0; i < NUMMOBJTYPES; i++)
	{
		memset(mobjinfo + i, 0, sizeof(mobjinfo_t));
		memcpy(mobjinfo + i, deh_mobjinfo + i, sizeof(deh_mobjinfo_t));
		mobjinfo[i].actor_name = doom_actor_name[i];
		mobjinfo[i].spawnid = doom_spawn_id[i];
		mobjinfo[i].state_idx_limit = NUMSTATES;
		mobjinfo[i].crushstate = 895;

		// basically everything is randomized
		mobjinfo[i].flags1 = MF1_RANDOMIZE;

		// mark enemies
		if(mobjinfo[i].flags & MF_COUNTKILL)
			mobjinfo[i].flags1 |= MF1_ISMONSTER;

		// modify projectiles
		if(mobjinfo[i].flags & MF_MISSILE)
			mobjinfo[i].flags1 |= MF1_NOTELEPORT;
	}

	// player stuff
	mobjinfo[0].flags1 |= MF1_TELESTOMP;

	// lost souls are enemy too
	mobjinfo[18].flags1 |= MF1_ISMONSTER;

	// archvile stuff
	mobjinfo[3].flags1 |= MF1_NOTARGET | MF1_QUICKTORETALIATE;

	//
	// PASS 1

	// count actors, allocate actor names
	wad_handle_lump("DECORATE", cb_count_actors);

	//
	// PASS 2

	// states
	states = ldr_malloc(NUMSTATES * sizeof(state_t));

	// copy original stuff
	for(uint32_t i = 0; i < NUMSTATES; i++)
	{
		states[i].sprite = deh_states[i].sprite;
		states[i].frame = deh_states[i].frame;
		states[i].arg = NULL;
		states[i].tics = deh_states[i].tics;
		states[i].action = deh_states[i].action;
		states[i].nextstate = deh_states[i].nextstate;
		states[i].misc1 = deh_states[i].misc1;
		states[i].misc2 = deh_states[i].misc2;
	}

	// process actors
	wad_handle_lump("DECORATE", cb_parse_actors);

	//
	// PASS 3

	// relocate extra storage
	for(uint32_t idx = 0; idx < num_mobj_types; idx++)
	{
		mobjinfo_t *info = mobjinfo + idx;
		void *target = states + info->state_idx_limit;

		// state arguments
		for(uint32_t i = info->state_idx_first; i < info->state_idx_limit; i++)
		{
			state_t *st = states + i;
			if(st->arg)
				st->arg = target + (st->arg - EXTRA_STORAGE_PTR);
		}

		// drop item list
		if(info->dropitems)
		{
			info->dropitems = target + (info->dropitems - EXTRA_STORAGE_PTR);
			info->dropitem_end = target + (info->dropitem_end - EXTRA_STORAGE_PTR);
		}
	}

	//
	// DONE

	// patch CODE - 'mobjinfo'
	for(uint32_t i = 0; i < NUM_INFO_HOOKS; i++)
		hook_mobjinfo[i].value += (uint32_t)mobjinfo;
	utils_install_hooks(hook_mobjinfo, NUM_INFO_HOOKS);

	// patch CODE - 'states'
	for(uint32_t i = 0; i < NUM_STATE_HOOKS; i++)
		hook_states[i].value += (uint32_t)states;
	utils_install_hooks(hook_states, NUM_STATE_HOOKS);

	// done
	*numsprites = num_spr_names;
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t set_mobj_animation(mobj_t *mo, uint8_t anim)
{
	mobj_set_state(mo, STATE_SET_ANIMATION(anim, 0));
}

static __attribute((regparm(2),no_caller_saved_registers))
void prepare_mobj(mobj_t *mo)
{
	mo->flags1 = mo->info->flags1;
	mo->netid = mobj_netid++;
	P_SetThingPosition(mo);
}

static __attribute((regparm(2),no_caller_saved_registers))
void touch_mobj(mobj_t *mo, mobj_t *toucher)
{
	// old items; workaround for 'sprite' changes in 'mobj_t'
	uint16_t temp = mo->frame;
	mo->frame = 0;
	P_TouchSpecialThing(mo, toucher);
	mo->frame = temp;
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t mobj_set_state(mobj_t *mo, uint32_t state)
{
	state_t *st;

	do
	{
		if(state & 0x80000000)
		{
			// change animation
			const dec_anim_t *anim;
			uint16_t offset;

			offset = state & 0xFFFF;
			mo->animation = (state >> 16) & 0xFF;

			anim = mobj_anim + mo->animation;

			state = *((uint32_t*)((void*)mo->info + anim->offset));
			if(state)
				state += offset;

			if(state >= mo->info->state_idx_limit)
				I_Error("[MOBJ] State set '%s + %u' is invalid!", anim->name, offset);
		}

		if(!state)
		{
			P_RemoveMobj(mo);
			return 0;
		}

		st = states + state;
		mo->state = st;
		mo->sprite = st->sprite;
		mo->frame = st->frame;
		mo->tics = st->tics;
		state = st->nextstate;

		if(st->acp)
			st->acp(mo);

	} while(!mo->tics);

	return 1;
}

__attribute((regparm(2),no_caller_saved_registers))
static void kill_animation(mobj_t *mo)
{
	// custom death animations can be added

	if(mo->info->xdeathstate && mo->health < -mo->info->spawnhealth)
		set_mobj_animation(mo, ANIM_XDEATH);
	else
		set_mobj_animation(mo, ANIM_DEATH);

	if(mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}
}

__attribute((regparm(2),no_caller_saved_registers))
void explode_missile(mobj_t *mo)
{
	mo->momx = 0;
	mo->momy = 0;
	mo->momz = 0;

	set_mobj_animation(mo, ANIM_DEATH);

	if(mo->flags1 & MF1_RANDOMIZE && mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	mo->flags &= ~MF_MISSILE;

	S_StartSound(mo, mo->info->deathsound);
}

void mobj_damage(mobj_t *target, mobj_t *source, mobj_t *origin, uint32_t damage, uint32_t extra)
{
	// target = what is damaged
	// source = damage source (projectile or ...)
	// origin = what is responsible
	player_t *player;

	if(!(target->flags & MF_SHOOTABLE))
		return;

	if(target->health <= 0)
		return;

	if(target->flags & MF_SKULLFLY)
	{
		target->momx = 0;
		target->momy = 0;
		target->momz = 0;
	}

	player = target->player;

	if(player && !*gameskill)
		damage /= 2;

	if(	source &&
		!(target->flags & MF_NOCLIP) &&
		(!origin || !origin->player || origin->player->readyweapon != 7) // chainsaw hack // TODO: replace with weapon.kickback
	) {
		angle_t angle;
		uint32_t thrust;

		thrust = damage > 10000 ? 10000 : damage;

		angle = R_PointToAngle2(source->x, source->y, target->x, target->y);
		thrust = thrust * (FRACUNIT >> 3) * 100 / target->info->mass;

		if(	!(target->flags1 & MF1_NOFORWARDFALL) &&
			(!source || !(source->flags1 & MF1_NOFORWARDFALL) ) && // TODO: extra steps for hitscan
			damage < 40 &&
			damage > target->health &&
			target->z - source->z > 64 * FRACUNIT &&
			P_Random() & 1
		) {
			angle += ANG180;
			thrust *= 4;
		}

		angle >>= ANGLETOFINESHIFT;
		target->momx += FixedMul(thrust, finecosine[angle]);
		target->momy += FixedMul(thrust, finesine[angle]);
	}

	if(player)
	{
		if(target->subsector->sector->special == 11 && damage >= target->health)
			damage = target->health - 1;

		if(	damage < 1000000 &&
			( player->cheats & CF_GODMODE ||
			player->powers[pw_invulnerability] )
		)
			return;

		if(player->armortype)
		{
			uint32_t saved;

			saved = (player->armortype == 1) ? (damage / 3) : (damage / 2);
			if(player->armorpoints <= saved)
			{
				saved = player->armorpoints;
				player->armortype = 0;
			}

			player->armorpoints -= saved;
			damage -= saved;
		}

		player->health -= damage;
		if(player->health < 0)
			player->health = 0;

		player->attacker = origin;

		player->damagecount += damage;
		if(player->damagecount > 100)
			player->damagecount = 100;

		// I_Tactile ...
	}

	target->health -= damage;
	if(target->health <= 0)
	{
		P_KillMobj(origin, target);
		return;
	}

	if(	P_Random() < target->info->painchance &&
		!(target->flags & MF_SKULLFLY)
	) {
		target->flags |= MF_JUSTHIT;
		mobj_set_state(target, STATE_SET_ANIMATION(ANIM_PAIN, 0));
	}

	target->reactiontime = 0;

	if(	(!target->threshold || target->flags1 & MF1_QUICKTORETALIATE) &&
		origin && origin != target &&
		!(origin->flags1 & MF1_NOTARGET)
	) {
		target->target = origin;
		target->threshold = 100;
		if(target->info->seestate && target->state == states + target->info->spawnstate)
			mobj_set_state(target, STATE_SET_ANIMATION(ANIM_SEE, 0));
	}
}

//
// hooks

static hook_t hook_mobjinfo[NUM_INFO_HOOKS] =
{
	{0x00031587, CODE_HOOK | HOOK_UINT32, 0}, // P_SpawnMobj
	{0x00031792, CODE_HOOK | HOOK_UINT32, 0x55}, // P_RespawnSpecials
	{0x00031A34, CODE_HOOK | HOOK_UINT32, 0x57}, // P_SpawnMapThing
	{0x00031A56, CODE_HOOK | HOOK_UINT32, offsetof(mobjinfo_t, flags1)}, // P_SpawnMapThing
	{0x00031A74, CODE_HOOK | HOOK_UINT32, 0x55}, // P_SpawnMapThing
};

static hook_t hook_states[NUM_STATE_HOOKS] =
{
	{0x0002A72F, CODE_HOOK | HOOK_UINT32, 0}, // P_DamageMobj
	{0x0002D062, CODE_HOOK | HOOK_UINT32, 0}, // P_SetPsprite
	{0x000315D9, CODE_HOOK | HOOK_UINT32, 0}, // P_SpawnMobj
	{0x0002D2FF, CODE_HOOK | HOOK_UINT32, 0x10D8}, // A_WeaponReady
	{0x0002D307, CODE_HOOK | HOOK_UINT32, 0x10F4}, // A_WeaponReady
	{0x0002D321, CODE_HOOK | HOOK_UINT32, 0x0754}, // A_WeaponReady
	{0x0002DAF3, CODE_HOOK | HOOK_UINT32, 0x05B0}, // A_FireCGun
};

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x00015800, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spr_names},
	{0x0005C8E0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsprites},
	// replace call to 'P_SetThingPosition' in 'P_SpawnMobj'
	{0x000315FC, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)prepare_mobj},
	// replace call to 'P_TouchSpecialThing' in 'PIT_CheckThing'
	{0x0002B031, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)touch_mobj},
	// replace call to 'P_SetMobjState' in 'P_MobjThinker'
	{0x000314F0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)mobj_set_state},
	// fix jump condition in 'P_SetMobjState'
	{0x000314E4, CODE_HOOK | HOOK_UINT8, 0x7F},
	// replace 'P_SetMobjState'
	{0x00030EA0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)mobj_set_state},
	// replace 'P_DamageMobj' - use trampoline
	{0x0002A460, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)hook_mobj_damage},
	// extra stuff in 'P_KillMobj' - replaces animation change
	{0x0002A3C8, CODE_HOOK | HOOK_UINT16, 0xD889},
	{0x0002A3CA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)kill_animation},
	{0x0002A3CF, CODE_HOOK | HOOK_JMP_DOOM, 0x0002A40D},
	// replace 'P_ExplodeMissile'
	{0x00030F00, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)explode_missile},
	// use 'MF1_ISMONSTER' in 'P_SpawnMapThing'
	{0x00031A5A, CODE_HOOK | HOOK_UINT8, MF1_ISMONSTER},
	// use 'MF1_NOTELEPORT' in 'EV_Teleport'
	{0x00031E4D, CODE_HOOK | HOOK_UINT8, offsetof(mobj_t, flags1)},
	{0x00031E4E, CODE_HOOK | HOOK_UINT8, MF1_NOTELEPORT},
	// use 'MF1_TELESTOMP' in 'PIT_StompThing'
	{0x0002ABC7, CODE_HOOK | HOOK_UINT16, 0x43F6},
	{0x0002ABC9, CODE_HOOK | HOOK_UINT8, offsetof(mobj_t, flags1)},
	{0x0002ABCA, CODE_HOOK | HOOK_UINT8, MF1_TELESTOMP},
	{0x0002ABCB, CODE_HOOK | HOOK_SET_NOPS, 3},
	// change 'mobj_t' size
	{0x00031552, CODE_HOOK | HOOK_UINT32, sizeof(mobj_t)},
	{0x00031563, CODE_HOOK | HOOK_UINT32, sizeof(mobj_t)},
	// change 'mobjinfo_t' size
	{0x00031574, CODE_HOOK | HOOK_UINT8, sizeof(mobjinfo_t)},
	{0x0003178F, CODE_HOOK | HOOK_UINT8, sizeof(mobjinfo_t)},
	{0x00030F10, CODE_HOOK | HOOK_UINT8, sizeof(mobjinfo_t)},
	{0x00031A31, CODE_HOOK | HOOK_UINT8, sizeof(mobjinfo_t)},
	{0x00031A53, CODE_HOOK | HOOK_UINT8, sizeof(mobjinfo_t)},
	{0x00031A63, CODE_HOOK | HOOK_UINT8, sizeof(mobjinfo_t)},
	// change 'sprite' of 'mobj_t' in 'R_PrecacheLevel'
	{0x00034992, CODE_HOOK | HOOK_UINT32, 0x2443B70F},
	{0x00034996, CODE_HOOK | HOOK_UINT32, 0x8B012488},
	// fix 'R_ProjectSprite'; use new 'frame' and 'state', ignore invalid sprites
	{0x00037D45, CODE_HOOK | HOOK_UINT32, 0x2446B70F},
	{0x00037D49, CODE_HOOK | HOOK_UINT32, 0x1072E839},
	{0x00037D4D, CODE_HOOK | HOOK_JMP_DOOM, 0x00037F86},
	{0x00037D6C, CODE_HOOK | HOOK_UINT32, 0x2656B70F},
	{0x00037D70, CODE_HOOK | HOOK_UINT32, 0xE283038B},
	{0x00037D74, CODE_HOOK | HOOK_UINT32, 0x7CC2393F},
	{0x00037D78, CODE_HOOK | HOOK_UINT8, 0x1F},
	{0x00037D79, CODE_HOOK | HOOK_JMP_DOOM, 0x00037F86},
	{0x00037F59, CODE_HOOK | HOOK_UINT8, 0x27},
	// fix 'R_DrawPSprite'; use new 'frame' and 'state', ignore invalid sprites
	{0x00038023, CODE_HOOK | HOOK_UINT32, 0x3910B70F},
	{0x00038027, CODE_HOOK | HOOK_UINT32, 0x1172DA},
	{0x0003802A, CODE_HOOK | HOOK_JMP_DOOM, 0x00038203},
	{0x0003804A, CODE_HOOK | HOOK_UINT32, 0x0258B70F},
	{0x0003804E, CODE_HOOK | HOOK_UINT32, 0xE3833A8B},
	{0x00038052, CODE_HOOK | HOOK_UINT32, 0x7CFB393F},
	{0x00038056, CODE_HOOK | HOOK_UINT8, 0x21},
	{0x00038057, CODE_HOOK | HOOK_JMP_DOOM, 0x00038203},
	{0x000381DD, CODE_HOOK | HOOK_UINT8, 0x03},
	// replace 'P_SetMobjState' with new animation system
	{0x00027776, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_SEE << 8)}, // A_Look
	{0x00027779, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Look
	{0x0002782A, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_SPAWN << 8)}, // A_Chase
	{0x0002782D, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	{0x0002789D, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_MELEE << 8)}, // A_Chase
	{0x000278A0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	{0x000278DD, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_MISSILE << 8)}, // A_Chase
	{0x000278E0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	// replace 'P_SetMobjState' with new animation system // P_DamageMobj will be replaced //
	{0x0002A6D7, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_PAIN << 8)},
	{0x0002A6DA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation},
	{0x0002A73E, CODE_HOOK | HOOK_UINT16, 0x00b2 | (ANIM_SEE << 8)},
	{0x0002A742, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation},
	// replace 'P_SetMobjState' with new animation system (PIT_CheckThing, MF_SKULLFLY)
	{0x0002AF2F, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_SPAWN << 8)},
	{0x0002AF32, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation},
	// replace 'P_SetMobjState' with new animation system (PIT_ChangeSector, gibs)
	{0x0002BEBA, CODE_HOOK | HOOK_UINT8, ANIM_CRUSH},
	{0x0002BEC0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation},
	// replace 'P_SetMobjState' with new animation system (P_MovePlayer)
	{0x0003324B, CODE_HOOK | HOOK_UINT16, 0xB880},
	{0x0003324D, CODE_HOOK | HOOK_UINT32, offsetof(mobj_t, animation)},
	{0x00033251, CODE_HOOK | HOOK_UINT8, ANIM_SPAWN}, // check for
	{0x00033255, CODE_HOOK | HOOK_UINT32, ANIM_SEE}, // replace with
	{0x00033259, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation},
};

