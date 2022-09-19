// kgsws' ACE Engine
////
// This handles both game and mod configuration.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
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

static config_entry_t game_config[] =
{
	// keys
	{"input.key.move.forward", (void*)0x0002B344, TYPE_U8, 1},
	{"input.key.move.backward", (void*)0x0002B35C, TYPE_U8, 1},
	{"input.key.strafe.left", (void*)0x0002B360, TYPE_U8, 1},
	{"input.key.strafe.right", (void*)0x0002B390, TYPE_U8, 1},
	{"input.key.turn.left", (void*)0x0002B364, TYPE_U8, 1},
	{"input.key.turn.right", (void*)0x0002B394, TYPE_U8, 1},
	{"input.key.attack.primary", (void*)0x0002B384, TYPE_U8, 1},
	{"input.key.attack.secondary", &key_fire_alt, TYPE_U8},
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
	{"input.mouse.left", &mouse_button[0], TYPE_U8},
	{"input.mouse.right", &mouse_button[1], TYPE_U8},
	{"input.mouse.middle", &mouse_button[2], TYPE_U8},
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
	{"screen.messages", (void*)0x0002B6B0, TYPE_S32, 1},
	{"screen.size", (void*)0x0002B698, TYPE_S32, 1},
	{"screen.gamma", (void*)0x00074FC0, TYPE_S32, 1},
	// terminator
	{NULL}
};

//
// API

void init_config()
{
	config_entry_t *conf;
	uint8_t *kw, *kv;
	int32_t value;

	// relocate pointers
	conf = game_config;
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

		// process config
		while(1)
		{
			kw = tp_get_keyword_lc();
			if(!kw)
				break;

			kv = tp_get_keyword_lc();
			if(!kv)
				break;

			conf = game_config;
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
	} else
	{
		// try to load original config
		doom_printf("[ACE] loading DOOM config ...\n");
		M_LoadDefaults();
	}

	// TODO: forced values
	// detaillevel = 1
	// snd_sfxdevice = disable PC speaker

	// check controls
	control_setup();
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

	conf = game_config;
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
};

