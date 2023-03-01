// kgsws' ACE Engine
////
// Network initialization.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "config.h"
#include "render.h"
#include "player.h"
#include "decorate.h"
#include "cheat.h"
#include "font.h"
#include "map.h"
#include "rng.h"
#include "net.h"

#define VERSION_MARK	0xAC

#define COLOR_PLAYER_DISABLED	'm'
#define COLOR_PSTATE_INITIAL	'r'
#define COLOR_PSTATE_READY	'd'
#define COLOR_PSTATE_WAITING	'i'
#define COLOR_PSTATE_ERROR	'g'
#define COLOR_PSTATE_ITSME	'n'
#define COLOR_PSTATE_INGAME	'y'
#define COLOR_PINFO_INITIAL	'u'
#define COLOR_PINFO_ERROR	'f'
#define COLOR_PINFO_CLASS	'c'

#define	NCMD_EXIT	0x80000000
#define	NCMD_RETRANSMIT	0x40000000
#define	NCMD_SETUP	0x20000000
#define	NCMD_KILL	0x10000000
#define	NCMD_CHECKSUM	0x0FFFFFFF

#define MAX_ITEM_TIME	60

#define FLAG_COUNT	5

enum
{
	ERROR_DISCONNECT = 252,
	ERROR_BAD_MOD = 253,
	ERROR_BAD_VERSION = 254,
	UNKNOWN_PLAYER_CLASS = 255
};

enum
{
	MENU_PLAYER_CLASS, // intentionaly first
	MENU_GAME_MAP,
	MENU_GAME_SKILL,
	MENU_GAME_MODE,
	MENU_ITEM_TIME,
	MENU_INVENTORY,
	MENU_GAME_FLAGS, // intentionaly last
	//
	NUM_MENUS
};

enum
{
	TYPE_SIMPLE,
	TYPE_MAP,
	TYPE_CLASS,
	TYPE_FLAGS,
	TYPE_NUMERIC,
};

typedef struct
{
	uint8_t text[64];
	uint32_t version;
	uint32_t mod_csum;
	uint32_t menu_yh;
	uint32_t menu_y0;
	uint32_t menu_yf;
	int32_t menu_now;
	int32_t menu_sel[NUM_MENUS];
	int32_t menu_top[2]; // class / map
	void *color_title[2];
	void *color_menu[4];
	int16_t flag_x[FLAG_COUNT];
	uint16_t prng_idx;
	uint8_t menu_flags;
	uint8_t player_state[MAXPLAYERS];
	uint8_t player_info[MAXPLAYERS];
	uint8_t node_check[MAXPLAYERS];
	uint8_t key_check[MAXPLAYERS];
	uint8_t force_update;
	uint8_t start_check;
	uint8_t map_string;
} net_setup_t;

typedef struct
{
	uint8_t *title;
	const uint8_t **options;
	uint8_t maxsel;
	uint8_t type;
	uint8_t value;
} net_menu_t;

//

static const uint8_t *menu_skill[] =
{
	"baby",
	"easy",
	"medium",
	"hard",
	"nightmare",
};

static const uint8_t *menu_mode[] =
{
	"coop",
	"survival",
	"deathmatch",
	"altdeath",
};

static const uint8_t *menu_flags[FLAG_COUNT] =
{
	"enable cheats",
	"friendly fire",
	"no monsters",
	"fast monsters",
	"respawn monsters",
};

static const uint8_t *menu_inventory[] =
{
	"discard",
	"keep",
	"drop",
	"drop locked",
};

static net_menu_t net_menu[] =
{
	[MENU_PLAYER_CLASS] =
	{
		.title = "CLASS",
		.type = TYPE_CLASS
	},
	[MENU_GAME_MAP] =
	{
		.title = "MAP",
		.type = TYPE_MAP
	},
	[MENU_GAME_SKILL] =
	{
		.title = "SKILL",
		.type = TYPE_SIMPLE,
		.options = menu_skill,
		.maxsel = sizeof(menu_skill) / sizeof(uint8_t*)
	},
	[MENU_GAME_MODE] =
	{
		.title = "MODE",
		.type = TYPE_SIMPLE,
		.options = menu_mode,
		.maxsel = sizeof(menu_mode) / sizeof(uint8_t*)
	},
	[MENU_ITEM_TIME] =
	{
		.title = "ITEM TIME",
		.type = TYPE_NUMERIC,
		.maxsel = 121
	},
	[MENU_INVENTORY] =
	{
		.title = "INVENTORY",
		.type = TYPE_SIMPLE,
		.options = menu_inventory,
		.maxsel = sizeof(menu_inventory) / sizeof(uint8_t*)
	},
	[MENU_GAME_FLAGS] =
	{
		.title = "FLAGS",
		.type = TYPE_FLAGS,
		.options = menu_flags,
		.maxsel = FLAG_COUNT
	},
};

//
// network screen

static void draw_map_menu(net_setup_t *ns, int32_t mx)
{
	map_level_t *info = map_info;
	int32_t sel = ns->menu_sel[MENU_GAME_MAP];

	if(sel > net_menu[MENU_GAME_MAP].maxsel)
		sel = 0;

	if(ns->menu_top[1] + 6 < sel)
		ns->menu_top[1] = sel - 6;
	else
	if(ns->menu_top[1] > sel)
		ns->menu_top[1] = sel;

	sel = ns->menu_top[1];

	for(uint32_t i = 0; i < sel; )
	{
		if(info->lump >= 0)
			i++;
		info++;
	}

	for(int32_t i = 0; i < 7; i++, sel++)
	{
		uint32_t idx = 0;
		uint8_t *text;

		if(sel >= net_menu[MENU_GAME_MAP].maxsel)
			break;

		if(sel == ns->menu_sel[MENU_GAME_MAP])
		{
			idx++;
			if(ns->menu_now == MENU_GAME_MAP)
				idx += 2;
		}

		font_color = ns->color_menu[idx];
		if(!info->name || !ns->map_string)
			text = lumpinfo[info->lump].name; // this assumes info->fd < 16777216
		else
			text = info->name;

		font_center_text(mx, ns->menu_y0 + ns->menu_yh * (i + 1) + 5, text, smallfont, 0);

		info++;
	}
}

static void draw_class_menu(net_setup_t *ns, int32_t mx, uint32_t left)
{
	int32_t sel = ns->menu_sel[MENU_PLAYER_CLASS];

	if(sel > net_menu[MENU_PLAYER_CLASS].maxsel)
		sel = 0;

	if(ns->menu_top[0] + 6 < sel)
		ns->menu_top[0] = sel - 6;
	else
	if(ns->menu_top[0] > sel)
		ns->menu_top[0] = sel;

	sel = ns->menu_top[0];

	for(int32_t i = 0; i < 7; i++, sel++)
	{
		uint32_t idx = 0;

		if(sel >= net_menu[MENU_PLAYER_CLASS].maxsel)
			break;

		if(sel == ns->menu_sel[MENU_PLAYER_CLASS])
		{
			idx++;
			if(ns->menu_now == MENU_PLAYER_CLASS)
				idx += 2;
		}

		font_color = ns->color_menu[idx];

		if(left)
			font_draw_text(mx, ns->menu_y0 + ns->menu_yh * (i + 1) + 5, mobjinfo[player_class[sel]].player.name, smallfont);
		else
			font_center_text(mx, ns->menu_y0 + ns->menu_yh * (i + 1) + 5, mobjinfo[player_class[sel]].player.name, smallfont, 0);
	}
}

static void menu_key(net_setup_t *ns)
{
	int32_t mi;

	mi = ns->menu_now - 1;
	if(mi < 0)
		mi += NUM_MENUS;

	for(int32_t mx = 20; mx <= SCREENWIDTH; mx += 140)
	{
		// title
		font_color = ns->color_title[ns->menu_now != mi];
		font_center_text(mx, ns->menu_y0, net_menu[mi].title, smallfont, 0);

		// items
		switch(net_menu[mi].type)
		{
			case TYPE_SIMPLE:
				for(uint32_t i = 0; i < net_menu[mi].maxsel; i++)
				{
					uint32_t idx = 0;

					if(i == ns->menu_sel[mi])
					{
						idx++;
						if(ns->menu_now == mi)
							idx += 2;
					}

					font_color = ns->color_menu[idx];
					font_center_text(mx, ns->menu_y0 + ns->menu_yh * (i + 1) + 5, net_menu[mi].options[i], smallfont, 0);
				}
			break;
			case TYPE_MAP:
				draw_map_menu(ns, mx);
			break;
			case TYPE_CLASS:
				draw_class_menu(ns, mx, 0);
			break;
			case TYPE_FLAGS:
				for(uint32_t i = 0; i < net_menu[mi].maxsel; i++)
				{
					uint32_t idx = 0;

					if(ns->menu_flags & (1 << i))
						idx |= 1;

					if(ns->menu_now == mi && i == ns->menu_sel[mi])
						idx |= 2;

					font_color = ns->color_menu[idx];
					font_center_text(mx, ns->menu_y0 + ns->menu_yh * (i + 1) + 5, net_menu[mi].options[i], smallfont, 0);
				}
			break;
			case TYPE_NUMERIC:
				font_color = ns->color_menu[(ns->menu_now == mi) << 1];
				doom_sprintf(ns->text, "%u", ns->menu_sel[mi]);
				font_center_text(mx, ns->menu_y0 + ns->menu_yh + 5, ns->text, smallfont, 0);
			break;
		}

		// next
		mi++;
		if(mi >= NUM_MENUS)
			mi = 0;
	}
}

static void menu_node(net_setup_t *ns)
{
	int32_t my;
	int32_t mx = 240;

	// player class
	font_color = ns->color_title[0];
	font_draw_text(20, ns->menu_y0, net_menu[MENU_PLAYER_CLASS].title, smallfont);
	draw_class_menu(ns, 2, 1);

	// other options
	my = ns->menu_y0 - (ns->menu_yh / 2);
	for(uint32_t i = MENU_PLAYER_CLASS+1; i < MENU_GAME_FLAGS; i++)
	{
		const uint8_t *text = "N/A";

		font_color = ns->color_title[0];
		font_center_text(mx, my, net_menu[i].title, smallfont, 0);
		my += ns->menu_yh;

		if(ns->start_check != 0xFF)
		{
			font_color = ns->color_menu[2 + (ns->start_check == i)];
			switch(net_menu[i].type)
			{
				case TYPE_SIMPLE:
					text = net_menu[i].options[ns->menu_sel[i]];
				break;
				case TYPE_MAP:
				{
					map_level_t *info = map_info;
					for(uint32_t ii = 0; ii < ns->menu_sel[MENU_GAME_MAP]; )
					{
						if(info->lump >= 0)
							ii++;
						info++;
					}
					if(!info->name || !ns->map_string)
						text = lumpinfo[info->lump].name; // this assumes info->fd < 16777216
					else
						text = info->name;
				}
				break;
				case TYPE_NUMERIC:
					doom_sprintf(ns->text, "%u", ns->menu_sel[i]);
					text = ns->text;
				break;
			}
		} else
			font_color = ns->color_menu[0];

		font_center_text(mx, my, text, smallfont, 0);
		my += ns->menu_yh;
	}

	// flags
	for(uint32_t i = 0; i < FLAG_COUNT; i++)
	{
		font_color = ns->color_menu[!!(ns->menu_flags & (1 << i)) * 3];
		font_draw_text(ns->flag_x[i], ns->menu_yf + (i / 3) * ns->menu_yh, menu_flags[i], smallfont);
	}
}

static void update_screen(net_setup_t *ns, uint32_t stage)
{
	// background
	dwcopy(screen_buffer, screen_buffer + SCREENWIDTH*SCREENHEIGHT, SCREENWIDTH*SCREENHEIGHT / sizeof(uint32_t));

	if(stage > 1)
		goto skip;

	// player list
	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		uint8_t *text;

		if(player_info[i].playerclass < num_player_classes)
			text = mobjinfo[player_class[player_info[i].playerclass]].player.name;
		else
		if(player_info[i].playerclass == ERROR_DISCONNECT)
			text = "disconnected";
		else
		if(player_info[i].playerclass == ERROR_BAD_VERSION)
			text = "bad version";
		else
		if(player_info[i].playerclass == ERROR_BAD_MOD)
			text = "bad WAD file";
		else
			text = "  ----";

		doom_sprintf(ns->text, "\x1C%cPlayer%u  \x1C%c%s", ns->player_state[i], i, ns->player_info[i], text);
		font_center_text((SCREENWIDTH / 2), ns->menu_yh * i + 2, ns->text, smallfont, 0);
	}

	if(stage)
		goto skip;

	if(consoleplayer)
		menu_node(ns);
	else
		menu_key(ns);

skip:
	// update video
	I_FinishUpdate();
}

//
// packets

static void pkt_send(net_setup_t *ns, uint32_t stage)
{
	netbuffer->player = VERSION_MARK; // this is 'version' in old initialization

	if(consoleplayer)
	{
		// sending client packet
		netbuffer->starttic = 'N';
		netbuffer->numtics = 1 + ((sizeof(dc_net_node_t) - 1) / sizeof(ticcmd_t));

		// version
		netbuffer->net_node.version = ns->version;
		netbuffer->net_node.mod_csum = ns->mod_csum;
		netbuffer->net_node.key_check = ns->key_check[0];
		netbuffer->net_node.node_check = ns->node_check[0];

		// we need to know the origin
		netbuffer->retransmitfrom = consoleplayer;

		// player info
		netbuffer->net_node.pi = player_info[consoleplayer];
		netbuffer->net_node.pi.playerclass = ns->menu_sel[MENU_PLAYER_CLASS];
	} else
	{
		// sending server packet
		netbuffer->starttic = 'K';
		netbuffer->numtics = 1 + ((sizeof(dc_net_key_t) - 1) / sizeof(ticcmd_t));

		// version
		netbuffer->net_key.check = ns->key_check[0];
		netbuffer->net_key.game_info = ns->menu_sel[MENU_GAME_SKILL] | (ns->menu_sel[MENU_GAME_MODE] << 3) | (ns->menu_sel[MENU_INVENTORY] << 5);
		netbuffer->net_key.respawn = ns->menu_sel[MENU_ITEM_TIME];
		netbuffer->net_key.map_idx = ns->menu_sel[MENU_GAME_MAP];
		netbuffer->net_key.menu_idx = ns->menu_now;
		netbuffer->net_key.flags = ns->menu_flags;

		// stage
		netbuffer->retransmitfrom = stage;

		// random seed
		netbuffer->net_key.prng_idx = ns->prng_idx;

		// player info
		for(uint32_t i = 0; i < MAXPLAYERS; i++)
			netbuffer->net_key.pi[i] = player_info[i];
	}

	// there does not seem to be any reasponable way to determine which node is which player
	// node 0 is always current player, so skip that one
	// but then send this packet to everyone else
	for(uint32_t i = 1; i < doomcom->numnodes; i++)
		HSendPacket(i, NCMD_SETUP);
}

static void pkt_recv(net_setup_t *ns, uint32_t stage)
{
	for(uint32_t i = 0; i < 8; i++)
	{
		if(!HGetPacket())
			break;

		if(!(netbuffer->checksum & NCMD_SETUP))
		{
			uint32_t idx;

			if(!stage)
				continue;

			// this is (most likely) actual game packet
			idx = netbuffer->player & 0x7F;
			if(idx >= doomcom->numplayers)
				continue;

			if(ns->player_state[idx] != COLOR_PSTATE_INGAME)
				ns->force_update = 1;

			ns->start_check |= 1 << idx;
			ns->player_state[idx] = COLOR_PSTATE_INGAME;

			continue;
		}

		if(netbuffer->player != VERSION_MARK)
			continue;

		if(netbuffer->starttic == 'N')
		{
			uint32_t idx;

			if(consoleplayer)
				continue;

			// parse node packet
			idx = netbuffer->retransmitfrom;
			if(idx <= 0 || idx >= doomcom->numplayers)
				continue;
			ns->key_check[idx] = netbuffer->net_node.key_check;

			// update check
			if(ns->node_check[idx] == netbuffer->net_node.node_check)
			{
				if(ns->player_state[idx] != COLOR_PSTATE_WAITING)
					continue;
				// only update state color
				if(	player_info[idx].playerclass < num_player_classes ||
					player_info[idx].playerclass == UNKNOWN_PLAYER_CLASS
				)
					ns->player_state[idx] = COLOR_PSTATE_READY;
				else
					ns->player_state[idx] = COLOR_PSTATE_ERROR;
				ns->force_update = 1;
				continue;
			}
			ns->node_check[idx] = netbuffer->net_node.node_check;

			// don't accept any changes after start
			if(stage)
				continue;

			// full update
			ns->force_update = 3;

			// check version
			if(netbuffer->net_node.version != ns->version)
			{
				player_info[idx].playerclass = ERROR_BAD_VERSION;
				ns->player_state[idx] = COLOR_PSTATE_ERROR;
				ns->player_info[idx] = COLOR_PINFO_ERROR;
				continue;
			}

			// check WAD
			if(netbuffer->net_node.mod_csum != ns->mod_csum)
			{
				player_info[idx].playerclass = ERROR_BAD_MOD;
				ns->player_state[idx] = COLOR_PSTATE_ERROR;
				ns->player_info[idx] = COLOR_PINFO_ERROR;
				continue;
			}

			// player class
			player_info[idx] = netbuffer->net_node.pi;
			if(player_info[idx].playerclass < num_player_classes)
			{
				ns->player_state[idx] = COLOR_PSTATE_READY;
				ns->player_info[idx] = COLOR_PINFO_CLASS;
			} else
			if(player_info[idx].playerclass == UNKNOWN_PLAYER_CLASS)
			{
				ns->player_state[idx] = COLOR_PSTATE_READY;
				ns->player_info[idx] = COLOR_PINFO_INITIAL;
			} else
			{
				ns->player_state[idx] = COLOR_PSTATE_ERROR;
				ns->player_info[idx] = COLOR_PINFO_ERROR;
			}
		} else
		if(netbuffer->starttic == 'K')
		{
			if(!consoleplayer)
				continue;

			// check for game start
			if(netbuffer->retransmitfrom)
				ns->force_update = 4;

			// parse key packet
			if(netbuffer->net_key.check == ns->key_check[0])
				continue;
			ns->key_check[0] = netbuffer->net_key.check;

			// game info
			ns->menu_sel[MENU_GAME_MAP] = netbuffer->net_key.map_idx;
			ns->menu_sel[MENU_GAME_SKILL] = netbuffer->net_key.game_info & 7;
			ns->menu_sel[MENU_GAME_MODE] = (netbuffer->net_key.game_info >> 3) & 3;
			ns->menu_sel[MENU_INVENTORY] = (netbuffer->net_key.game_info >> 5) & 3;
			ns->menu_sel[MENU_ITEM_TIME] = netbuffer->net_key.respawn;
			ns->start_check = netbuffer->net_key.menu_idx;
			ns->menu_flags = netbuffer->net_key.flags;
			ns->prng_idx = netbuffer->net_key.prng_idx;

			// player info
			for(uint32_t idx = 0; idx < doomcom->numplayers; idx++)
			{
				player_info[idx] = netbuffer->net_key.pi[idx];
				if(player_info[idx].playerclass < num_player_classes)
				{
					ns->player_state[idx] = COLOR_PSTATE_READY;
					ns->player_info[idx] = COLOR_PINFO_CLASS;
				} else
				if(player_info[idx].playerclass == UNKNOWN_PLAYER_CLASS)
				{
					ns->player_state[idx] = COLOR_PSTATE_READY;
					ns->player_info[idx] = COLOR_PINFO_INITIAL;
				} else
				{
					ns->player_state[idx] = COLOR_PSTATE_ERROR;
					ns->player_info[idx] = COLOR_PINFO_ERROR;
				}
				if(idx == consoleplayer)
					ns->player_state[idx] = COLOR_PSTATE_ITSME;
			}
		} else
		if(netbuffer->starttic == 'I')
		{
			uint8_t *ptr;
			uint8_t len;

			ptr = screen_buffer + SCREENWIDTH*SCREENHEIGHT;
			ptr += netbuffer->net_px.offset;

			len = 1 + (netbuffer->retransmitfrom & 63);

			if(ptr + len < screen_buffer + SCREENWIDTH*SCREENHEIGHT*2)
			{
				ns->force_update = netbuffer->retransmitfrom >> 7;
				memcpy(ptr, netbuffer->net_px.data, len);
			}
		}
	}
}

//
// new network init

static __attribute((regparm(2),no_caller_saved_registers))
void D_ArbitrateNetStart()
{
	uint32_t utick;
	net_setup_t ns;

	if(	doomcom->numplayers != doomcom->numnodes ||
		doomcom->ticdup != 1
	)
		engine_error("NET", "Unsupported network setup!");

	memset(&ns, 0, sizeof(ns));

	// modify stuff
	menuactive = 1;

	// fade the screen
	for(uint8_t *ptr = (uint8_t*)0xA0000; ptr < (uint8_t*)0xA0000 + SCREENWIDTH * SCREENHEIGHT; ptr++)
		*ptr = colormaps[*ptr + 256 * 28];
	// and store for later
	dwcopy(screen_buffer + SCREENWIDTH*SCREENHEIGHT, (void*)0xA0000, SCREENWIDTH*SCREENHEIGHT / sizeof(uint32_t));

	if(!M_CheckParm("-nonetcheck"))
	{
		// checksum
		ns.mod_csum = dec_mod_csum;
		// also check all lumps, order dependent
		for(uint32_t i = 0; i < numlumps; i++)
		{
			ns.mod_csum = (ns.mod_csum << 16) | (ns.mod_csum >> 16);
			ns.mod_csum += lumpinfo[i].same[0];
			ns.mod_csum += lumpinfo[i].same[1];
		}
		ns.version = ace_git_version;
	} else
	{
		// skip the check
		// both sides must agree on this
		ns.version = 0;
		ns.mod_csum = 0;
	}

	doom_printf("[NET] v%08X c%08X\n", ns.version, ns.mod_csum);

	//
	if(startskill > sk_nightmare)
		startskill = sk_nightmare;
	if(deathmatch > 2)
		deathmatch = 2;

	// defaults
	ns.menu_now = MENU_PLAYER_CLASS;
	ns.menu_yh = mod_config.menu_font_height > 12 ? 12 : mod_config.menu_font_height;
	ns.menu_y0 = ns.menu_yh * 4 + ns.menu_yh / 2;
	ns.menu_yf = SCREENHEIGHT - ns.menu_yh * (((FLAG_COUNT-1) / 3) + 1);
	ns.color_title[0] = &render_tables->fmap[FONT_COLOR_COUNT * FCOL_GOLD];
	ns.color_title[1] = &render_tables->fmap[FONT_COLOR_COUNT * FCOL_DARKBROWN];
	ns.color_menu[0] = &render_tables->fmap[FONT_COLOR_COUNT * FCOL_DARKGRAY];
	ns.color_menu[1] = &render_tables->fmap[FONT_COLOR_COUNT * FCOL_DARKGREEN];
	ns.color_menu[2] = &render_tables->fmap[FONT_COLOR_COUNT * FCOL_GRAY];
	ns.color_menu[3] = &render_tables->fmap[FONT_COLOR_COUNT * FCOL_GREEN];
	ns.menu_sel[MENU_PLAYER_CLASS] = 255;
	ns.menu_sel[MENU_GAME_SKILL] = startskill;
	ns.menu_sel[MENU_GAME_MODE] = deathmatch ? deathmatch + 1 : 0;
	ns.menu_sel[MENU_ITEM_TIME] = deathmatch > 1 ? 30 : 0;

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		ns.key_check[i] = 0;
		ns.node_check[i] = 0;
		ns.player_state[i] = COLOR_PLAYER_DISABLED;
		ns.player_info[i] = COLOR_PLAYER_DISABLED;
		player_info[i].playerclass = UNKNOWN_PLAYER_CLASS;
	}

	for(uint32_t i = 0; i < doomcom->numplayers; i++)
	{
		ns.player_state[i] = COLOR_PSTATE_INITIAL;
		ns.player_info[i] = COLOR_PINFO_INITIAL;
	}

	if(consoleplayer)
	{
		ns.menu_flags = 0;
		ns.start_check = 0xFF;
		ns.node_check[0] = 1;
		player_info[consoleplayer] = player_info[0];

		// get string positions
		for(uint32_t i = 0; i < FLAG_COUNT; i++)
		{
			uint32_t tmp = i % 3;

			if(tmp)
				ns.flag_x[i] = font_draw_text(0, 0, menu_flags[i], smallfont);

			if(i == FLAG_COUNT-1) // hack
				tmp++;

			switch(tmp)
			{
				case 0:
					ns.flag_x[i] = 1;
				break;
				case 1:
					ns.flag_x[i] = (SCREENWIDTH - ns.flag_x[i]) / 2;
				break;
				case 2:
					ns.flag_x[i] = SCREENWIDTH - ns.flag_x[i] - 1;
				break;
			}
		}
	} else
	{
		ns.menu_flags = 3 | (!!nomonsters << 2) | (!!fastparm << 3) | (!!respawnparm << 4);
		ns.key_check[0] = 1;
		player_info[0].playerclass = 0;
		ns.player_info[0] = COLOR_PINFO_CLASS;
	}

	ns.player_state[consoleplayer] = COLOR_PSTATE_ITSME;
	ns.force_update = 1;

	// classes
	net_menu[MENU_PLAYER_CLASS].maxsel = num_player_classes;

	// maps
	for(uint32_t i = 0; i < num_maps; i++)
	{
		if(map_info[i].lump >= 0)
			net_menu[MENU_GAME_MAP].maxsel++;
	}

	// handle initialization
	utick = 64;
	while(1)
	{
		uint32_t update = ns.force_update;

		ns.force_update = 0;

		for( ; eventtail != eventhead; eventtail = (++eventtail)&(MAXEVENTS-1))
		{
			event_t *ev = &events[eventtail];
			if(ev->type == ev_keydown)
			{
				switch(ev->data1)
				{
					case 0xAD: // UP
						if(ns.menu_sel[ns.menu_now] >= net_menu[ns.menu_now].maxsel)
						{
							ns.menu_sel[ns.menu_now] = 0;
							update |= 3;
						} else
						if(ns.menu_sel[ns.menu_now] > 0)
						{
							ns.menu_sel[ns.menu_now]--;
							update |= 3;
						}
					break;
					case 0xAF: // DOWN
						if(ns.menu_sel[ns.menu_now] >= net_menu[ns.menu_now].maxsel)
						{
							ns.menu_sel[ns.menu_now] = net_menu[ns.menu_now].maxsel-1;
							update |= 3;
						} else
						if(ns.menu_sel[ns.menu_now] < net_menu[ns.menu_now].maxsel-1)
						{
							ns.menu_sel[ns.menu_now]++;
							update |= 3;
						}
					break;
					case 0xAC: // LEFT
						if(consoleplayer)
							break;
						ns.menu_now--;
						if(ns.menu_now < 0)
							ns.menu_now = NUM_MENUS-1;
						update |= 3;
					break;
					case 0xAE: // RIGHT
						if(consoleplayer)
							break;
						ns.menu_now++;
						if(ns.menu_now >= NUM_MENUS)
							ns.menu_now = 0;
						update |= 3;
					break;
					case ' ': // SPACE
						if(	consoleplayer ||
							ns.menu_now == MENU_GAME_MAP
						){
							ns.map_string = !ns.map_string;
							update |= 1;
							break;
						}
						if(net_menu[ns.menu_now].type != TYPE_FLAGS)
							break;
						ns.menu_flags ^= 1 << ns.menu_sel[ns.menu_now];
						update |= 3;
					break;
					case 0x0D: // ENTER
						if(consoleplayer)
							break;
						// start the game
						update |= 4;
						// check players (including self)
						for(uint32_t i = 0; i < doomcom->numplayers; i++)
						{
							if(player_info[i].playerclass >= num_player_classes)
							{
								// can't start
								update &= ~4;
								break;
							}
						}
					break;
					case 0x1B: // ESC
						// let other players now
						if(consoleplayer)
						{
							// set status
							ns.menu_sel[MENU_PLAYER_CLASS] = ERROR_DISCONNECT;
							// increment check counter, avoid zero
							ns.node_check[0]++;
							if(!ns.node_check[0])
								ns.node_check[0]++;
						} else
						{
							// set status
							player_info[0].playerclass = ERROR_DISCONNECT;
							// increment check counter, avoid zero
							ns.key_check[0]++;
							if(!ns.key_check[0])
								ns.key_check[0]++;
						}
						// send packets
						for(uint32_t i = 0; i < 16; i++)
							pkt_send(&ns, 0);
						// abort network game
						engine_error("NET", "Network game setup aborted!");
					break;
				}
			}
		}

		if(update >= 4)
			break;

		if(consoleplayer)
		{
			if(update & 2)
			{
				// increment check counter, avoid zero
				ns.node_check[0]++;
				if(!ns.node_check[0])
					ns.node_check[0]++;
			}
			if(ns.key_check[0] != ns.key_check[1])
			{
				// we have received new info; update screen, send response
				ns.key_check[1] = ns.key_check[0];
				update = 3;
			}
			if(utick > 30)
				// keep sending info packet once in a while
				update |= 2;
		} else
		{
			// force playerclass
			player_info[0].playerclass = ns.menu_sel[MENU_PLAYER_CLASS];

			// check for update
			if(update & 2)
			{
				// increment check counter, avoid zero
				ns.key_check[0]++;
				if(!ns.key_check[0])
					ns.key_check[0]++;
				// change all players color status
				for(uint32_t i = 1; i < doomcom->numplayers; i++)
					if(ns.player_state[i] == COLOR_PSTATE_READY)
						ns.player_state[i] = COLOR_PSTATE_WAITING;
				// reset repeat ticker
				utick = 0;
			} else
			if(utick > 15)
			{
				// scan nodes
				for(uint32_t i = 1; i < doomcom->numplayers; i++)
				{
					if(ns.key_check[i] != ns.key_check[0])
					{
						// there is a player that has old info
						update |= 2;
						break;
					}
				}
			}

			// add some entropy
			ns.prng_idx++;
			if(ns.prng_idx >= rng_max)
				ns.prng_idx = 0;
		}

		// screen update
		if(update & 1)
			update_screen(&ns, 0);

		// network update
		if(update & 2)
		{
			pkt_send(&ns, 0);
			utick = 0;
		}

		update = 0;

		pkt_recv(&ns, 0);
		if(ns.force_update >= 4)
			break;

		I_WaitVBL(1);
		utick++;
	}

	if(consoleplayer)
	{
		update_screen(&ns, 2);
		I_WaitVBL(1);
	} else
	{
		// change all players color status
		for(uint32_t i = 1; i < doomcom->numplayers; i++)
			ns.player_state[i] = COLOR_PSTATE_WAITING;
		update_screen(&ns, 1);
		I_WaitVBL(1);

		// increment check counter, avoid zero
		ns.key_check[0]++;
		if(!ns.key_check[0])
			ns.key_check[0]++;

		// tell everyone it's time to start
		ns.start_check = 1;
		ns.force_update = 0;
		utick = 64;

		while(ns.start_check != (1 << doomcom->numplayers) - 1)
		{
			for( ; eventtail != eventhead; eventtail = (++eventtail)&(MAXEVENTS-1))
			{
				event_t *ev = &events[eventtail];
				if(ev->type == ev_keydown && ev->data1 == 0x1B)
					engine_error("NET", "Network game setup aborted!");
			}

			if(utick > 10)
			{
				pkt_send(&ns, 1);
				utick = 0;
			}

			pkt_recv(&ns, 1);

			if(ns.force_update & 1)
				update_screen(&ns, 1);

			I_WaitVBL(1);
			utick++;
		}
	}

	// apply settings

	startskill = ns.menu_sel[MENU_GAME_SKILL];

	deathmatch = ns.menu_sel[MENU_GAME_MODE];
	survival = deathmatch == 1;
	if(deathmatch > 0)
		deathmatch--;

	cheat_disable = !(ns.menu_flags & 1);
	no_friendly_fire = !((ns.menu_flags >> 1) & 1);
	nomonsters = (ns.menu_flags >> 2) & 1;
	fastparm = (ns.menu_flags >> 3) & 1;
	respawnparm = (ns.menu_flags >> 4) & 1;

	net_inventory = ns.menu_sel[MENU_INVENTORY];

	prndindex = ns.prng_idx;
	if(prndindex >= rng_max)
		prndindex = 0;

	states[STATE_SPECIAL_HIDE].tics = ns.menu_sel[MENU_ITEM_TIME] * 35;

	// map
	{
		map_level_t *info = map_info;
		for(uint32_t i = 0; i < ns.menu_sel[MENU_GAME_MAP]; )
		{
			if(info->lump >= 0)
				i++;
			info++;
		}
		memcpy(map_lump.name, lumpinfo[info->lump].name, 8);
	}

	// checks
	if(deathmatch)
		no_friendly_fire = 0;

	if(survival && net_inventory == 1)
		net_inventory = 0;

	// restore stuff
	menuactive = 0;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'D_ArbitrateNetStart' in 'I_InitNetwork'
	{0x0001F685, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)D_ArbitrateNetStart},
	// remove call to 'D_CheckNetGame' in 'D_DoomMain'
	{0x0001E865, CODE_HOOK | HOOK_SET_NOPS, 5},
};

