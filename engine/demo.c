// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "player.h"
#include "map.h"
#include "decorate.h"
#include "net.h"
#include "demo.h"

#define DEMO_MAGIC	0xA7B54D3A	// just a random number

typedef struct
{
	uint32_t magic;
	uint32_t version;
	uint32_t mod_csum;
	uint16_t rngindex;
	uint16_t flags;
	uint8_t skill;
	uint8_t netgame;
	uint32_t players;
	uint8_t maplump[8];
} demo_header_t;

//

static ticcmd_t last_ticcmd[MAXPLAYERS];
static uint8_t buffer[1024];
static uint8_t *bptr;
static int32_t demofd;
static uint32_t demoffs;
static uint32_t demotic;
static player_info_t demobackup;

//
// reading

static void bufrd(void *data, uint32_t size)
{
	uint32_t left = sizeof(buffer) - (bptr - buffer);

	while(size)
	{
		uint32_t rl;

		if(!left)
		{
			if(demoffs >= demoplayback)
				engine_error("DEMO", "Demo is incomplete!");
			doom_lseek(demofd, demoffs, SEEK_SET);
			doom_read(demofd, buffer, sizeof(buffer));
			demoffs += sizeof(buffer);
			left = sizeof(buffer);
			bptr = buffer;
		}

		rl = size > left ? left : size;

		memcpy(data, bptr, rl);

		data += rl;
		size -= rl;

		bptr += rl;
		left -= rl;
	}
}

__attribute((regparm(2),no_caller_saved_registers))
static void demo_close_read()
{
	memcpy(player_info, &demobackup, sizeof(player_info_t));
}

__attribute((regparm(2),no_caller_saved_registers))
static void demo_read_ticcmd(ticcmd_t *cmd)
{
	uint32_t pidx, pdst;
	ticcmd_t tmpcmd;

	if(!demoplayback)
		return;

	pdst = demotic >> 24;
	if(	pdst >= MAXPLAYERS &&
		(demotic & 0x00FFFFFF) == (leveltime & 0x00FFFFFF)
	)
	{
		G_CheckDemoStatus();
		usergame = 1;
		demoplayback = 0;
		return;
	}

	if(	pdst == pidx &&
		(demotic & 0x00FFFFFF) == (leveltime & 0x00FFFFFF)
	){
		bufrd(last_ticcmd + pidx, sizeof(ticcmd_t));
		bufrd(&demotic, sizeof(demotic));
		if(last_ticcmd[pidx].buttons & BT_SPECIAL)
			last_ticcmd[pidx].buttons = 0;
	}

	memcpy(cmd, last_ticcmd + pidx, sizeof(ticcmd_t));
}

__attribute((regparm(2),no_caller_saved_registers))
static void do_play_demo()
{
	demo_header_t header;
	int32_t lump;

	gameaction = ga_nothing;

	lump = W_CheckNumForName(defdemoname);
	if(lump < 0)
		goto demo_fail;

	demofd = lumpinfo[lump].fd;
	demoffs = lumpinfo[lump].offset;
	demoplayback = demoffs + lumpinfo[lump].size;

	if(!lumpinfo[lump].size)
		goto demo_fail;

	bptr = buffer + sizeof(buffer);

	bufrd(&header, sizeof(header));

	if(header.magic != DEMO_MAGIC)
		goto demo_fail;

	if(header.version != ace_git_version)
		goto demo_fail;

	if(header.mod_csum != dec_mod_csum)
		goto demo_fail;

	net_parse_flags(header.flags);

	gameskill = header.skill;
	netgame = header.netgame;
	prndindex = header.rngindex;

	memcpy(&demobackup, player_info, sizeof(player_info_t));

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		playeringame[i] = !!(header.players & (1 << i));
		if(playeringame[i])
		{
			players[i].state = PST_REBORN;
			bufrd(player_info + i, sizeof(player_info_t));
		}
	}

	if(netgame)
		netdemo = 1;

	memcpy(map_lump.name, header.maplump, 8);

	prndindex = 0;
	gameepisode = 1;
	map_load_setup(1);

	usergame = 0;

	bufrd(&demotic, sizeof(demotic));

	return;

demo_fail:
	if(M_CheckParm(dtxt_playdemo) || M_CheckParm(dtxt_timedemo))
		engine_error("DEMO", "Unable to play this demo!");

	usergame = 1;
	demoplayback = 0;
	singledemo = 0;
	if(gamestate != GS_DEMOSCREEN)
	{
		gamestate = GS_DEMOSCREEN;
		map_start_title();
	}
	gameaction = ga_nothing;
}

//
// writing

static void bufwr(void *data, uint32_t size)
{
	uint32_t left = sizeof(buffer) - (bptr - buffer);

	while(size)
	{
		uint32_t wl = size > left ? left : size;

		memcpy(bptr, data, wl);

		bptr += wl;
		left -= wl;

		data += wl;
		size -= wl;

		if(!left)
		{
			uint32_t ret;
			ret = doom_write(demorecording, buffer, sizeof(buffer));
			if(ret != sizeof(buffer))
				engine_error("DEMO", "Unable to write the data!");
			left = sizeof(buffer);
			bptr = buffer;
		}
	}
}

__attribute((regparm(2),no_caller_saved_registers))
static void demo_close_write()
{
	uint32_t pidx = 0xFF000000;

	pidx |= leveltime & 0x00FFFFFF;

	bufwr(&pidx, sizeof(uint32_t));

	if(bptr != buffer)
	{
		uint32_t ret;
		uint32_t size = bptr - buffer;
		ret = doom_write(demorecording, buffer, size);
		if(ret != size)
			engine_error("DEMO", "Unable to write the data!");
	}

	doom_close(demorecording);

	engine_error("", "Demo was successfully recorded.");
}

__attribute((regparm(2),no_caller_saved_registers))
static void demo_write_ticcmd(ticcmd_t *cmd)
{
	uint32_t pidx;

	if(paused)
	{
		if(cmd->buttons != (BT_SPECIAL|BTS_PAUSE))
			return;
	} else
	if(cmd->buttons == (BT_SPECIAL|BTS_PAUSE))
		return;

	if(menuactive && !netgame)
		return;

	pidx = (uint32_t)&players[0].cmd - (uint32_t)cmd;

	if(!memcmp(last_ticcmd + pidx, cmd, sizeof(ticcmd_t)))
		return;

	memcpy(last_ticcmd + pidx, cmd, sizeof(ticcmd_t));

	pidx <<= 24;
	pidx |= leveltime & 0x00FFFFFF;

	bufwr(&pidx, sizeof(uint32_t));
	bufwr(cmd, sizeof(ticcmd_t));
}

__attribute((regparm(2),no_caller_saved_registers))
static void do_record_start()
{
	demo_header_t header;

	memset(last_ticcmd, 0xFF, sizeof(last_ticcmd));

	bptr = buffer;

	header.magic = DEMO_MAGIC;
	header.version = ace_git_version;
	header.mod_csum = dec_mod_csum;
	header.rngindex = prndindex;
	header.flags = net_make_flags();
	header.skill = gameskill;
	header.netgame = netgame;
	header.players = 0;
	memcpy(header.maplump, map_lump.name, 8);

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
		if(playeringame[i])
			header.players |= 1 << i;

	bufwr(&header, sizeof(header));

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
		if(playeringame[i])
			bufwr(player_info + i, sizeof(player_info_t));
}

__attribute((regparm(2),no_caller_saved_registers))
static void do_record_demo(uint8_t *name)
{
	doom_sprintf(buffer, "%s.lmp", name);
	demorecording = doom_open_WR(buffer);
	if(demorecording <= 0)
		engine_error("DEMO", "Unable to create the file!");
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace 'G_DoPlayDemo'
	{0x00021AD0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)do_play_demo},
	// replace 'G_RecordDemo'
	{0x00021970, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)do_record_demo},
	// replace call to 'G_BeginRecording' in 'D_DoomLoop'
	{0x0001D520, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)do_record_start},
	// replace call to 'G_ReadDemoTiccmd' in 'G_Ticker'
	{0x00020607, CODE_HOOK | HOOK_UINT16, 0xD889},
	{0x00020609, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)demo_read_ticcmd},
	{0x0002060E, CODE_HOOK | HOOK_UINT16, 0x43EB},
	// replace call to 'G_WriteDemoTiccmd' in 'G_Ticker'
	{0x00020660, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)demo_write_ticcmd},
	// replace call to 'Z_ChangeTag' in 'G_CheckDemoStatus'
	{0x00021C8A, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)demo_close_read},
	{0x00021C8F, CODE_HOOK | HOOK_UINT32, 0x28EBC931},
	// replace write end in 'G_CheckDemoStatus'
	{0x00021D16, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)demo_close_write},
	{0x00021D1B, CODE_HOOK | HOOK_UINT16, 0x44EB},
};

