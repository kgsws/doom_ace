// kgsws' ACE Engine
////
// This handles both game and mod configuration.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "player.h"
#include "map.h"
#include "textpars.h"
#include "controls.h"
#include "config.h"

#define ACE_CONFIG_FILE	"ace.cfg"

enum
{
	TYPE_U8,
	TYPE_U16,
	TYPE_S32,
};

typedef struct
{
	const uint8_t *name;
	union
	{
		void *ptr;
		uint8_t *u8;
		uint16_t *u16;
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
	{"input.key.move.forward", &key_up, TYPE_U8},
	{"input.key.move.backward", &key_down, TYPE_U8},
	{"input.key.strafe.left", &key_strafeleft, TYPE_U8},
	{"input.key.strafe.right", &key_straferight, TYPE_U8},
	{"input.key.turn.left", &key_left, TYPE_U8},
	{"input.key.turn.right", &key_right, TYPE_U8},
	{"input.key.jump", &key_jump, TYPE_U8},
	{"input.key.atk.pri", &key_fire, TYPE_U8},
	{"input.key.atk.sec", &key_fire_alt, TYPE_U8},
	{"input.key.use", &key_use, TYPE_U8},
	{"input.key.inv.use", &key_inv_use, TYPE_U8},
	{"input.key.inv.next", &key_inv_next, TYPE_U8},
	{"input.key.inv.previous", &key_inv_prev, TYPE_U8},
	{"input.key.speed", &key_speed, TYPE_U8},
	{"input.key.strafe", &key_strafe, TYPE_U8},
	{"input.key.cheats", (void*)0x0003BA1B, TYPE_U8, 1},
	// mouse
	{"input.mouse.enable", &usemouse, TYPE_S32},
	{"input.mouse.sensitivity", &mouseSensitivity, TYPE_S32},
	{"input.mouse.atk.pri", &mouseb_fire, TYPE_S32},
	{"input.mouse.atk.sec", &mouseb_fire_alt, TYPE_S32},
	{"input.mouse.use", &mouseb_use, TYPE_S32},
	{"input.mouse.inv.use", &mouseb_inv_use, TYPE_S32},
	{"input.mouse.strafe", &mouseb_strafe, TYPE_S32},
	// joystick
	{"input.joy.enable", &usejoystick, TYPE_S32},
	{"input.joy.fire", &joyb_fire, TYPE_S32},
	{"input.joy.strafe", &joyb_strafe, TYPE_S32},
	{"input.joy.use", &joyb_use, TYPE_S32},
	{"input.joy.speed", &joyb_speed, TYPE_S32},
	// sound
	{"sound.channels", &numChannels, TYPE_S32},
	{"sound.device.sfx", &snd_sfxdevice, TYPE_S32},
	{"sound.device.music", &snd_musicdevice, TYPE_S32},
	{"sound.volume.sfx", &snd_SfxVolume, TYPE_S32},
	{"sound.volume.music", &snd_MusicVolume, TYPE_S32},
	{"sound.sb.port", &snd_sbport, TYPE_S32},
	{"sound.sb.irq", &snd_sbirq, TYPE_S32},
	{"sound.sb.dma", &snd_sbdma, TYPE_S32},
	{"sound.m.port", &snd_mport, TYPE_S32},
	// screen
	{"display.messages", &showMessages, TYPE_S32},
	{"display.size", &screenblocks, TYPE_S32},
	{"display.gamma", &usegamma, TYPE_S32},
	{"display.xhair.type", &extra_config.crosshair_type, TYPE_U8},
	{"display.xhair.red", &extra_config.crosshair_red, TYPE_U8},
	{"display.xhair.green", &extra_config.crosshair_green, TYPE_U8},
	{"display.xhair.blue", &extra_config.crosshair_blue, TYPE_U8},
	// player
	{"player.autorun", (void*)0x0001FBC5, TYPE_U8, 1},
	{"player.autoaim", &extra_config.auto_aim, TYPE_U8},
	{"player.mouselook", &extra_config.mouse_look, TYPE_U8},
	{"player.weapon.switch", &extra_config.auto_switch, TYPE_U8},
	{"player.weapon.center", &extra_config.center_weapon, TYPE_U8},
	// terminator
	{NULL}
};

static config_entry_t config_mod[] =
{
	{"render.visplane.count", &mod_config.visplane_count, TYPE_U16},
	{"render.vissprite.count", &mod_config.vissprite_count, TYPE_U16},
	{"render.drawseg.count", &mod_config.drawseg_count, TYPE_U16},
	{"render.e3dplane.count", &mod_config.e3dplane_count, TYPE_U16},
	{"decorate.enable", &mod_config.enable_decorate, TYPE_U8},
	{"dehacked.enable", &mod_config.enable_dehacked, TYPE_U8},
	// terminator
	{NULL}
};

//
// funcs

static uint32_t parse_value(config_entry_t *conf_def)
{
	uint8_t *kw, *kv;
	int32_t value;
	config_entry_t *conf;

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			break;

		kv = tp_get_keyword_lc();
		if(!kv)
			break;

		conf = conf_def;
		while(conf->name)
		{
			if(!strcmp(conf->name, kw))
			{
				switch(conf->type)
				{
					case TYPE_U8:
						if(doom_sscanf(kv, "%d", &value) == 1 && value >= 0 && value < 256)
							*conf->u8 = value;
					break;
					case TYPE_U16:
						if(doom_sscanf(kv, "%d", &value) == 1 && value >= 0 && value < 65535)
							*conf->u16 = value;
					break;
					case TYPE_S32:
						if(doom_sscanf(kv, "%d", &value) == 1)
							*conf->s32 = value;
					break;
				}
			}
			conf++;
		}
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
		if(conf->relocate)
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
	mouseb_forward = -1;
	detaillevel = 0;

	// mod configuration
	lump = wad_check_lump("ACE_CONF");
	if(lump >= 0)
	{
		doom_printf("[ACE] loading mod config ...\n");
		tp_load_lump(lumpinfo + lump);
		while(parse_value(config_mod));
	}

	// check controls
	control_setup();

	// player setup
	pli = player_info + consoleplayer;
	extra_config.auto_switch = !!extra_config.auto_switch;
	extra_config.auto_aim = !!extra_config.auto_aim;
	if(extra_config.mouse_look > 2)
		extra_config.mouse_look = 0;
	if(extra_config.center_weapon > 2)
		extra_config.center_weapon = 0;
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

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// repace 'M_SaveDefaults'
	{0x00024300, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)config_save},
	// invert 'run key' logic (auto run)
	{0x0001FBC5, CODE_HOOK | HOOK_UINT8, 0x01},
	// new defaults
	{0x0002B698, DATA_HOOK | HOOK_UINT32, 11}, // screenblocks
};

