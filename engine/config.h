// kgsws' ACE Engine
////

typedef struct
{
	uint8_t enable_decorate;
	uint8_t enable_dehacked;
} mod_config_t;

typedef struct
{
	// player
	uint8_t auto_switch;
	uint8_t auto_aim;
	uint8_t mouse_look;
	uint8_t center_weapon;
	// display
	uint8_t crosshair_type;
	uint8_t crosshair_red;
	uint8_t crosshair_green;
	uint8_t crosshair_blue;
} extra_config_t;

//

extern extra_config_t extra_config;
extern mod_config_t mod_config;

//

void init_config();

