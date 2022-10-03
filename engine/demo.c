// kgsws' ACE Engine
////
// Demo files ... TBD.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "player.h"
#include "map.h"
#include "filebuf.h"
#include "demo.h"

#define OLD_VERSION	0x6D

typedef struct
{
	uint8_t version;
	uint8_t skill;
	uint8_t episode;
	uint8_t map;
	uint8_t deathmatch;
	uint8_t respawnparm;
	uint8_t fastparm;
	uint8_t nomonsters;
	uint8_t consoleplayer;
	uint8_t playeringame[MAXPLAYERS];
} __attribute__((packed)) old_header_t;

typedef struct
{
	int8_t forwardmove;
	int8_t sidemove;
	uint8_t angleturn;
	uint8_t buttons;
} __attribute__((packed)) old_ticcmd_t;

//

static uint8_t **defdemoname;

uint32_t *netdemo;
uint32_t *demoplayback;
uint32_t *demorecording;

uint32_t demo_map_mode;

static uint32_t *singledemo;

//
// new demo handling

__attribute((regparm(2),no_caller_saved_registers))
static void demo_close_read()
{
	reader_close();
}

__attribute((regparm(2),no_caller_saved_registers))
static void demo_read_ticcmd(ticcmd_t *cmd)
{
	old_ticcmd_t dmd;

	if(reader_get(&dmd, sizeof(dmd)) || dmd.forwardmove == 0x80 || dmd.buttons & BT_SPECIAL)
	{
		G_CheckDemoStatus();
		return;
	}

	cmd->forwardmove = dmd.forwardmove;
	cmd->sidemove = dmd.sidemove;
	cmd->angleturn = dmd.angleturn << 8;
	cmd->pitchturn = 0;
	cmd->chatchar = 0;
	cmd->buttons = dmd.buttons & (BT_ATTACK | BT_USE);

	if(dmd.buttons & BT_CHANGE)
		// translate old weapon change to new format
		cmd->buttons |= ((((dmd.buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT) + 2) << BT_ACTIONSHIFT);
}

__attribute((regparm(2),no_caller_saved_registers))
static void do_play_demo()
{
	int32_t lump;
	old_header_t header;

	*gameaction = ga_nothing;

	lump = W_CheckNumForName(*defdemoname);
	if(lump < 0)
		goto skip_demo;
	reader_open_lump(lump);

	if(reader_get(&header, sizeof(header)))
		goto close_skip_demo;

	if(header.version != OLD_VERSION)
		goto close_skip_demo;

	if(header.episode < 0 || header.episode > 9)
		goto close_skip_demo;

	if(header.map > 99)
		goto close_skip_demo;

	*deathmatch = header.deathmatch;
	*respawnparm = header.respawnparm;
	*fastparm = header.fastparm;
	*nomonsters = header.nomonsters;
	*consoleplayer = header.consoleplayer;

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		playeringame[i] = header.playeringame[i];
		players[i].playerstate = PST_REBORN;
	}

	if(playeringame[1])
	{
		*netgame = 1;
		*netdemo = 1;
	}

	*demoplayback = DEMO_OLD;
	*prndindex = 0;

	if(demo_map_mode)
		doom_sprintf(map_lump.name, "MAP%02u", header.map);
	else
		doom_sprintf(map_lump.name, "E%uM%u", header.episode, header.map);
	map_load_setup();

	return;

close_skip_demo:
	reader_close();
skip_demo:
	*singledemo = 0;
	if(*gamestate != GS_DEMOSCREEN)
	{
		*gamestate = GS_DEMOSCREEN;
		map_start_title();
	}
	*gameaction = ga_nothing;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x0002B3E4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&demoplayback},
	{0x0002B3F0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&demorecording},
	// replace 'G_DoPlayDemo'
	{0x00021AD0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)do_play_demo},
	// replace call to 'G_ReadDemoTiccmd' in 'G_Ticker'
	{0x00020607, CODE_HOOK | HOOK_UINT16, 0xD889},
	{0x00020609, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)demo_read_ticcmd},
	{0x0002060E, CODE_HOOK | HOOK_UINT16, 0x43EB},
	// replace call to 'Z_ChangeTag' in 'G_CheckDemoStatus'
	{0x00021C8A, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)demo_close_read},
	{0x00021C8F, CODE_HOOK | HOOK_UINT32, 0x28EBC931},
	// disable 'demorecording'
	{0x0002B3F0, DATA_HOOK | HOOK_UINT32, 0},
	// import variables
	{0x0002B2E8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&defdemoname},
	{0x0002B388, DATA_HOOK | HOOK_IMPORT, (uint32_t)&netdemo},
	{0x0002B3B4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&singledemo},
};

