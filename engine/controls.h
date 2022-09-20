// kgsws' ACE Engine
////

#define NUM_MOUSE_BTNS	3
#define NUM_MOUSE_CTRL	5

enum
{
	cgrp_movement,
	cgrp_interaction,
	cgrp_inventory,
	cgrp_misc,
	//
	NUM_CTRL_GROUPS
};

enum
{
	ctrl_key_up,
	ctrl_key_down,
	ctrl_key_strafeleft,
	ctrl_key_straferight,
	ctrl_key_left,
	ctrl_key_right,
	//
	ctrl_key_fire,
	ctrl_key_fire_alt,
	ctrl_key_use,
	//
	ctrl_key_inv_use,
	ctrl_key_inv_next,
	ctrl_key_inv_prev,
	//
	ctrl_key_speed,
	ctrl_key_strafe,
	ctrl_key_cheats,
	//
	NUM_CONTROLS
};

typedef struct
{
	uint32_t group;
	const uint8_t *name;
	uint8_t *ptr;
} key_ctrl_t;

//

extern key_ctrl_t control_list[NUM_CONTROLS];
extern const uint8_t *ctrl_group[NUM_CTRL_GROUPS];

extern uint8_t key_fire_alt;
extern uint8_t key_inv_use;
extern uint8_t key_inv_next;
extern uint8_t key_inv_prev;
extern uint8_t key_cheats;

extern uint8_t mouse_button[3];

extern int32_t mouseb_fire_alt;
extern int32_t mouseb_use;
extern int32_t mouseb_inv_use;

extern uint32_t *gamekeydown;
extern uint32_t *mousebuttons;

//

void control_setup();
void control_clear_key(uint8_t id);
uint8_t *control_key_name(uint8_t id);
uint8_t *control_btn_name(uint8_t id);

