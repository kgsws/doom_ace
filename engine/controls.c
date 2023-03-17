// kgsws' ACE Engine
////
// All key and mouse controls.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "controls.h"

uint8_t key_jump = ' ';
uint8_t key_fire_alt;
uint8_t key_inv_use = '\r';
uint8_t key_inv_next = '.';
uint8_t key_inv_prev = ',';
uint8_t key_inv_quick;
uint8_t key_cheats = '`';

int32_t mouseb_fire_alt = 1;
int32_t mouseb_use = -1;
int32_t mouseb_inv_use = 2;

key_ctrl_t control_list[NUM_CONTROLS] =
{
	//
	[ctrl_key_up] = {0, "Move Forward", (uint8_t*)&key_up},
	[ctrl_key_down] = {0, "Move Backward", (uint8_t*)&key_down},
	[ctrl_key_strafeleft] = {0, "Strafe Left", (uint8_t*)&key_strafeleft},
	[ctrl_key_straferight] = {0, "Strafe Right", (uint8_t*)&key_straferight},
	[ctrl_key_left] = {0, "Turn Left", (uint8_t*)&key_left},
	[ctrl_key_right] = {0, "Turn Right", (uint8_t*)&key_right},
	[ctrl_key_jump] = {0, "Jump", (uint8_t*)&key_jump},
	//
	[ctrl_key_fire] = {1, "Attack", (uint8_t*)&key_fire},
	[ctrl_key_fire_alt] = {1, "Alt Attack", &key_fire_alt},
	[ctrl_key_use] = {1, "Use", (uint8_t*)&key_use},
	//
	[ctrl_key_inv_use] = {2, "Use", &key_inv_use},
	[ctrl_key_inv_next] = {2, "Next", &key_inv_next},
	[ctrl_key_inv_prev] = {2, "Previous", &key_inv_prev},
	[ctrl_key_inv_quick] = {2, "Quick Item", &key_inv_quick},
	//
	[ctrl_key_speed] = {3, "Speed", (uint8_t*)&key_speed},
	[ctrl_key_strafe] = {3, "Strafe", (uint8_t*)&key_strafe},
	[ctrl_key_cheats] = {3, "Cheats", &key_cheats},
};

const uint8_t *ctrl_group[NUM_CTRL_GROUPS] =
{
	"Movement",
	"Interaction",
	"Inventory",
	"Misc",
};

int32_t *const ctrl_mouse_ptr[NUM_MOUSE_CTRL] =
{
	&mouseb_fire,
	&mouseb_fire_alt,
	&mouseb_use,
	&mouseb_inv_use,
	&mouseb_strafe,
};

static const uint8_t *mouse_btn[NUM_MOUSE_BTNS+1] =
{
	"-",
	"L",
	"R",
	"M",
};

//
// API

void control_setup()
{
	// check mouse
	for(uint32_t i = 1; i < NUM_MOUSE_CTRL; i++)
	{
		if(*ctrl_mouse_ptr[i] < -1 || *ctrl_mouse_ptr[i] >= NUM_MOUSE_BTNS)
			*ctrl_mouse_ptr[i] = -1;
	}
	// check keys
	for(uint32_t i = 1; i < NUM_CONTROLS; i++)
	{
		if(!*control_list[i].ptr)
			*control_list[i].ptr = 1;
	}
}

void control_clear_key(uint8_t id)
{
	for(uint32_t i = 0; i < NUM_CONTROLS; i++)
	{
		key_ctrl_t *ctrl = control_list + i;
		if(*ctrl->ptr == id)
			*ctrl->ptr = 1;
	}
}

uint8_t *control_key_name(uint8_t id)
{
	static uint8_t name_tmp[8];

	switch(id)
	{
		case 0:
			return "INVALID";
		case 1:
			return "---";
		case '\t':
			return "[TAB]";
		case '\r':
			return "[ENTER]";
		case 27:
			return "[ESC]";
		case 32:
			return "[SPACE]";
		case 96:
			return "[GRAVE]";
		case 127:
			return "[BACKSPACE]";
		case 172:
			return "[LEFT]";
		case 173:
			return "[UP]";
		case 174:
			return "[RIGHT]";
		case 175:
			return "[DOWN]";
		case 157:
			return "[CTRL]";
		case 182:
			return "[SHIFT]";
		case 184:
			return "[ALT]";
		case 199:
			return "[HOME]";
		case 201:
			return "[PGUP]";
		case 207:
			return "[END]";
		case 209:
			return "[PGDN]";
		case 210:
			return "[INS]";
		case 211:
			return "[DEL]";
	}

	if(id >= 'a' && id <= 'z')
		id &= ~0x20;

	if(id > ' ' && id < 0x7F)
	{
		*((uint32_t*)name_tmp) = 0x005D005B | ((uint32_t)id << 8);
		return name_tmp;
	}

	doom_sprintf(name_tmp, "*%u", id);
	return name_tmp;
}

uint8_t *control_btn_name(uint8_t id)
{
	return (uint8_t*)mouse_btn[*ctrl_mouse_ptr[id] + 1];
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// original keys
	{0x0003BA1B, CODE_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_cheats].ptr},
};

