// kgsws' ACE Engine
////
// This handles both game and mod configuration.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "player.h"
#include "textpars.h"
#include "controls.h"
#include "config.h"

#define ACE_CONFIG_FILE	"ace.cfg"

enum
{
	TYPE_U8,
	TYPE_S32,
};

typedef struct
{
	const uint8_t *name;
	union
	{
		void *ptr;
		uint8_t *u8;
		int32_t *s32;
	};
	uint8_t type;
	uint8_t relocate;
} config_entry_t;

//

extra_config_t extra_config =
{
	.auto_switch = 0,
	.auto_aim = 0,
	.mouse_look = 1,
	.center_weapon = 1,
	.crosshair_type = 1,
	.crosshair_red = 255,
	.crosshair_green = 255,
	.crosshair_blue = 0,
};

mod_config_t mod_config =
{
	.enable_decorate = 1,
	.enable_dehacked = 1,
};

//

static config_entry_t config_game[] =
{
	// keys
	{"input.key.move.forward", (void*)0x0002B344, TYPE_U8, 1},
	{"input.key.move.backward", (void*)0x0002B35C, TYPE_U8, 1},
	{"input.key.strafe.left", (void*)0x0002B360, TYPE_U8, 1},
	{"input.key.strafe.right", (void*)0x0002B390, TYPE_U8, 1},
	{"input.key.turn.left", (void*)0x0002B364, TYPE_U8, 1},
	{"input.key.turn.right", (void*)0x0002B394, TYPE_U8, 1},
	{"input.key.atk.pri", (void*)0x0002B384, TYPE_U8, 1},
	{"input.key.atk.sec", &key_fire_alt, TYPE_U8},
	{"input.key.use", (void*)0x0002B354, TYPE_U8, 1},
	{"input.key.inv.use", &key_inv_use, TYPE_U8},
	{"input.key.inv.next", &key_inv_next, TYPE_U8},
	{"input.key.inv.previous", &key_inv_prev, TYPE_U8},
	{"input.key.speed", (void*)0x0002B34C, TYPE_U8, 1},
	{"input.key.strafe", (void*)0x0002B370, TYPE_U8, 1},
	{"input.key.cheats", (void*)0x0003BA1B, TYPE_U8, 2},
	// mouse
	{"input.mouse.enable", (void*)0x0002B6E8, TYPE_S32, 1},
	{"input.mouse.sensitivity", (void*)0x0002B6C8, TYPE_S32, 1},
	{"input.mouse.atk.pri", (void*)0x0002B380, TYPE_S32, 1},
	{"input.mouse.atk.sec", &mouseb_fire_alt, TYPE_S32},
	{"input.mouse.use", &mouseb_use, TYPE_S32},
	{"input.mouse.inv.use", &mouseb_inv_use, TYPE_S32},
	{"input.mouse.strafe", (void*)0x0002B36C, TYPE_S32, 1},
	// joystick
	{"input.joy.enable", (void*)0x0002B6EC, TYPE_S32, 1},
	{"input.joy.fire", (void*)0x0002B37C, TYPE_S32, 1},
	{"input.joy.strafe", (void*)0x0002B368, TYPE_S32, 1},
	{"input.joy.use", (void*)0x0002B350, TYPE_S32, 1},
	{"input.joy.speed", (void*)0x0002B348, TYPE_S32, 1},
	// sound
	{"sound.channels", (void*)0x00075C94, TYPE_S32, 1},
	{"sound.device.sfx", (void*)0x00029200, TYPE_S32, 1},
	{"sound.device.music", (void*)0x00029204, TYPE_S32, 1},
	{"sound.volume.sfx", (void*)0x0002B6B8, TYPE_S32, 1},
	{"sound.volume.music", (void*)0x0002B6B4, TYPE_S32, 1},
	{"sound.sb.port", (void*)0x00029208, TYPE_S32, 1},
	{"sound.sb.irq", (void*)0x000291EC, TYPE_S32, 1},
	{"sound.sb.dma", (void*)0x000291E0, TYPE_S32, 1},
	{"sound.m.port", (void*)0x000291E4, TYPE_S32, 1},
	// screen
	{"display.messages", (void*)0x0002B6B0, TYPE_S32, 1},
	{"display.size", (void*)0x0002B698, TYPE_S32, 1},
	{"display.gamma", (void*)0x00074FC0, TYPE_S32, 1},
	{"display.xhair.type", &extra_config.crosshair_type, TYPE_U8},
	{"display.xhair.red", &extra_config.crosshair_red, TYPE_U8},
	{"display.xhair.green", &extra_config.crosshair_green, TYPE_U8},
	{"display.xhair.blue", &extra_config.crosshair_blue, TYPE_U8},
	// player
	{"player.autorun", (void*)0x0001FBC5, TYPE_U8, 2},
	{"player.autoaim", &extra_config.auto_aim, TYPE_U8},
	{"player.mouselook", &extra_config.mouse_look, TYPE_U8},
	{"player.weapon.switch", &extra_config.auto_switch, TYPE_U8},
	{"player.weapon.center", &extra_config.center_weapon, TYPE_U8},
	// terminator
	{NULL}
};

static config_entry_t config_mod[] =
{
	{"decorate.enable", &mod_config.enable_decorate, TYPE_U8},
	{"dehacked.enable", &mod_config.enable_dehacked, TYPE_U8},
};

static const hook_t def_set[];

//
// funcs

static uint32_t parse_value(config_entry_t *conf)
{
	uint8_t *kw, *kv;
	int32_t value;

	while(conf->name)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			break;

		kv = tp_get_keyword_lc();
		if(!kv)
			break;

		if(!strcmp(conf->name, kw))
		{
			switch(conf->type)
			{
				case TYPE_U8:
					if(doom_sscanf(kv, "%d", &value) == 1 && value >= 0 && value < 256)
						*conf->u8 = value;
				break;
				case TYPE_S32:
					if(doom_sscanf(kv, "%d", &value) == 1)
						*conf->s32 = value;
				break;
			}
			break;
		}
		conf++;
	}
}

//
// API

void init_config()
{
	player_info_t *pli;
	config_entry_t *conf;
	int32_t lump;

	// relocate pointers
	conf = config_game;
	while(conf->name)
	{
		if(conf->relocate == 1)
			conf->ptr = conf->ptr + doom_data_segment;
		else
		if(conf->relocate == 2)
			conf->ptr = conf->ptr + doom_code_segment;
		conf++;
	}

	// load config
	if(!tp_load_file(ACE_CONFIG_FILE))
	{
		doom_printf("[ACE] loading game config ...\n");
		while(parse_value(config_game));
	} else
	{
		// try to load original config
		doom_printf("[ACE] loading DOOM config ...\n");
		M_LoadDefaults();
	}

	// forced values
	utils_install_hooks(def_set, 0);

	// mod configuration
	lump = wad_check_lump("ACE_CONF");
	if(lump >= 0)
	{
		doom_printf("[ACE] loading mod config ...\n");
		tp_load_lump(*lumpinfo + lump);
		while(parse_value(config_mod));
	}

	// check controls
	control_setup();

	// player setup
	pli = player_info + *consoleplayer;
	extra_config.auto_switch = !!extra_config.auto_switch;
	extra_config.auto_aim = !!extra_config.auto_aim;
	if(extra_config.mouse_look > 2)
		extra_config.mouse_look = 0;
/*
	// PLAYER FLAGS ARE FORCED TO UPDATE WHEN GAME STARTS
	// TODO: do this when sending net-game info, with player class
	pli->flags |= (uint32_t)extra_config.auto_switch << plf_auto_switch;
	pli->flags |= (uint32_t)extra_config.auto_aim << plf_auto_aim;
	pli->flags |= (uint32_t)!!(extra_config.mouse_look) << plf_mouse_look;
*/
}

static __attribute((regparm(2),no_caller_saved_registers))
void config_save()
{
	void *f;
	config_entry_t *conf;

	f = doom_fopen(ACE_CONFIG_FILE, "w");
	if(!f)
		return;

	doom_fprintf(f, "// ACE Engine config file //\n");

	conf = config_game;
	while(conf->name)
	{
		doom_fprintf(f, "%s\t", conf->name);
		switch(conf->type)
		{
			case TYPE_U8:
				doom_fprintf(f, "%u\n", *conf->u8);
			break;
			case TYPE_S32:
				doom_fprintf(f, "%d\n", *conf->s32);
			break;
		}
		conf++;
	}

	doom_fclose(f);
}

//
// hooks

static const hook_t def_set[] =
{
	// mousebstrafe = -1
	{0x0002B36C, DATA_HOOK | HOOK_UINT32, -1},
	// detaillevel = 0
	{0x0002B6AC, DATA_HOOK | HOOK_UINT32, 0},
	// terminator
	{0}
};

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// repace 'M_SaveDefaults'
	{0x00024300, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)config_save},
	// invert 'run key' logic (auto run)
	{0x0001FBC5, CODE_HOOK | HOOK_UINT8, 0x01},
};

