// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "dehacked.h"

#define POINTER_COUNT	448

typedef struct
{
	const uint8_t *name;
	uint32_t clen;
	void (*parse)(uint8_t*);
} deh_section_t;

typedef struct
{
	const uint8_t *name;
	uint32_t offset;
	void (*handler)(uint8_t*,void*);
} deh_value_t;

//
static uint8_t *deh_ptr;
static uint8_t *deh_end;

static uint16_t *ptr2state;
static void **ptrtable;

// section info
static void parser_thing(uint8_t*);
static void parser_frame(uint8_t*);
static void parser_weapon(uint8_t*);
static void parser_ammo(uint8_t*);
static void parser_misc(uint8_t*);
static void parser_pointer(uint8_t*);
static void parser_text(uint8_t*);
static void parser_dummy(uint8_t*);
static const deh_section_t deh_section[] =
{
	{"Thing ", 6, parser_thing},
	{"Frame ", 6, parser_frame},
	{"Weapon ", 7, parser_weapon},
	{"Ammo ", 5, parser_ammo},
	{"Misc ", 5, parser_misc},
	{"Pointer ", 8, parser_pointer},
	{"Text ", 5, parser_text},
	// terminator
	{NULL, 0, parser_dummy}
};

// section handlers
static void handle_u32(uint8_t*,void*);
static void handle_u8(uint8_t*,void*);
static void handle_give_limit(uint8_t*,void*);
static void handle_bfg(uint8_t*,void*);
static void handle_species(uint8_t*,void*);
static void handle_infight(uint8_t*,void*);

// section 'thing' values
static const deh_value_t deh_value_thing[] =
{
	{"ID #", offsetof(mobjinfo_t, doomednum), handle_u32},
	{"Initial frame", offsetof(mobjinfo_t, spawnstate), handle_u32},
	{"Hit points", offsetof(mobjinfo_t, spawnhealth), handle_u32},
	{"First moving frame", offsetof(mobjinfo_t, seestate), handle_u32},
	{"Alert sound", offsetof(mobjinfo_t, seesound), handle_u32},
	{"Reaction time", offsetof(mobjinfo_t, reactiontime), handle_u32},
	{"Attack sound", offsetof(mobjinfo_t, attacksound), handle_u32},
	{"Injury frame", offsetof(mobjinfo_t, painstate), handle_u32},
	{"Pain chance", offsetof(mobjinfo_t, painchance), handle_u32},
	{"Pain sound", offsetof(mobjinfo_t, painsound), handle_u32},
	{"Close attack frame", offsetof(mobjinfo_t, meleestate), handle_u32},
	{"Far attack frame", offsetof(mobjinfo_t, missilestate), handle_u32},
	{"Death frame", offsetof(mobjinfo_t, deathstate), handle_u32},
	{"Exploding frame", offsetof(mobjinfo_t, xdeathstate), handle_u32},
	{"Death sound", offsetof(mobjinfo_t, deathsound), handle_u32},
	{"Speed", offsetof(mobjinfo_t, speed), handle_u32},
	{"Width", offsetof(mobjinfo_t, radius), handle_u32},
	{"Height", offsetof(mobjinfo_t, height), handle_u32},
	{"Mass", offsetof(mobjinfo_t, mass), handle_u32},
	{"Missile damage", offsetof(mobjinfo_t, damage), handle_u32},
	{"Action sound", offsetof(mobjinfo_t, activesound), handle_u32},
	{"Bits", offsetof(mobjinfo_t, flags), handle_u32},
	{"Respawn frame", offsetof(mobjinfo_t, raisestate), handle_u32},
	// terminator
	{NULL}
};

// section 'frame' values
static const deh_value_t deh_value_frame[] =
{
	{"Sprite number", offsetof(state_t, sprite), handle_u32},
	{"Sprite subnumber", offsetof(state_t, frame), handle_u32},
	{"Duration", offsetof(state_t, tics), handle_u32},
//	{"Codep frame", offsetof(state_t, action), handle_u32},
	{"Next frame", offsetof(state_t, nextstate), handle_u32},
	{"Unknown 1", offsetof(state_t, misc1), handle_u32},
	{"Unknown 2", offsetof(state_t, misc2), handle_u32},
	// terminator
	{NULL}
};

// section 'weapon' values
static const deh_value_t deh_value_weapon[] =
{
	{"Ammo type", offsetof(weaponinfo_t, ammo), handle_u32},
	{"Deselect frame", offsetof(weaponinfo_t, upstate), handle_u32},
	{"Select frame", offsetof(weaponinfo_t, downstate), handle_u32},
	{"Bobbing frame", offsetof(weaponinfo_t, readystate), handle_u32},
	{"Shooting frame", offsetof(weaponinfo_t, atkstate), handle_u32},
	{"Firing frame", offsetof(weaponinfo_t, flashstate), handle_u32},
	// terminator
	{NULL}
};

// section 'ammo' values
static const deh_value_t deh_value_ammo[] =
{
	{"Max ammo", 0, handle_u32},
	{"Per ammo", 0x00012D80 - 0x00012D70, handle_u32},
	// terminator
	{NULL}
};

// section 'misc' values
static const deh_value_t deh_value_misc[] =
{
	{"Initial Health", 0x000209A4, handle_u32},
	{"Initial Bullets", 0x000209C0, handle_u32},
	{"Max Health", 0x00029BA6, handle_give_limit},
	{"Max Armor", 0x00029BD3, handle_give_limit},
//	{"Green Armor Class", },
//	{"Blue Armor Class", },
	{"Max Soulsphere", 0x00029C06, handle_give_limit},
	{"Soulsphere Health", 0x00029C01, handle_u8},
	{"Megasphere Health", 0x00029C3F, handle_u32},
//	{"God Mode Health", },
//	{"IDFA Armor", },
//	{"IDFA Armor Class", },
//	{"IDKFA Armor", },
//	{"IDKFA Armor Class", },
	{"BFG Cells/Shot", 0, handle_bfg},
	{"Monsters Infight", 0, handle_species},
	{"Monsters Ignore Each Other", 0, handle_infight},
	// terminator
	{NULL}
};

//
// functions

static void *get_line()
{
	uint8_t *ret;

	if(deh_ptr >= deh_end)
		return NULL;

	ret = deh_ptr;

	while(deh_ptr < deh_end)
	{
		if(*deh_ptr == '\r')
		{
			*deh_ptr = 0;
			deh_ptr++;
			if(deh_ptr < deh_end && *deh_ptr == '\n')
				deh_ptr++;
			break;
		}

		if(*deh_ptr == '\n')
		{
			*deh_ptr = 0;
			deh_ptr++;
			break;
		}

		deh_ptr++;
	}

	return ret;
}

static void *get_text(uint32_t length)
{
	uint8_t *ret = deh_ptr;
	uint8_t *dst = deh_ptr;

	while(deh_ptr < deh_end)
	{
		if(*deh_ptr == '\r')
		{
			// parse CR/LF as single byte
			if(deh_ptr + 1 < deh_end && deh_ptr[1] == '\n')
				deh_ptr++;
			else
				*deh_ptr = '\n';
		}

		*dst++ = *deh_ptr;
		deh_ptr++;

		length--;
		if(!length)
			break;
	}

	if(length)
		return NULL;

	return ret;
}

//
// value handlers

static void handle_u32(uint8_t *text, void *target)
{
	uint32_t tmp;

	if(doom_sscanf(text, "%i", &tmp) != 1) // allow DEC HEX and OCT
		return;

	*((uint32_t*)target) = tmp;
}

static void handle_u8(uint8_t *text, void *target)
{
	uint32_t tmp;

	if(doom_sscanf(text, "%i", &tmp) != 1) // allow DEC HEX and OCT
		return;

	*((uint8_t*)target) = tmp;
}
#if 0
static void handle_max_health(uint8_t *text, void *target)
{
	// this modifies multiple locations
	// only 8bit (255 max) is allowed
	uint32_t tmp;

	if(doom_sscanf(text, "%i", &tmp) != 1) // allow DEC HEX and OCT
		return;

	// patch 'P_GiveBody'
	*((uint8_t*)(doom_code_segment + 0x000298F7)) = tmp; // cmp $0x64,%ebx
	*((uint8_t*)(doom_code_segment + 0x00029907)) = tmp; // cmp $0x64,%ecx
	*((uint8_t*)(doom_code_segment + 0x0002990D)) = tmp; // movl $0x64,0x20(%eax)
}
#endif
static void handle_give_limit(uint8_t *text, void *target)
{
	// health or armor give limit
	// this modifies multiple locations
	uint32_t tmp;

	if(doom_sscanf(text, "%i", &tmp) != 1) // allow DEC HEX and OCT
		return;

	*((uint32_t*)target) = tmp;
	*((uint32_t*)(target + 9)) = tmp;
}

static void handle_bfg(uint8_t *text, void *target)
{
	uint32_t tmp;

	if(doom_sscanf(text, "%i", &tmp) != 1) // allow DEC HEX and OCT
		return;

	*((uint32_t*)(doom_code_segment + 0x0002D484)) = tmp;
	*((uint8_t*)(doom_code_segment + 0x0002D74D)) = tmp;
}

static void handle_species(uint8_t *text, void *target)
{
	uint32_t tmp;

	if(doom_sscanf(text, "%i", &tmp) != 1) // allow DEC HEX and OCT
		return;

	if(tmp == 202)
		tmp = 0x74;
	else
	if(tmp == 221)
		tmp = 0xEB;
	else
		return;

	*((uint8_t*)(doom_code_segment + 0x0002AFC9)) = tmp;
}

static void handle_infight(uint8_t *text, void *target)
{
	uint32_t tmp;

	if(doom_sscanf(text, "%i", &tmp) != 1) // allow DEC HEX and OCT
		return;

	if(tmp)
		tmp = 0x7500;
	else
		tmp = 0x7403;

	*((uint16_t*)(doom_code_segment + 0x0002A705)) = tmp;
}

//
// common parser

static void common_parser(void *base_ptr, const deh_value_t *valtab)
{
	while(1)
	{
		uint8_t *line, *ptr, *vtxt;
		const deh_value_t *info;

		line = get_line();
		if(!line)
			// end of file
			break;

		if(!line[0])
			// end of section
			break;

		if(!base_ptr)
			// ignore
			continue;

		if(line[0] == '#')
			// comment
			continue;

		// find assignment
		vtxt = line;
		while(*vtxt)
		{
			if(*vtxt == '=')
				break;
			vtxt++;
		}

		// remove any spaces before
		ptr = vtxt - 1;
		while(ptr >= line)
		{
			if(*ptr == ' ')
				*ptr = 0;
			else
				break;
			ptr--;
		}

		// skip any spaces after
		do
		{
			vtxt++;
		} while(*vtxt == ' ');

		// find this value
		info = valtab;
		while(info->name)
		{
			if(!strcmp(info->name, line))
			{
				// found
				info->handler(vtxt, base_ptr + info->offset);
				break;
			}
			info++;
		}
	}
}

//
// section parsers

static void parser_thing(uint8_t *line)
{
	uint32_t idx;
	void *base_ptr;

	if(doom_sscanf(line, "%u", &idx) == 1 && idx > 0 && idx < NUMMOBJTYPES)
		base_ptr = mobjinfo + idx - 1;
	else
		base_ptr = NULL;

	common_parser(base_ptr, deh_value_thing);
}

static void parser_frame(uint8_t *line)
{
	uint32_t idx;
	void *base_ptr;

	if(doom_sscanf(line, "%u", &idx) == 1 && idx < NUMSTATES)
		base_ptr = states + idx;
	else
		base_ptr = NULL;

	common_parser(base_ptr, deh_value_frame);
}

static void parser_weapon(uint8_t *line)
{
	uint32_t idx;
	void *base_ptr;

	if(doom_sscanf(line, "%u", &idx) == 1 && idx < NUMWEAPONS)
		base_ptr = weaponinfo + idx;
	else
		base_ptr = NULL;

	common_parser(base_ptr, deh_value_weapon);
}

static void parser_ammo(uint8_t *line)
{
	uint32_t idx;
	void *base_ptr;

	if(doom_sscanf(line, "%u", &idx) == 1 && idx < NUMAMMO)
		base_ptr = (void*)doom_data_segment + 0x00012D70 + idx * sizeof(uint32_t);
	else
		base_ptr = NULL;

	common_parser(base_ptr, deh_value_ammo);
}

static void parser_misc(uint8_t *line)
{
	common_parser((void*)doom_code_segment, deh_value_misc);
}

static void parser_pointer(uint8_t *line)
{
	uint32_t idx;
	int32_t ptr_state;

	if(doom_sscanf(line, "%u", &idx) == 1 && idx < POINTER_COUNT)
		ptr_state = ptr2state[idx];
	else
		ptr_state = -1;

	while(1)
	{
		uint8_t *line, *ptr, *vtxt;
		uint32_t src_state;

		line = get_line();
		if(!line)
			// end of file
			break;

		if(!line[0])
			// end of section
			break;

		if(ptr_state < 0)
			// ignore
			continue;

		if(line[0] == '#')
			// comment
			continue;

		// find assignment
		vtxt = line;
		while(*vtxt)
		{
			if(*vtxt == '=')
				break;
			vtxt++;
		}

		// remove any spaces before
		ptr = vtxt - 1;
		while(ptr >= line)
		{
			if(*ptr == ' ')
				*ptr = 0;
			else
				break;
			ptr--;
		}

		// skip any spaces after
		do
		{
			vtxt++;
		} while(*vtxt == ' ');

		// check
		if(strcmp(line, "Codep Frame"))
			// not interested
			continue;

		// parse value
		if(doom_sscanf(vtxt, "%i", &src_state) == 1 && src_state < NUMSTATES)
			states[ptr_state].action = ptrtable[src_state];
	}
}

static void parser_text(uint8_t *line)
{
	// Parsing of text is quite different.
	// Any text can span multiple lines and lenghts are defined in section header.
	uint32_t olen, nlen, tmp;
	uint8_t *dst, *end, *otxt, *ntxt;

	if(doom_sscanf(line, "%u %u", &olen, &nlen) != 2)
		// OK, this is a fail
		return;

	// process original text
	otxt = get_text(olen);
	if(!otxt)
		return;

	// process new text
	ntxt = get_text(nlen);
	if(!ntxt)
		return;
	ntxt[nlen] = 0;
	nlen++;

	// find original text in the memory
	dst = (uint8_t*)doom_data_segment + 0x1F904;
	end = dst + 0x5A94;
	while(dst + olen < end)
	{
		if(!dst[olen] && !strncmp(otxt, dst, olen))
		{
			// found one, replace it
			memcpy(dst, ntxt, nlen);

			// next
			dst += olen;
			continue;
		}
		dst++;
	}
}

static void parser_dummy(uint8_t *line)
{
	common_parser(NULL, NULL);
}

//
// main parser

static void dehacked_parse(uint8_t *text, uint32_t size)
{
	const deh_section_t *section;

	deh_ptr = text;
	deh_end = text + size;
	*deh_end = 0;

	while(1)
	{
		uint8_t *line;

		line = get_line();
		if(!line)
			break;

		if(!line[0])
			// empty line
			continue;

		if(line[0] == '#')
			// comment
			continue;

		// find a section
		section = deh_section;
		while(section->name)
		{
			if(!strncmp(line, section->name, section->clen))
				break;
			section++;
		}

		// parse this section
		section->parse(line + section->clen);
	}
}

//
// API

void deh_init()
{
	int32_t lump;
	uint32_t size;
	void *data;
	void *syst;

	lump = wad_check_lump("DEHACKED");
	if(lump < 0)
		return;

	syst = doom_malloc(POINTER_COUNT * sizeof(uint16_t) + NUMSTATES * sizeof(void*));
	if(!syst)
		return;

	ptr2state = syst;
	ptrtable = syst + POINTER_COUNT * sizeof(uint16_t);

	size = 0;
	for(uint32_t i = 0; i < NUMSTATES; i++)
	{
		state_t *state = states + i;

		ptrtable[i] = state->action;

		if(state->action && size < POINTER_COUNT)
			ptr2state[size++] = i;
	}

	data = wad_cache_lump(lump, &size);
	doom_printf("[ACE] parse DEHACKED %u bytes\n", size);
	dehacked_parse(data, size);

	doom_free(data);
	doom_free(syst);
}

