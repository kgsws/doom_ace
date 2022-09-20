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
uint8_t key_inv_next = '.';
uint8_t key_inv_prev = ',';
uint8_t key_cheats = '`';

int32_t mouseb_fire_alt = 1;
int32_t mouseb_use = -1;
int32_t mouseb_inv_use = 2;

key_ctrl_t control_list[NUM_CONTROLS] =
{
	//
	[ctrl_key_up] = {0, "Move Forward"},
	[ctrl_key_down] = {0, "Move Backward"},
	[ctrl_key_strafeleft] = {0, "Strafe Left"},
	[ctrl_key_straferight] = {0, "Strafe Right"},
	[ctrl_key_left] = {0, "Turn Left"},
	[ctrl_key_right] = {0, "Turn Right"},
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

static int32_t *const mouse_ptr[NUM_MOUSE_CTRL] =
{
	NULL, // mouseb_fire
	&mouseb_fire_alt,
	&mouseb_use,
	&mouseb_inv_use,
	NULL, // mouseb_strafe
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
		if(*mouse_ptr[i] < -1 || *mouse_ptr[i] >= NUM_MOUSE_BTNS)
			*mouse_ptr[i] = -1;
	}
}

void control_clear_key(uint8_t id)
{
	for(uint32_t i = 0; i < NUM_CONTROLS; i++)
	{
		key_ctrl_t *ctrl = control_list + i;
		if(*ctrl->ptr == id)
			*ctrl->ptr = 0;
	}
}

uint8_t *control_key_name(uint8_t id)
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
	}

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
	return (uint8_t*)mouse_btn[*mouse_ptr[id] + 1];
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x0002A880, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gamekeydown},
	{0x0002AC84, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mousebuttons},
	// original keys
	{0x0002B344, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_up].ptr},
	{0x0002B35C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_down].ptr},
	{0x0002B360, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_strafeleft].ptr},
	{0x0002B390, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_straferight].ptr},
	{0x0002B364, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_left].ptr},
	{0x0002B394, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_right].ptr},
	{0x0002B384, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_fire].ptr},
	{0x0002B354, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_use].ptr},
	{0x0002B34C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_speed].ptr},
	{0x0002B370, DATA_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_strafe].ptr},
	{0x0003BA1B, CODE_HOOK | HOOK_IMPORT, (uint32_t)&control_list[ctrl_key_cheats].ptr},
	// original mouse buttons
	{0x0002B380, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mouse_ptr[0]},
	{0x0002B36C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mouse_ptr[4]},
};

