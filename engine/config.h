// kgsws' ACE Engine
////

typedef struct
{
	uint8_t enable_decorate;
	uint8_t enable_dehacked;
} mod_config_t;

//

extern mod_config_t mod_config;

//

void init_config();

