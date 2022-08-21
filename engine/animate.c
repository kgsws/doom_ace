// kgsws' ACE Engine
////
// Subset of ANIMDEFS support.
// - flat animations
// - texture animations
// - switches
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "textpars.h"
#include "animate.h"
#include "sound.h"
#include "map.h"
#include "ldr_texture.h"
#include "ldr_flat.h"

enum
{
	ANIM_TYPE_FLAT_SINGLE,
	ANIM_TYPE_FLAT_RANGE,
	ANIM_TYPE_TEXTURE_SINGLE,
	ANIM_TYPE_TEXTURE_RANGE,
	//
	ANIM_TERMINATOR = 0xFF
};

typedef struct
{
	uint16_t pic;
	uint16_t tick;
} animframe_t;

typedef struct
{
	uint16_t tick_total;
	animframe_t frame[];
} animt_single_t;

typedef struct
{
	uint8_t type;
	uint8_t count;
	uint16_t target;
} anim_header_t;

typedef struct
{
	uint16_t tics;
	uint16_t pic[];
} animt_range_t;

typedef struct
{
	anim_header_t head;
	union
	{
		animt_single_t single;
		animt_range_t range;
	};
} animation_t;

//

typedef struct aswitch_s
{
	uint8_t terminator;
	uint8_t count;
	uint16_t target;
	uint32_t tick_total;
	uint32_t sound;
	struct aswitch_s *reverse;
	animframe_t frame[];
} aswitch_t;

//

typedef struct
{
	uint16_t *dest;
	aswitch_t *swtch;
	uint32_t base;
	degenmobj_t *soundorg;
	uint16_t animate;
	uint16_t delay;
} switch_t;

//
static void *anim_ptr;
static void *switch_ptr;
static void *animations;

static switch_t active_switch[MAX_BUTTONS];

// built-in animations
static uint8_t engine_animdefs[] =
"flat	BLOOD1	r BLOOD3 t 8\n"
"flat	FWATER1	r FWATER4 t 8\n"
"flat	LAVA1	r LAVA4 t 8\n"
"flat	NUKAGE1	r NUKAGE3 t 8\n"
"flat	SLIME01	r SLIME04 t 8\n"
"flat	SLIME05	r SLIME08 t 8\n"
"flat	SLIME09	r SLIME12 t 8\n"
"flat	SWATER1	r SWATER4 t 8\n"
"texture	BFALL1	r BFALL4 t 8\n"
"texture	BLODGR1	r BLODGR4 t 8\n"
"texture	BLODRIP1	r BLODRIP4 t 8\n"
"texture	DBRAIN1	r DBRAIN4 t 8\n"
"texture	FIREBLU1	r FIREBLU2 t 8\n"
"texture	FIRELAV3	r FIRELAVA t 8\n"
"texture	FIREMAG1	r FIREMAG3 t 8\n"
"texture	FIREWALA	r FIREWALL t 8\n"
"texture	GSTFONT1	r GSTFONT3 t 8\n"
"texture	ROCKRED1	r ROCKRED3 t 8\n"
"texture	SFALL1	r SFALL4 t 8\n"
"texture	SLADRIP1	r SLADRIP3 t 8\n"
"texture	WFALL1	r WFALL4 t 8\n"
"switch	SW1BRCOM on p SW2BRCOM t 0\n"
"switch	SW1BRN1 on p SW2BRN1 t 0\n"
"switch	SW1BRN2 on p SW2BRN2 t 0\n"
"switch	SW1BRNGN on p SW2BRNGN t 0\n"
"switch	SW1BROWN on p SW2BROWN t 0\n"
"switch	SW1COMM on p SW2COMM t 0\n"
"switch	SW1COMP on p SW2COMP t 0\n"
"switch	SW1DIRT on p SW2DIRT t 0\n"
"switch	SW1EXIT on p SW2EXIT t 0\n"
"switch	SW1GRAY on p SW2GRAY t 0\n"
"switch	SW1GRAY1 on p SW2GRAY1 t 0\n"
"switch	SW1METAL on p SW2METAL t 0\n"
"switch	SW1PIPE on p SW2PIPE t 0\n"
"switch	SW1SLAD on p SW2SLAD t 0\n"
"switch	SW1STARG on p SW2STARG t 0\n"
"switch	SW1STON1 on p SW2STON1 t 0\n"
"switch	SW1STON2 on p SW2STON2 t 0\n"
"switch	SW1STONE on p SW2STONE t 0\n"
"switch	SW1STRTN on p SW2STRTN t 0\n"
"switch	SW1BLUE on p SW2BLUE t 0\n"
"switch	SW1CMT on p SW2CMT t 0\n"
"switch	SW1GARG on p SW2GARG t 0\n"
"switch	SW1GSTON on p SW2GSTON t 0\n"
"switch	SW1HOT on p SW2HOT t 0\n"
"switch	SW1LION on p SW2LION t 0\n"
"switch	SW1SATYR on p SW2SATYR t 0\n"
"switch	SW1SKIN on p SW2SKIN t 0\n"
"switch	SW1VINE on p SW2VINE t 0\n"
"switch	SW1WOOD on p SW2WOOD t 0\n"
"switch	SW1PANEL on p SW2PANEL t 0\n"
"switch	SW1ROCK on p SW2ROCK t 0\n"
"switch	SW1MET2 on p SW2MET2 t 0\n"
"switch	SW1WDMET on p SW2WDMET t 0\n"
"switch	SW1BRIK on p SW2BRIK t 0\n"
"switch	SW1MOD1 on p SW2MOD1 t 0\n"
"switch	SW1ZIM on p SW2ZIM t 0\n"
"switch	SW1STON6 on p SW2STON6 t 0\n"
"switch	SW1TEK on p SW2TEK t 0\n"
"switch	SW1MARB on p SW2MARB t 0\n"
"switch	SW1SKULL on p SW2SKULL t 0\n"
;

//
// funcs

uint32_t switch_line_texture(aswitch_t *swtch, uint16_t *dest, uint32_t diff, int32_t state)
{
	animframe_t *frame;

	// tick offset
	if(swtch->tick_total)
	{
		// find current frame
		for(uint32_t i = 0; i < swtch->count; i++)
		{
			if(swtch->frame[i].tick <= diff)
				frame = swtch->frame + i;
			else
				break;
		}
	} else
	{
		// it's a last frame
		frame = swtch->frame + swtch->count - 1;
	}

	if(state >= 0)
	{
		// this is a first step
		uint32_t animate;

		// check for animation
		animate = frame != swtch->frame + swtch->count - 1;

		if(animate || state)
		{
			// add this switch to active buttons
			switch_t *slot = NULL;

			for(uint32_t i = 0; i < MAX_BUTTONS; i++)
			{
				switch_t *active = active_switch + i;

				if(active->dest == dest)
				{
					// force instant reverse
					active->delay = 1;
					return 0;
				}

				if(!active->dest)
					// found free slot
					slot = active;
			}

			if(slot)
			{
				slot->dest = dest;
				slot->swtch = swtch;
				slot->base = *leveltime;
				slot->soundorg = NULL; // TODO: origin from line
				slot->delay = state ? BUTTON_TIME + swtch->tick_total : 0;
				slot->animate = animate;
			}
		}

		// sound
		S_StartSound(NULL, swtch->sound); // TODO: origin from line

		// change texture
		*dest = frame->pic;

		return 0;
	}

	// change texture
	*dest = frame->pic;

	// return 1 if on last frame
	return frame != swtch->frame + swtch->count - 1;
}

//
// parser

static void parse_animdefs()
{
	union
	{
		animation_t *anim;
		aswitch_t *swtch;
	} dest;
	uint8_t *kw, *name;
	int32_t target;
	uint32_t skip = 0;
	int32_t (*get_pic)(uint8_t*) __attribute((regparm(2),no_caller_saved_registers));

	while(1)
	{
		kw = tp_get_keyword();
continue_keyword:
		if(!kw)
			return;

		if(!strcmp(kw, "flat"))
			get_pic = flat_num_check;
		else
		if(!strcmp(kw, "texture"))
			get_pic = texture_num_check;
		else
		if(!strcmp(kw, "switch"))
		{
			int32_t num;
			uint32_t tick_total;
			uint32_t count;
			uint16_t sound = 23;
			animframe_t *frame;
			aswitch_t *reverse;

			skip = 0;

			// get picture name
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;

			target = texture_num_check(kw);
			if(target < 0)
			{
				// this does not exist
				skip = 1;
				continue;
			}
			name = kw;

			// expecting 'on'
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;
			if(*((uint16_t*)kw) != 0x6E6F)
				I_Error("[ANIMDEFS] Expected keyword 'ok' found '%s' in '%s'!", kw, name);

			// expecting 'pic' or 'sound'
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;

			if(kw[0] == 's')
			{
				// get sound name
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;

				sound = sfx_by_name(kw);

				// read next keyword
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;
			}

			if(kw[0] != 'p')
				I_Error("[ANIMDEFS] Expected keyword 'pic' found '%s' in '%s'!", kw, name);

			// switch
			dest.swtch = switch_ptr;

			// offset: header
			switch_ptr += sizeof(aswitch_t);

			// reset
			tick_total = 0;
			count = 0;

			// parse all frames
			while(1)
			{
				frame = switch_ptr;

				// offset: frame
				switch_ptr += sizeof(animframe_t);

				// get picture name
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;
				num = texture_num_check(kw);
				if(num < 0)
					I_Error("[ANIMDEFS] frame '%s' not found for '%s'\n", kw, name);

				if(animations)
				{
					frame->pic = num;
					frame->tick = tick_total;
				}

				// only 'tics' is supported; no 'rand'
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;
				if(kw[0] != 't')
					goto error_tics;

				// get tick count
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;
				if(doom_sscanf(kw, "%u", &num) != 1 || num < 0) // zero can be used on last frame
					goto error_numeric;

				// add this info
				count++;
				tick_total += num;

				// check for next frame
				kw = tp_get_keyword();
				if(!kw)
					break;
				if(!kw || kw[0] != 'p')
					break;
			}

			// checks
			if(tick_total > 65535)
				goto error_tic_count;
			if(count > 255)
				goto error_anim_count;

			// store switch info
			if(animations)
			{
				dest.swtch->terminator = 0;
				dest.swtch->count = count;
				dest.swtch->target = target;
				dest.swtch->tick_total = tick_total;
				dest.swtch->sound = sound;
				dest.swtch->reverse = switch_ptr;

				// store stuff for reversion
				skip = frame->pic;
				reverse = dest.swtch;
			}

			// check for 'off'
			if(kw && *((uint32_t*)kw) == 0x0066666F)
			{
				// expecting 'pic'
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;

				if(kw[0] == 's')
				{
					// get sound name
					kw = tp_get_keyword();
					if(!kw)
						goto error_end;

					sound = sfx_by_name(kw);

					// read next keyword
					kw = tp_get_keyword();
					if(!kw)
						goto error_end;
				}

				if(kw[0] != 'p')
					I_Error("[ANIMDEFS] Expected keyword 'pic' found '%s' in '%s'!", kw, name);

				// switch
				dest.swtch = switch_ptr;

				// offset: header
				switch_ptr += sizeof(aswitch_t);

				// reset
				tick_total = 0;
				count = 0;

				// parse all frames
				while(1)
				{
					frame = switch_ptr;

					// offset: frame
					switch_ptr += sizeof(animframe_t);

					// get picture name
					kw = tp_get_keyword();
					if(!kw)
						goto error_end;
					num = texture_num_check(kw);
					if(num < 0)
						I_Error("[ANIMDEFS] frame '%s' not found for '%s'\n", kw, name);

					if(animations)
					{
						frame->pic = num;
						frame->tick = tick_total;
					}

					// only 'tics' is supported; no 'rand'
					kw = tp_get_keyword();
					if(!kw)
						goto error_end;
					if(kw[0] != 't')
						goto error_tics;

					// get tick count
					kw = tp_get_keyword();
					if(!kw)
						goto error_end;
					if(doom_sscanf(kw, "%u", &num) != 1 || num < 0) // zero can be used on last frame
						goto error_numeric;

					// add this info
					count++;
					tick_total += num;

					// check for next frame
					kw = tp_get_keyword();
					if(!kw)
						break;
					if(!kw || kw[0] != 'p')
						break;
				}

				// checks
				if(tick_total > 65535)
					goto error_tic_count;
				if(count > 255)
					goto error_anim_count;

				// store switch info
				if(animations)
				{
					dest.swtch->terminator = 0;
					dest.swtch->count = count;
					dest.swtch->target = skip; // this was stored above
					dest.swtch->tick_total = tick_total;
					dest.swtch->sound = sound;
					dest.swtch->reverse = reverse; // this was stored above
				}
			} else
			{
				// generate simple reverse sequence

				// switch
				dest.swtch = switch_ptr;

				// offset: header + frame
				switch_ptr += sizeof(aswitch_t) + sizeof(animframe_t);

				if(animations)
				{
					dest.swtch->terminator = 0;
					dest.swtch->count = 1;
					dest.swtch->target = skip; // this was stored above
					dest.swtch->tick_total = 1;
					dest.swtch->sound = sound;
					dest.swtch->reverse = reverse; // this was stored above
					dest.swtch->frame[0].tick = 0;
					dest.swtch->frame[0].pic = target;
				}
			}

			skip = 0;
			goto continue_keyword;
		} else
		{
			if(!skip && kw[0] && strcmp(kw, "allowdecals"))
				I_Error("[ANIMDEFS] Invalid keyword '%s'!", kw);
			continue;
		}

		// get picture name
		kw = tp_get_keyword();
		if(!kw)
			goto error_end;

		target = get_pic(kw);
		if(target < 0)
		{
			// this does not exist
			skip = 1;
			continue;
		}
		name = kw;

		// animation
		dest.anim = anim_ptr;

		// expecting 'pic' or 'range'
		kw = tp_get_keyword();
		if(!kw)
			goto error_end;

		skip = 0;

		if(kw[0] == 'r')
		{
			// 'range'
			int32_t num;
			uint32_t tics;

			kw = tp_get_keyword();
			if(!kw)
				goto error_end;

			num = get_pic(kw);
			if(num <= target || num - target > 255)
				I_Error("[ANIMDEFS] invalid range in '%s'", name);

			num -= target;
			num++;

			// only 'tics' is supported; no 'rand'
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;
			if(kw[0] != 't')
				goto error_tics;

			kw = tp_get_keyword();
			if(!kw)
				goto error_end;
			if(doom_sscanf(kw, "%u", &tics) != 1 || !tics)
				goto error_numeric;

			// offset: header
			anim_ptr += sizeof(anim_header_t) + sizeof(animt_range_t);

			// generate frames
			if(animations)
			{
				uint16_t *pic;

				pic = anim_ptr;

				for(uint32_t i = 0; i < num; i++)
				{
					uint32_t tmp = target + i;
					if(get_pic == flat_num_check)
						*pic++ = flatlump[tmp];
					else
						*pic++ = tmp;
				}

				for(uint32_t i = 0; i < num - 1; i++)
				{
					uint32_t tmp = target + i;
					if(get_pic == flat_num_check)
						*pic++ = flatlump[tmp];
					else
						*pic++ = tmp;
				}

				// store animation info
				if(get_pic == flat_num_check)
					dest.anim->head.type = ANIM_TYPE_FLAT_RANGE;
				else
					dest.anim->head.type = ANIM_TYPE_TEXTURE_RANGE;
				dest.anim->head.target = target;
				dest.anim->head.count = num;
				dest.anim->range.tics = tics;
			}

			// offset: frames
			anim_ptr += (num * 2 - 1) * sizeof(uint16_t);

			continue;
		} else
		if(kw[0] == 'p')
		{
			// 'pic'
			uint32_t tick_total;
			uint32_t count;

			// reset info
			tick_total = 0;
			count = 0;

			// offset: header
			anim_ptr += sizeof(anim_header_t) + sizeof(animt_single_t);

			// parse all frames
			while(1)
			{
				int32_t num;
				animframe_t *frame = anim_ptr;

				// offset: frame
				anim_ptr += sizeof(animframe_t);

				// get picture name
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;
				num = get_pic(kw);
				if(num < 0)
					I_Error("[ANIMDEFS] frame '%s' not found for '%s'\n", kw, name);

				if(animations)
				{
					if(get_pic == flat_num_check)
						frame->pic = flatlump[num];
					else
						frame->pic = num;
					frame->tick = tick_total;
				}

				// only 'tics' is supported; no 'rand'
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;
				if(kw[0] != 't')
					goto error_tics;

				// get tick count
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;
				if(doom_sscanf(kw, "%u", &num) != 1 || num <= 0)
					goto error_numeric;

				// add this info
				count++;
				tick_total += num;

				// check next keyword
				kw = tp_get_keyword();
				if(!kw)
					break;

				if(kw[0] == 'a')
					// skip 'allowdecals'
					kw = tp_get_keyword();

				// check for next frame
				if(!kw || kw[0] != 'p')
					break;
			}

			// checks
			if(!tick_total || tick_total > 65535)
				goto error_tic_count;
			if(count <= 1 || count > 255)
				goto error_anim_count;

			// store animation info
			if(animations)
			{
				if(get_pic == flat_num_check)
					dest.anim->head.type = ANIM_TYPE_FLAT_SINGLE;
				else
					dest.anim->head.type = ANIM_TYPE_TEXTURE_SINGLE;
				dest.anim->head.target = target;
				dest.anim->head.count = count;
				dest.anim->single.tick_total = tick_total;
			}

			goto continue_keyword;
		}

		I_Error("[ANIMDEFS] Expected keyword 'pic' or 'range' found '%s' in '%s'!", kw, name);
	}

error_end:
	I_Error("[ANIMDEFS] Incomplete definition!");
error_tics:
	I_Error("[ANIMDEFS] Expected keyword 'tics' found '%s' in '%s'!", kw, name);
error_numeric:
	I_Error("[ANIMDEFS] Unable to parse number '%s' in '%s'!", kw, name);
error_tic_count:
	I_Error("[ANIMDEFS] bad tick count for '%s'!", name);
error_anim_count:
	I_Error("[ANIMDEFS] bad frame count for '%s'!", name);
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void animate_step()
{
	void *ptr = animations;

	// animations
	while(ptr)
	{
		animation_t *anim = ptr;

		switch(anim->head.type)
		{
			case ANIM_TYPE_FLAT_SINGLE:
			case ANIM_TYPE_TEXTURE_SINGLE:
			{
				uint32_t tick_offs;
				animframe_t *frame;

				// pointer offset
				ptr += sizeof(anim_header_t) + sizeof(animt_single_t) + anim->head.count * sizeof(animframe_t);

				// tick offset
				tick_offs = *leveltime % anim->single.tick_total;

				// find current frame
				for(uint32_t i = 0; i < anim->head.count; i++)
				{
					if(anim->single.frame[i].tick <= tick_offs)
						frame = anim->single.frame + i;
					else
						break;
				}

				if(anim->head.type == ANIM_TYPE_FLAT_SINGLE)
					(*flattranslation)[anim->head.target] = frame->pic;
				else
					(*texturetranslation)[anim->head.target] = frame->pic;
			}
			break;
			case ANIM_TYPE_FLAT_RANGE:
			case ANIM_TYPE_TEXTURE_RANGE:
			{
				uint32_t count = anim->head.count;
				uint32_t offset = *leveltime / anim->range.tics;

				// pointer offset
				ptr += sizeof(anim_header_t) + sizeof(animt_range_t) + (count * 2 - 1) * sizeof(uint16_t);

				for(uint32_t i = anim->head.target; i < anim->head.target + anim->head.count; i++)
				{
					uint32_t idx = (offset + i) % anim->head.count;

					if(anim->head.type == ANIM_TYPE_FLAT_RANGE)
						(*flattranslation)[i] = anim->range.pic[idx];
					else
						(*texturetranslation)[i] = anim->range.pic[idx];
				}
			}
			break;
			default:
				ptr = NULL;
			break;
		}
	}

	// buttons
	for(uint32_t i = 0; i < MAX_BUTTONS; i++)
	{
		uint32_t diff;
		switch_t *active = active_switch + i;

		if(!active->dest)
			continue;

		diff = *leveltime - active->base;

		if(active->delay && diff >= active->delay)
		{
			// restore this button
			uint16_t *dest = active->dest;
			void *ptr = switch_ptr;

			// cancel
			active->dest = NULL;

			// use reverse animation
			switch_line_texture(active->swtch->reverse, dest, 0, 0);

			return;
		}

		if(active->animate)
		{
			active->animate = switch_line_texture(active->swtch, active->dest, *leveltime - active->base, -1);
			if(!active->animate && !active->delay)
				active->dest = NULL;
		}
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
void clear_buttons()
{
	memset(active_switch, 0, sizeof(active_switch));
}

static __attribute((regparm(2),no_caller_saved_registers))
void do_line_switch(line_t *ln, uint32_t repeat)
{
	void *ptr = switch_ptr;
	side_t *side = *sides + ln->sidenum[0];

	if(!repeat)
		ln->special = 0;

	while(1)
	{
		aswitch_t *swtch = ptr;

		if(swtch->terminator)
			return;

		// offset
		ptr += sizeof(aswitch_t) + swtch->count * sizeof(animframe_t);

		// check all textures
		if(swtch->target == side->toptexture)
		{
			switch_line_texture(swtch, &side->toptexture, 0, repeat);
			break;
		} else
		if(swtch->target == side->midtexture)
		{
			switch_line_texture(swtch, &side->midtexture, 0, repeat);
			break;
		} else
		if(swtch->target == side->bottomtexture)
		{
			switch_line_texture(swtch, &side->bottomtexture, 0, repeat);
			break;
		}
	}
}

//
// callbacks

static void cb_animdefs(lumpinfo_t *li)
{
	tp_load_lump(li);
	parse_animdefs();
}

//
// API

void init_animations()
{
	// To avoid unnecessary memory fragmentation, this function does multiple passes.
	uint32_t size;

	doom_printf("[ACE] init animations\n");
	ldr_alloc_message = "Animation memory allocation failed!";

	//
	// PASS 1

	// count all animations (get buffer size)

	// internal
	tp_text_ptr = engine_animdefs;
	parse_animdefs();

	// external
	wad_handle_lump("ANIMDEFS", cb_animdefs);

	//
	// PASS 2

	// allocate memory (anim_size + terminator + switch_size + terminator)
	size = (uint32_t)anim_ptr;
	animations = ldr_malloc((uint32_t)anim_ptr + (uint32_t)switch_ptr + 2);
	anim_ptr = animations;
	switch_ptr = animations + size + 1;

	// fix internal animations
	for(uint32_t i = 0; i < sizeof(engine_animdefs)-1; i++)
	{
		if(!engine_animdefs[i])
			engine_animdefs[i] = ' ';
	}

	// generate animations

	// internal
	tp_text_ptr = engine_animdefs;
	parse_animdefs();

	// external
	wad_handle_lump("ANIMDEFS", cb_animdefs);

	// animation terminator
	*((uint8_t*)anim_ptr) = ANIM_TERMINATOR;

	// switch terminator
	*((uint8_t*)switch_ptr) = ANIM_TERMINATOR;

	// correct switch pointer
	switch_ptr = anim_ptr + 1;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// disable call to 'P_InitSwitchList' in 'P_Init'
	{0x0002E9A0, CODE_HOOK | HOOK_SET_NOPS, 5},
	// disable call to 'P_InitPicAnims' in 'P_Init'
	{0x0002E9A5, CODE_HOOK | HOOK_SET_NOPS, 5},
	// replace 'P_ChangeSwitchTexture'
	{0x00030310, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)do_line_switch},
	// replace animation loop in 'P_UpdateSpecials'
	{0x0002FC72, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)animate_step},
	{0x0002FC77, CODE_HOOK | HOOK_JMP_DOOM, 0x0002FCDB},
	// disable button loop in 'P_UpdateSpecials'
	{0x0002FD12, CODE_HOOK | HOOK_JMP_DOOM, 0x0002FE06},
	// replace button cleanup in 'P_SpawnSpecials'
	{0x000301C1, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)clear_buttons},
	{0x000301C6, CODE_HOOK | HOOK_JMP_DOOM, 0x000301E1},
};

