// kgsws' ACE Engine
////
// All key and mouse controls.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "controls.h"

// reuse relocation space
extern uint8_t _reloc_start[];
#define name_tmp	_reloc_start

uint32_t *gamekeydown;
uint32_t *mousebuttons;

uint8_t key_fire_alt;
uint8_t key_inv_use = '\r';
uint8_t key_inv_next = ',';
uint8_t key_inv_prev = '.';
uint8_t key_cheats = '`';

control_t control_list[NUM_CONTROLS] =
{
	//
	[ctrl_key_up] = {0, "Move Forward"},
	[ctrl_key_down] = {0, "Move Backward"},
	[ctrl_key_strafeleft] = {0, "Strafe Left"},
	[ctrl_key_straferight] = {0, "Strafe Right"},
	[ctrl_key_right] = {0, "Turn Left"},
	[ctrl_key_left] = {0, "Turn Right"},
	//
	[ctrl_key_fire] = {1, "Attack"},
	[ctrl_key_fire_alt] = {1, "Alt Attack", &key_fire_alt},
	[ctrl_key_use] = {1, "Use"},
	//
	[ctrl_key_inv_use] = {2, "Use", &key_inv_use},
	[ctrl_key_inv_next] = {2, "Next", &key_inv_next},
	[ctrl_key_inv_prev] = {2, "Previous", &key_inv_prev},
	//
	[ctrl_key_speed] = {3, "Speed"},
	[ctrl_key_strafe] = {3, "Strafe"},
	[ctrl_key_cheats] = {3, "Cheats", &key_cheats},
};

const uint8_t *ctrl_group[NUM_CTRL_GROUPS] =
{
	"Movement",
	"Interaction",
	"Inventory",
	"Misc",
};

//
// API

void control_clear_key(uint8_t id)
{
	for(uint32_t i = 0; i < NUM_CONTROLS; i++)
	{
		control_t *ctrl = control_list + i;
		if(*ctrl->ptr == id)
			*ctrl->ptr = 0;
	}
}

uint8_t *control_id_name(uint8_t id)
{
	switch(id)
	{
		case 0:
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
			return "[RIGHT]";
		case 173:
			return "[UP]";
		case 174:
			return "[LEFT]";
		case 175:
			return "[DOWN]";
		case 157:
			return "[CTRL]";
		case 182:
			return "[SHIFT]";
		case 184:
			return "[ALT]";
	}

	if(id > ' ' && id < 0x7F)
	{
		name_tmp[0] = '[';
		name_tmp[1] = id;
		name_tmp[2] = ']';
		name_tmp[3] = 0;
		return name_tmp;
	}

	doom_sprintf(name_tmp, "*%u", id);
	return name_tmp;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x0002A880, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gamekeydown},
	{0x0002AC84, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mousebuttons},
	// original inputs
	{0x0002B344, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_up].ptr},
	{0x0002B35C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_down].ptr},
	{0x0002B360, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_strafeleft].ptr},
	{0x0002B390, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_straferight].ptr},
	{0x0002B394, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_right].ptr},
	{0x0002B364, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_left].ptr},
	{0x0002B384, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_fire].ptr},
	{0x0002B354, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_use].ptr},
	{0x0002B370, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_strafe].ptr},
	{0x0002B34C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_speed].ptr},
	{0x0003BA1B, CODE_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_cheats].ptr},
};

