// kgsws' ACE Engine
////
// Subset of ANIMDEFS support.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "animate.h"
#include "ldr_texture.h"
#include "ldr_flat.h"

//
static uint8_t *text_ptr;
static void *anim_ptr;
static void *animations;

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
"texture	SLADRIP1	r SLADRIP4 t 8\n"
"texture	WFALL1	r WFALL4 t 8\n"
;

//
// parser

static uint8_t *get_keyword()
{
	uint8_t *ret;

	// end check
	if(!*text_ptr)
		return NULL;

	// skip whitespaces and newlines
	while(*text_ptr == ' ' || *text_ptr == '\t' || *text_ptr == '\r' || *text_ptr == '\n')
		text_ptr++;

	// this is the keyword
	ret = text_ptr;

	// skip the text
	while(*text_ptr && *text_ptr != ' ' && *text_ptr != '\t' && *text_ptr != '\r' && *text_ptr != '\n')
		text_ptr++;

	// terminate string
	if(*text_ptr)
		*text_ptr++ = 0;

	// return the keyword
	return ret;
}

static void parse_animdefs(uint8_t *text)
{
	uint8_t *kw, *name;
	animation_t *anim;
	int32_t target;
	uint32_t skip = 0;
	int32_t (*get_pic)(uint8_t*) __attribute((regparm(2),no_caller_saved_registers));

	text_ptr = text;
	while(1)
	{
		kw = get_keyword();
continue_keyword:
		if(!kw)
			break;

		if(!strcmp(kw, "flat"))
			get_pic = flat_num_check;
		else
		if(!strcmp(kw, "texture"))
			get_pic = texture_num_check;
		else
		{
			if(!skip && strcmp(kw, "allowdecals"))
				I_Error("ANIMDEFS: Invalid keyword '%s'!", kw);
			continue;
		}

		// get picture name
		kw = get_keyword();
		if(!kw)
			goto error_end;

		target = get_pic(kw);
		if(target < 0)
		{
			// this flat does not exist
			skip = 1;
			continue;
		}
		name = kw;

		// animation
		anim = anim_ptr;

		// expecting 'pic' or 'range'
		kw = get_keyword();
		if(!kw)
			goto error_end;

		if(kw[0] == 'r')
		{
			// 'range'
			int32_t num;
			uint32_t tics;

			kw = get_keyword();
			if(!kw)
				goto error_end;

			num = get_pic(kw);
			if(num <= target || num - target > 255)
				I_Error("ANIMDEFS: invalid range in '%s'", name);

			num -= target;
			num++;

			// only 'tics' is supported; no 'rand'
			kw = get_keyword();
			if(!kw)
				goto error_end;
			if(kw[0] != 't')
				goto error_tics;

			kw = get_keyword();
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
					anim->head.type = ANIM_TYPE_FLAT_RANGE;
				else
					anim->head.type = ANIM_TYPE_TEXTURE_RANGE;
				anim->head.target = target;
				anim->head.count = num;
				anim->range.tics = tics;
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
				animframe_t *frame;

				frame = anim_ptr;

				// offset: frame
				anim_ptr += sizeof(animframe_t);

				// get picture name
				kw = get_keyword();
				if(!kw)
					goto error_end;

				num = get_pic(kw);
				if(num < 0)
					I_Error("ANIMDEFS: frame '%s' not found for '%s'\n", kw, name);

				if(animations)
				{
					if(get_pic == flat_num_check)
						frame->pic = flatlump[num];
					else
						frame->pic = num;
					frame->tick = tick_total;
				}

				// only 'tics' is supported; no 'rand'
				kw = get_keyword();
				if(!kw)
					goto error_end;
				if(kw[0] != 't')
					goto error_tics;

				// get tick count
				kw = get_keyword();
				if(!kw)
					goto error_end;
				if(doom_sscanf(kw, "%u", &num) != 1 || num <= 0)
					goto error_numeric;

				// add this info
				count++;
				tick_total += num;

				// check next keyword
				kw = get_keyword();
				if(!kw)
					break;

				if(kw[0] == 'a')
					// skip 'allowdecals'
					kw = get_keyword();

				if(!kw || kw[0] != 'p')
					break;
			}

			// checks
			if(!tick_total || tick_total > 65535)
				I_Error("ANIMDEFS: bad tick count for '%s'!", name);
			if(count <= 1 || count > 255)
				I_Error("ANIMDEFS: bad frame count for '%s'!", name);

			// store animation info
			if(animations)
			{
				if(get_pic == flat_num_check)
					anim->head.type = ANIM_TYPE_FLAT_SINGLE;
				else
					anim->head.type = ANIM_TYPE_TEXTURE_SINGLE;
				anim->head.target = target;
				anim->head.count = count;
				anim->single.tick_total = tick_total;
			}

			goto continue_keyword;
		}

		I_Error("ANIMDEFS: Expected keyword 'pic' or 'range' found '%s' in '%s'!", kw, name);
	}

	return;
error_end:
	I_Error("ANIMDEFS: Incomplete definition!");
error_tics:
	I_Error("ANIMDEFS: Expected keyword 'tics' found '%s' in '%s'!", kw, name);
error_numeric:
	I_Error("ANIMDEFS: Unable to parse number '%s' in '%s'!", kw, name);
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void animate_step()
{
	void *ptr = animations;

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
}

//
// callbacks

static void cb_animdefs(lumpinfo_t *li)
{
	void *data;

	data = wad_cache_lump(li - *lumpinfo, NULL);
	parse_animdefs(data);

	doom_free(data);
}

//
// API

void init_animations()
{
	// To avoid unnecessary memory fragmentation this function does multiple passes.
	doom_printf("[ACE] init animations\n");
	ldr_alloc_message = "Animation memory allocation failed!";

	//
	// PASS 1

	// count all animations (get buffer size)
	parse_animdefs(engine_animdefs); // internal
	wad_handle_lump("ANIMDEFS", cb_animdefs);

	//
	// PASS 2

	// allocate memory (size + terminator)
	animations = ldr_malloc((uint32_t)anim_ptr + sizeof(uint8_t));
	anim_ptr = animations;

	// fix internal animations
	for(uint32_t i = 0; i < sizeof(engine_animdefs)-1; i++)
	{
		if(!engine_animdefs[i])
			engine_animdefs[i] = ' ';
	}

	// generate animations
	parse_animdefs(engine_animdefs); // internal
	wad_handle_lump("ANIMDEFS", cb_animdefs);

	// terminator
	*((uint8_t*)anim_ptr) = ANIM_TERMINATOR;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// disable call to 'P_InitSwitchList' in 'P_Init'
//	{0x0002E9A5, CODE_HOOK | HOOK_SET_NOPS, 5},
	// disable call to 'P_InitPicAnims' in 'P_Init'
	{0x0002E9A5, CODE_HOOK | HOOK_SET_NOPS, 5},
	// replace animation loop in 'P_UpdateSpecials'
	{0x0002FC72, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)animate_step},
	{0x0002FC77, CODE_HOOK | HOOK_JMP_DOOM, 0x0002FCDB},
};

