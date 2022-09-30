// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "decorate.h"
#include "mobj.h"
#include "inventory.h"
#include "animate.h"
#include "think.h"
#include "player.h"
#include "hitscan.h"
#include "config.h"
#include "demo.h"
#include "map.h"
#include "ldr_flat.h"
#include "ldr_texture.h"

#include "map_info.h"

#define WI_TITLEY	2
#define	FB	0

mapthing_t *playerstarts;
mapthing_t *deathmatchstarts;
mapthing_t **deathmatch_p;

uint32_t *nomonsters;
uint32_t *fastparm;
uint32_t *respawnparm;

uint32_t *secretexit;
uint32_t *deathmatch;
uint32_t *gamemap;
uint32_t *gameepisode;
uint32_t *gameskill;
uint32_t *leveltime;

uint32_t *totalkills;
uint32_t *totalitems;
uint32_t *totalsecret;

uint32_t *skytexture;
uint32_t *skyflatnum;

uint32_t *numsides;
uint32_t *numlines;
uint32_t *numsectors;
line_t **lines;
vertex_t **vertexes;
side_t **sides;
sector_t **sectors;

uint32_t *prndindex;

uint32_t *viewactive;
uint32_t *automapactive;

static uint32_t *d_skill;
static uint32_t *d_map;
static uint32_t *d_episode;

plat_t **activeplats;
ceiling_t **activeceilings;

uint32_t *netgame;
uint32_t *usergame;

uint32_t *nofit;
uint32_t *crushchange;

fixed_t *tmdropoffz;
fixed_t *openrange;
fixed_t *opentop;
fixed_t *openbottom;

line_t **ceilingline;

mobj_t **linetarget;

fixed_t *bmaporgx;
fixed_t *bmaporgy;

static wbstartstruct_t *wminfo;
static int32_t *finaleflat; // type is changed!
static uint8_t **finaletext;
static uint32_t *finalecount;
static uint32_t *finalestage;

map_lump_name_t map_lump;
int32_t map_lump_idx;
map_level_t *map_level_info;
map_level_t *map_next_info;

uint_fast8_t map_skip_stuff;
uint_fast8_t is_title_map;

uint32_t num_maps;
map_level_t *map_info;

uint32_t num_clusters;
map_cluster_t *map_cluster;

static int32_t cluster_music;
static int32_t patch_finshed;
static int32_t patch_entering;

static uint32_t intermission_mode;

//
static uint8_t **mapnames;
static uint8_t **mapnames2;

// unnamed map
static map_level_t map_info_unnamed =
{
	.name = "Unnamed",
	.next_normal = MAP_END_DOOM2,
	.next_secret = MAP_END_DOOM2,
	.title_patch = -1,
	.music_lump = -1,
	.par_time = 35,
};

//

static const uint8_t skillbits[] = {1, 1, 2, 4, 4};

//
static const hook_t patch_new[];
static const hook_t patch_old[];

//
// callbacks

static uint32_t cb_free_inventory(mobj_t *mo)
{
	inventory_clear(mo);
	return 0;
}

//
// line scroller

__attribute((regparm(2),no_caller_saved_registers))
void think_line_scroll(line_scroll_t *ls)
{
	side_t *side = *sides + ls->line->sidenum[0];
	side->textureoffset += (fixed_t)ls->x * FRACUNIT;
	side->rowoffset += (fixed_t)ls->y * FRACUNIT;
}

static inline void spawn_line_scroll()
{
	for(uint32_t i = 0; i < *numlines; i++)
	{
		line_t *ln = *lines + i;

		if(ln->special == 48)
		{
			line_scroll_t *ls;

			ls = Z_Malloc(sizeof(line_scroll_t), PU_LEVEL, NULL);
			ls->line = ln;
			ls->x = 1;
			ls->y = 0;
			ls->thinker.function = think_line_scroll;
			think_add(&ls->thinker);
		}
	}
}

//
// map info

static map_level_t *map_get_info(int32_t lump)
{
	for(uint32_t i = 0; i < num_maps; i++)
	{
		map_level_t *info = map_info + i;
		if(info->lump == lump)
			return info;
	}
	return &map_info_unnamed;
}

static map_cluster_t *map_find_cluster(uint32_t num)
{
	if(!num)
		return NULL;

	for(uint32_t i = 0; i < num_clusters; i++)
	{
		map_cluster_t *cl = map_cluster + i;
		if(cl->idx == num)
			return cl;
	}

	return NULL;
}

//
// map loading

__attribute((regparm(2),no_caller_saved_registers))
void map_load_setup()
{
	uint32_t cache;

	if(*paused) 
	{ 
		*paused = 0;
		S_ResumeSound();
	}

	if(*gameskill > sk_nightmare)
		*gameskill = sk_nightmare;

	*viewactive = 1;
	*automapactive = 0;

	if(*gameepisode)
	{
		is_title_map = 0;
		cache = !*demoplayback;
	} else
	{
		// titlemap can be visited by other means
		// so this has to be different
		map_lump.wame = 0x50414D454C544954; // TITLEMAP
		is_title_map = 1;
		cache = 0;
	}

	// find map lump
	map_lump_idx = W_CheckNumForName(map_lump.name);

	if(map_lump_idx < 0)
		goto map_load_error;
	// TODO: check map validity

	// setup level info
	map_level_info = map_get_info(map_lump_idx);

	// hack for original or new levels
	if(map_level_info->levelhack)
	{
		if(map_level_info->levelhack & 0x80)
		{
			*gameepisode = 1;
			*gamemap = map_level_info->levelhack & 0x7F;
		} else
		{
			*gameepisode = map_level_info->levelhack >> 4;
			*gamemap = map_level_info->levelhack & 15;
		}
	} else
	{
		*gameepisode = 1;
		*gamemap = 1;
	}
doom_printf("EPI %u MAP %u\n", *gameepisode, *gamemap);
	// sky
	*skytexture = map_level_info->texture_sky[0];

	// free old inventories
	mobj_for_each(cb_free_inventory);

	// reset netID
	mobj_netid = 1; // 0 is NULL, so start with 1

	think_clear();

	// apply game patches
	if(*demoplayback == DEMO_OLD)
		utils_install_hooks(patch_old, 0);
	else
		utils_install_hooks(patch_new, 0);

	P_SetupLevel();

	// reset some stuff
	for(uint32_t i = 0; i < MAXPLATS; i++)
		activeplats[i] = NULL;

	for(uint32_t i = 0; i < MAXCEILINGS; i++)
		activeceilings[i] = NULL;

	clear_buttons();

	if(cache)
		R_PrecacheLevel();

	// specials
	if(!map_skip_stuff)
	{
		P_SpawnSpecials();
		spawn_line_scroll();
	}

	// in the level
	*gamestate = GS_LEVEL;
	*usergame = !is_title_map && !*demoplayback;
	return;

map_load_error:
	if(is_title_map)
	{
		// actually start title
		*gameaction = ga_nothing;
		*demosequence = -1;
		*advancedemo = 1;
	} else
		map_start_title();
	*wipegamestate = *gamestate;
	M_StartMessage("This map does not exist!", NULL, 0);
	return;
}

void map_setup_old(uint32_t skill, uint32_t episode, uint32_t level)
{
	for(uint32_t i = 0; i < MAXPLAYERS; i++)
		players[i].playerstate = PST_REBORN;

	if(!episode || episode > 9)
		episode = 1;

	if(level > 99)
		level = 1;

	if(*gamemode)
	{
		episode = 1;
		doom_sprintf(map_lump.name, "MAP%02u", level);
	} else
		doom_sprintf(map_lump.name, "E%uM%u", episode, level);

	*gameskill = skill;
	*gameepisode = episode;
	*wipegamestate = -1;

	map_load_setup();
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
static void spawn_map_thing(mapthing_t *mt)
{
	uint32_t idx;
	mobj_t *mo;
	mobjinfo_t *info;
	fixed_t x, y, z;

	// deathmatch starts
	if(mt->type == 11)
	{
		if(*deathmatch_p < deathmatchstarts + 10)
		{
			**deathmatch_p = *mt;
			*deathmatch_p = *deathmatch_p + 1;
		}
		return;
	}

	// player starts
	if(mt->type && mt->type <= 4)
	{
		playerstarts[mt->type - 1] = *mt;
		if(!*deathmatch && !map_skip_stuff)
			spawn_player(mt);
		return;
	}

	if(map_skip_stuff)
		return;

	// check network game
	if(!*netgame && mt->options & 16)
		return;

	// check skill level
	if(!(mt->options & skillbits[*gameskill]))
		return;

	// backward search for type
	idx = num_mobj_types;
	do
	{
		idx--;
		if(mobjinfo[idx].doomednum == mt->type)
			break;
	} while(idx);
	if(!idx)
		idx = MOBJ_IDX_UNKNOWN;
	info = mobjinfo + idx;

	// 'not in deathmatch'
	if(*deathmatch && info->flags & MF_NOTDMATCH)
		return;

	// '-nomonsters'
	if(*nomonsters && info->flags1 & MF1_ISMONSTER)
		return;

	// position
	x = (fixed_t)mt->x << FRACBITS;
	y = (fixed_t)mt->y << FRACBITS;
	z = info->flags & MF_SPAWNCEILING ? 0x7FFFFFFF : 0x80000000;

	// spawn
	mo = P_SpawnMobj(x, y, z, idx);
	mo->spawnpoint = *mt;
	mo->angle = ANG45 * (mt->angle / 45);

	if(mo->tics > 0)
		mo->tics = 1 + (P_Random() % mo->tics);

	if(mt->options & 8)
		mo->flags |= MF_AMBUSH;

	if(mo->flags & MF_COUNTKILL)
		*totalkills = *totalkills + 1;

	if(mo->flags & MF_COUNTITEM)
		*totalitems = *totalitems + 1;
}

__attribute((regparm(2),no_caller_saved_registers))
static uint32_t check_door_key(line_t *line, mobj_t *mo)
{
	player_t *pl = mo->player;
	uint16_t k0, k1;
	uint8_t *text;

	switch(line->special)
	{
		case 26:
		case 32:
			if(!pl)
				return 0;
			k0 = 47;
			k1 = 52;
			text = (uint8_t*)0x00022C90 + doom_data_segment;
		break;
		case 27:
		case 34:
			if(!pl)
				return 0;
			k0 = 49;
			k1 = 50;
			text = (uint8_t*)0x00022CB8 + doom_data_segment;
		break;
		case 28:
		case 33:
			if(!pl)
				return 0;
			k0 = 48;
			k1 = 51;
			text = (uint8_t*)0x00022CE0 + doom_data_segment;
		break;
		default:
			return 1;
	}

	if(inventory_check(mo, k0))
		return 1;

	if(inventory_check(mo, k1))
		return 1;

	pl->message = text;
	S_StartSound(mo, mo->info->player.sound.usefail);

	return 0;
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t check_obj_key(line_t *line, mobj_t *mo)
{
	player_t *pl = mo->player;
	uint16_t k0, k1;
	uint8_t *text;

	switch(line->special)
	{
		case 99:
		case 133:
			if(!pl)
				return 0;
			k0 = 47;
			k1 = 52;
			text = (uint8_t*)0x00022C08 + doom_data_segment;
		break;
		case 134:
		case 135:
			if(!pl)
				return 0;
			k0 = 48;
			k1 = 51;
			text = (uint8_t*)0x00022C34 + doom_data_segment;
		break;
		case 136:
		case 137:
			if(!pl)
				return 0;
			k0 = 49;
			k1 = 50;
			text = (uint8_t*)0x00022C60 + doom_data_segment;
		break;
		default:
			goto do_door;
	}

	if(inventory_check(mo, k0))
		goto do_door;

	if(inventory_check(mo, k1))
		goto do_door;

	pl->message = text;
	S_StartSound(mo, mo->info->player.sound.usefail);

	return 0;

do_door:
	return EV_DoDoor(line, 6); // only 'blazeOpen' is ever used here
}

//
// TITLEMAP

void map_start_title()
{
	int32_t lump;

	*gameaction = ga_nothing;

	lump = W_CheckNumForName("TITLEMAP");
	if(lump < 0)
	{
		*demosequence = -1;
		*advancedemo = 1;
		return;
	}

	if(*gameepisode)
		*wipegamestate = -1;
	else
		*wipegamestate = GS_LEVEL;

	*gameskill = sk_medium;
	*fastparm = 0;
	*respawnparm = 0;
	*nomonsters = 0;
	*deathmatch = 0;
	*gameepisode = 0;
	*prndindex = 0;
	*netgame = 0;
	*netdemo = 0;

	*consoleplayer = 0;
	memset(players, 0, sizeof(player_t) * MAXPLAYERS);
	players[0].playerstate = PST_REBORN;

	memset(playeringame, 0, sizeof(uint32_t) * MAXPLAYERS);
	playeringame[0] = 1;

	map_load_setup();
}

//
// default map setup

static void setup_episode(uint32_t start, uint32_t episode, uint32_t secret, uint32_t victory, uint8_t **names)
{
	uint8_t text[12];

	for(uint32_t i = 0; i < 9; i++)
	{
		map_level_t *info = map_info + start + i;

		info->name = names[i];
		info->cluster = episode;
		info->levelnum = (episode - 1) * 10 + i + 1;
		info->levelhack = (episode << 4) | (i + 1);

		doom_sprintf(text, "E%uM%u", episode, i + 1);
		info->lump = wad_check_lump(text);

		doom_sprintf(text, "WILV%02u", info->levelnum - 1);
		info->title_patch = wad_check_lump(text);

		doom_sprintf(text, "SKY%u", episode);
		info->texture_sky[0] = texture_num_get(text);
		info->texture_sky[1] = info->texture_sky[0];

		info->music_lump = -1; // TODO
		info->par_time = 123; // TODO

		if(i == 8)
		{
			info->next_normal = start + secret;
			info->next_secret = start + secret;
		} else
		if(i == 7)
		{
			info->next_normal = victory;
			info->next_secret = victory;
			info->flags = MAP_FLAG_NOINTERMISSION;
		} else
		{
			info->next_normal = start + i + 1;
			if(i == secret - 1)
				info->next_secret = start + 8;
			else
				info->next_secret = info->next_normal;
		}
	}
}

static void setup_cluster(uint32_t start, uint32_t count, uint32_t cluster, uint32_t sky)
{
	uint8_t text[12];

	for(uint32_t i = 0; i < count; i++)
	{
		map_level_t *info = map_info + start + i;

		info->name = mapnames2[start + i];
		info->cluster = cluster;
		info->levelnum = start + i + 1;
		info->levelhack = 0x80 | (i + start + 1);

		doom_sprintf(text, "MAP%02u", info->levelnum);
		info->lump = wad_check_lump(text);

		doom_sprintf(text, "CWILV%02u", info->levelnum - 1);
		info->title_patch = wad_check_lump(text);

		doom_sprintf(text, "SKY%u", sky);
		info->texture_sky[0] = texture_num_get(text);
		info->texture_sky[1] = info->texture_sky[0];

		info->music_lump = -1; // TODO
		info->par_time = 123; // TODO

		switch(start + i)
		{
			case MAPD2_MAP15:
				info->next_normal = MAPD2_MAP16;
				info->next_secret = MAPD2_MAP31;
			break;
			case MAPD2_MAP30:
				info->next_normal = MAP_END_DOOM2;
				info->next_secret = MAP_END_DOOM2;
			break;
			case MAPD2_MAP31:
				info->next_normal = MAPD2_MAP16;
				info->next_secret = MAPD2_MAP32;
			break;
			case MAPD2_MAP32:
				info->next_normal = MAPD2_MAP16;
				info->next_secret = MAPD2_MAP16;
			break;
			default:
				info->next_normal = start + i + 1;
				info->next_secret = info->next_normal;
			break;
		}
	}
}

//
// API

void init_map()
{
	uint8_t text[16];
	const def_cluster_t *def_clusters;

	doom_printf("[ACE] init MAPs\n");
	ldr_alloc_message = "Map and game info memory allocation failed!";

	// find some GFX
	patch_finshed = wad_get_lump((void*)0x000247BC + doom_data_segment);
	patch_entering = wad_get_lump((void*)0x000247C0 + doom_data_segment);

	// default clusters
	if(*gamemode)
	{
		doom_sprintf(text, "D_%s", (void*)0x00024B40 + doom_data_segment);
		num_clusters = D2_CLUSTER_COUNT;
		def_clusters = d2_clusters;
	} else
	{
		doom_sprintf(text, "D_%s", (void*)0x00024A30 + doom_data_segment);
		num_clusters = D1_CLUSTER_COUNT;
		def_clusters = d1_clusters;
	}

	// default music
	cluster_music = wad_check_lump(text);

	// allocate default clusters
	map_cluster = ldr_malloc(num_clusters * sizeof(map_cluster_t));
	memset(map_cluster, 0, num_clusters * sizeof(map_cluster_t));

	// prepare default clusters
	for(uint32_t i = 0; i < num_clusters; i++)
	{
		map_cluster_t *clst = map_cluster + i;
		const def_cluster_t *defc = def_clusters + i;

		clst->lump_music = cluster_music;
		clst->idx = defc->idx;

		if(defc->type)
			clst->text_leave = defc->text + doom_data_segment;
		else
			clst->text_enter = defc->text + doom_data_segment;

		clst->flat_num = flatlump[flat_num_get(defc->flat + doom_data_segment)];
	}

	// prepare default maps
	if(*gamemode)
		num_maps = NUM_D2_MAPS;
	else
		num_maps = NUM_D1_MAPS;

	// allocate maps
	map_info = ldr_malloc(num_maps * sizeof(map_level_t));
	memset(map_info, 0, num_maps * sizeof(map_level_t));

	if(*gamemode)
	{
		// cluster 5
		setup_cluster(MAPD2_MAP01, MAPD2_MAP07 - MAPD2_MAP01, 5, 1);
		// cluster 6
		setup_cluster(MAPD2_MAP07, MAPD2_MAP12 - MAPD2_MAP07, 6, 1);
		// cluster 7
		setup_cluster(MAPD2_MAP12, MAPD2_MAP21 - MAPD2_MAP12, 7, 2);
		// cluster 8
		setup_cluster(MAPD2_MAP21, MAPD2_MAP31 - MAPD2_MAP21, 8, 3);
		// cluster 9
		setup_cluster(MAPD2_MAP31, 1, 9, 3);
		// cluster 10
		setup_cluster(MAPD2_MAP32, 1, 10, 3);
	} else
	{
		// episode 1
		setup_episode(MAPD1_E1M1, 1, 3, MAP_END_DOOM1_EP1, mapnames + MAPD1_E1M1);
		// episode 2
		setup_episode(MAPD1_E2M1, 2, 5, MAP_END_DOOM1_EP2, mapnames + MAPD1_E2M1);
		// episode 3
		setup_episode(MAPD1_E3M1, 3, 6, MAP_END_DOOM1_EP3, mapnames + MAPD1_E3M1);
		// this is not the same as in ZDoom
		map_info_unnamed.next_normal = MAP_END_TITLE;
		map_info_unnamed.next_secret = MAP_END_TITLE;
	}

	map_info_unnamed.texture_sky[0] = texture_num_get("SKY1");
	map_info_unnamed.texture_sky[1] = map_info_unnamed.texture_sky[0];

	// default, dummy
	map_level_info = &map_info_unnamed;
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
static void projectile_sky_flat(mobj_t *mo)
{
	if(mo->subsector && !(mo->flags1 & MF1_SKYEXPLODE))
	{
		sector_t *sec = mo->subsector->sector;
		if(mo->z <= sec->floorheight)
		{
			if(sec->floorpic == *skyflatnum)
			{
				P_RemoveMobj(mo);
				return;
			}
		} else
		if(mo->z + mo->height >= sec->ceilingheight)
		{
			if(sec->ceilingpic == *skyflatnum)
			{
				P_RemoveMobj(mo);
				return;
			}
		}
	}

	explode_missile(mo);
}

__attribute((regparm(2),no_caller_saved_registers))
static void projectile_sky_wall(mobj_t *mo)
{
	if(mo->flags1 & MF1_SKYEXPLODE)
		explode_missile(mo);
	else
		P_RemoveMobj(mo);
}

__attribute((regparm(2),no_caller_saved_registers))
static void do_new_game()
{
	*gameaction = ga_nothing;

	*demoplayback = 0;
	*netdemo = 0;
	*netgame = 0;
	*deathmatch = 0;
	*respawnparm = 0;
	*fastparm = 0;
	*nomonsters = 0;
	*consoleplayer = 0;

	playeringame[0] = 1;
	for(uint32_t i = 1; i < MAXPLAYERS; i++)
		playeringame[i] = 0;

	map_setup_old(*d_skill, *d_episode, *d_map);
}

__attribute((regparm(2),no_caller_saved_registers))
static void do_autostart_game()
{
	uint32_t *startskill = (void*)0x0002A398 + doom_data_segment;
	uint32_t *startepisode = (void*)0x0002A3A4 + doom_data_segment;
	uint32_t *startmap = (void*)0x0002A39C + doom_data_segment;

	map_setup_old(*startskill, *startepisode, *startmap);
}

__attribute((regparm(2),no_caller_saved_registers))
static void set_world_done()
{
	map_cluster_t *old_cl, *new_cl;
	int32_t music_lump = -1;
	uint16_t next;

	*gameaction = ga_worlddone;

	// mark secret level visited
	if(*secretexit)
	{
		for(uint32_t i = 0; i < MAXPLAYERS; i++)
			players[i].didsecret = 1;
		next = map_level_info->next_secret;
	} else
		next = map_level_info->next_normal;

	// check for cluser change
	old_cl = map_find_cluster(map_level_info->cluster);
	if(map_next_info)
		new_cl = map_find_cluster(map_next_info->cluster);
	else
		new_cl = NULL;

	*finaletext = NULL;

	if(old_cl != new_cl)
	{
		if(new_cl && new_cl->text_enter)
		{
			*finaletext = new_cl->text_enter;
			*finaleflat = new_cl->flat_num;
			music_lump = new_cl->lump_music;
		} else
		{
			*finaletext = old_cl->text_leave;
			*finaleflat = old_cl->flat_num;
			music_lump = old_cl->lump_music;
		}
	}

	// finale?
	if(*finaletext || next > MAP_END_TITLE)
	{
		*gameaction = ga_nothing;
		*gamestate = GS_FINALE;
		*viewactive = 0;
		*automapactive = 0;
		*finalecount = 0;

		if(!*finaletext)
			*finalestage = 1;
		else
			*finalestage = 0;

		switch(next)
		{
			case MAP_END_DOOM1_EP1:
				intermission_mode = 0;
				*gameepisode = 1;
			break;
			case MAP_END_DOOM1_EP2:
				intermission_mode = 0;
				*gameepisode = 2;
			break;
			case MAP_END_DOOM1_EP3:
				intermission_mode = 0;
				*gameepisode = 3;
				// TODO: music_lump = mus_bunny ?
			break;
			case MAP_END_DOOM2:
				intermission_mode = 1;
				*gamemap = 30;
				if(!*finaletext)
				{
					F_StartCast();
					// TODO: music_lump = mus_evil ?
				}
			break;
		}

		// TODO: S_ChangeMusic
	}
}

__attribute((regparm(2),no_caller_saved_registers))
static void do_completed()
{
	uint16_t next;

	*gameaction = ga_nothing;

	// leave automap
	if(*automapactive)
		AM_Stop();

	// clean-up players
	for(uint32_t i = 0; i < MAXPLAYERS; i++)
		player_finish(players + i);

	// check next level
	if(*secretexit)
		next = map_level_info->next_secret;
	else
		next = map_level_info->next_normal;

	if(next < num_maps)
		map_next_info = map_info + next;
	else
		map_next_info = NULL;

	// intermission
	*gamestate = GS_INTERMISSION;
	*viewactive = 0;

	// endgame
	// *gamemap = 30; // to enable 'cast' finale
	// D1 endings: F_Ticker

	if(map_level_info->flags & MAP_FLAG_NOINTERMISSION)
	{
		set_world_done();
		return;
	}

	// setup intermission
	wminfo->didsecret = players[*consoleplayer].didsecret;
	wminfo->epsd = *gameepisode - 1;
	wminfo->last = *gamemap - 1;
	wminfo->next = *gamemap;
	wminfo->maxkills = *totalkills;
	wminfo->maxitems = *totalitems;
	wminfo->maxsecret = *totalsecret;
	wminfo->maxfrags = 0;
	wminfo->partime = map_level_info->par_time;
	wminfo->pnum = *consoleplayer;

	if(map_next_info && map_next_info->levelhack && map_next_info->levelhack < 0x80)
	{
		intermission_mode = 0;
		wminfo->next = (map_next_info->levelhack & 15) - 1;
	} else
	{
		intermission_mode = 1;
		if(!map_next_info)
			// use of 30 disables 'entering' text
			wminfo->next = 30;
	}

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		wminfo->plyr[i].in = playeringame[i];
		wminfo->plyr[i].skills = players[i].killcount;
		wminfo->plyr[i].sitems = players[i].itemcount;
		wminfo->plyr[i].ssecret = players[i].secretcount;
		wminfo->plyr[i].stime = *leveltime;
		// TODO: frags - these are no longer in player structure
	}

	WI_Start(wminfo);
}

__attribute((regparm(2),no_caller_saved_registers))
static void do_world_done()
{
	if(!map_next_info)
	{
		// there's no next level
		map_start_title();
		return;
	}

	*gameaction = ga_nothing;
	map_lump.wame = (*lumpinfo)[map_next_info->lump].wame;
	map_load_setup();
}

__attribute((regparm(2),no_caller_saved_registers))
static void music_intermission()
{
	doom_printf("TODO: intermission music\n");
}

__attribute((regparm(2),no_caller_saved_registers))
static void draw_finished()
{
	int32_t y = WI_TITLEY;
	patch_t *patch;

	if(map_level_info->title_patch >= 0)
	{
		patch = W_CacheLumpNum(map_level_info->title_patch, PU_CACHE);
		V_DrawPatch((SCREENWIDTH - patch->width) / 2, y, FB, patch);

		y += (5 * patch->height) / 4;
	}

	patch = W_CacheLumpNum(patch_finshed, PU_CACHE);
	V_DrawPatch((SCREENWIDTH - patch->width) / 2, y, FB, patch);
}

__attribute((regparm(2),no_caller_saved_registers))
static void draw_entering()
{
	int32_t y = WI_TITLEY;
	patch_t *patch;

	patch = W_CacheLumpNum(patch_entering, PU_CACHE);
	V_DrawPatch((SCREENWIDTH - patch->width) / 2, y, FB, patch);

	if(map_next_info && map_next_info->title_patch)
	{
		y += (5 * patch->height) / 4;

		patch = W_CacheLumpNum(map_next_info->title_patch, PU_CACHE);
		V_DrawPatch((SCREENWIDTH - patch->width) / 2, y, FB, patch);
	}
}

//
// hooks

static const hook_t patch_new[] =
{
	// fix 'A_Tracer' - make it leveltime based
	{0x00027E2A, CODE_HOOK | HOOK_ABSADDR_DATA, 0x0002CF80},
	// fix typo in 'P_DivlineSide'
	{0x0002EA00, CODE_HOOK | HOOK_UINT16, 0xCA39},
	// projectile sky explosion
	{0x0003137E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)projectile_sky_flat},
	{0x00031089, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)projectile_sky_wall},
	// replace 'P_PathTraverse' on multiple places
	{0x0002B5F6, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_SlideMove
	{0x0002B616, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_SlideMove
	{0x0002B636, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_SlideMove
	{0x0002BBFA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_AimLineAttack
	{0x0002BC89, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_LineAttack
	{0x0002BD55, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_UseLines
	// replace pointers to 'PTR_SlideTraverse' in 'P_SlideMove'
	{0x0002B5DD, CODE_HOOK | HOOK_UINT32, (uint32_t)hs_slide_traverse},
	{0x0002B5FC, CODE_HOOK | HOOK_UINT32, (uint32_t)hs_slide_traverse},
	{0x0002B61C, CODE_HOOK | HOOK_UINT32, (uint32_t)hs_slide_traverse},
	// enable sliding on things
	{0x0002B5EA, CODE_HOOK | HOOK_UINT8, PT_ADDLINES | PT_ADDTHINGS},
	{0x0002B60F, CODE_HOOK | HOOK_UINT8, PT_ADDLINES | PT_ADDTHINGS},
	{0x0002B62C, CODE_HOOK | HOOK_UINT8, PT_ADDLINES | PT_ADDTHINGS},
	// terminator
	{0}
};

static const hook_t patch_old[] =
{
	// restore 'A_Tracer'
	{0x00027E2A, CODE_HOOK | HOOK_ABSADDR_DATA, 0x0002B3BC},
	// restore typo in 'P_DivlineSide'
	{0x0002EA00, CODE_HOOK | HOOK_UINT16, 0xC839},
	// projectile sky explosion
	{0x0003137E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)explode_missile},
	{0x00031089, CODE_HOOK | HOOK_CALL_DOOM, 0x00031660},
	// restore 'P_PathTraverse' on multiple places
	{0x0002B5F6, CODE_HOOK | HOOK_CALL_DOOM, 0x0002C8A0}, // P_SlideMove
	{0x0002B616, CODE_HOOK | HOOK_CALL_DOOM, 0x0002C8A0}, // P_SlideMove
	{0x0002B636, CODE_HOOK | HOOK_CALL_DOOM, 0x0002C8A0}, // P_SlideMove
	{0x0002BBFA, CODE_HOOK | HOOK_CALL_DOOM, 0x0002C8A0}, // P_AimLineAttack
	{0x0002BC89, CODE_HOOK | HOOK_CALL_DOOM, 0x0002C8A0}, // P_LineAttack
	{0x0002BD55, CODE_HOOK | HOOK_CALL_DOOM, 0x0002C8A0}, // P_UseLines
	// restore pointers to 'PTR_SlideTraverse' in 'P_SlideMove'
	{0x0002B5DD, CODE_HOOK | HOOK_ABSADDR_CODE, 0x0002B4B0},
	{0x0002B5FC, CODE_HOOK | HOOK_ABSADDR_CODE, 0x0002B4B0},
	{0x0002B61C, CODE_HOOK | HOOK_ABSADDR_CODE, 0x0002B4B0},
	// disable sliding on things
	{0x0002B5EA, CODE_HOOK | HOOK_UINT8, PT_ADDLINES},
	{0x0002B60F, CODE_HOOK | HOOK_UINT8, PT_ADDLINES},
	{0x0002B62C, CODE_HOOK | HOOK_UINT8, PT_ADDLINES},
	// terminator
	{0}
};

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace 'D_StartTitle'
	{0x00022A4E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)map_start_title},
	{0x0001EB1F, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)map_start_title},
	// replace 'demoplayback' check for 'usergame' in 'G_Responder'
	{0x000201CE, CODE_HOOK | HOOK_ABSADDR_DATA, 0x0002B40C},
	{0x000201D3, CODE_HOOK | HOOK_UINT8, 0x74},
	// replace 'demoplayback' check for 'usergame' in 'P_Ticker'
	{0x00032F83, CODE_HOOK | HOOK_ABSADDR_DATA, 0x0002B40C},
	{0x00032F88, CODE_HOOK | HOOK_UINT8, 0x74},
	// replace call to 'P_SetupLevel' in 'G_DoLoadLevel'
	{0x000200AA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)map_load_setup},
	// replace call to 'P_SpawnMapThing' in 'P_LoadThings'
	{0x0002E1F9, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)spawn_map_thing},
	// disable 'commercial' check in 'P_LoadThings'
	{0x0002E1B4, CODE_HOOK | HOOK_UINT16, 0x41EB},
	// disable call to 'P_SpawnSpecials' and 'R_PrecacheLevel' in 'P_SetupLevel'
	{0x0002E981, CODE_HOOK | HOOK_UINT16, 0x11EB},
	// disable line scroller and stuff cleanup in 'P_SpawnSpecials'
	{0x00030155, CODE_HOOK | HOOK_JMP_DOOM, 0x000301E1},
	// replace key checks in 'EV_VerticalDoor'
	{0x00026C85, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)check_door_key},
	{0x00026C8A, CODE_HOOK | HOOK_UINT16, 0xC085},
	{0x00026C8D, CODE_HOOK | HOOK_JMP_DOOM, 0x00026E97},
	{0x00026C8C, CODE_HOOK | HOOK_UINT16, 0x840F},
	{0x00026C92, CODE_HOOK | HOOK_JMP_DOOM, 0x00026D3E},
	// replace calls to 'EV_DoLockedDoor'
	{0x00030B0E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_obj_key},
	{0x00030E55, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_obj_key},
	// change map name variable in 'P_SetupLevel'
	{0x0002E858, CODE_HOOK | HOOK_UINT16, 0x61EB},
	{0x0002E8BB, CODE_HOOK | HOOK_UINT8, 0xB8},
	{0x0002E8BC, CODE_HOOK | HOOK_UINT32, (uint32_t)&map_lump},
	// disable 'no secret exit' check in 'G_SecretExitLevel'
	{0x00020D61, CODE_HOOK | HOOK_UINT16, 0x1FEB},
	// replace call to 'G_DoNewGame' in 'G_Ticker'
	{0x000204B2, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)do_new_game},
	{0x000204B7, CODE_HOOK | HOOK_UINT16, 0x57EB},
	// replace call to 'G_DoNewGame' in 'D_DoomMain'
	{0x0001EB07, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)do_autostart_game},
	// replace call to 'G_DoCompleted' in 'G_Ticker'
	{0x00020533, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)do_completed},
	// replace call to 'G_WorldDone'
	{0x0003C98A, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_world_done},
	{0x0003DCAA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_world_done},
	// replace call to 'G_DoWorldDone' in 'G_Ticker'
	{0x00020547, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)do_world_done},
	{0x0002054C, CODE_HOOK | HOOK_UINT16, 0x1BEB},
	// replace call to 'S_ChangeMusic' in 'WI_Ticker'
	{0x0003DBFA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)music_intermission},
	// disable music setup in 'S_Start'
	{0x0003F642, CODE_HOOK | HOOK_UINT16, 0x3EEB},
	// disable music setup in 'F_StartCast'
	{0x0001C768, CODE_HOOK | HOOK_SET_NOPS, 5},
	// use 'map_level_info->name' in 'HU_Start'
	{0x0003B5D2, CODE_HOOK | HOOK_UINT8, 0xA1},
	{0x0003B5D3, CODE_HOOK | HOOK_UINT32, (uint32_t)&map_level_info},
	{0x0003B5D7, CODE_HOOK | HOOK_UINT32, 0x708B | (offsetof(map_level_t,name) << 16)},
	{0x0003B5DA, CODE_HOOK | HOOK_UINT16, 0x2DEB},
	// replace call to 'W_CacheLumpName' in 'F_TextWrite' with 'W_CacheLumpNum'
	{0x0001C5A3, CODE_HOOK | HOOK_CALL_DOOM, 0x00038D00},
	// disable range check in 'WI_initVariables'
	{0x0003E9CE, CODE_HOOK | HOOK_JMP_DOOM, 0x0003EABA},
	// replace 'WI_drawLF'
	{0x0003C360, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)draw_finished},
	// replace 'WI_drawEL'
	{0x0003C3F0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)draw_entering},
	// replace 'commercial' check in intermission code
	{0x0003C568, CODE_HOOK | HOOK_UINT32, (uint32_t)&intermission_mode},
	{0x0003C60E, CODE_HOOK | HOOK_UINT32, (uint32_t)&intermission_mode},
	{0x0003C727, CODE_HOOK | HOOK_UINT32, (uint32_t)&intermission_mode},
	{0x0003CA80, CODE_HOOK | HOOK_UINT32, (uint32_t)&intermission_mode},
	{0x0003CAF0, CODE_HOOK | HOOK_UINT32, (uint32_t)&intermission_mode},
	{0x0003CE22, CODE_HOOK | HOOK_UINT32, (uint32_t)&intermission_mode},
	{0x0003D4CA, CODE_HOOK | HOOK_UINT32, (uint32_t)&intermission_mode},
	{0x0003D911, CODE_HOOK | HOOK_UINT32, (uint32_t)&intermission_mode},
	{0x0003DBE7, CODE_HOOK | HOOK_UINT32, (uint32_t)&intermission_mode},
	{0x0003DCCB, CODE_HOOK | HOOK_UINT32, (uint32_t)&intermission_mode},
	// replace 'commercial' check in finale code
	{0x0001C4A8, CODE_HOOK | HOOK_UINT32, (uint32_t)&intermission_mode},
	{0x0001C51C, CODE_HOOK | HOOK_UINT32, (uint32_t)&intermission_mode},
	// disable use of 'lnames' in 'WI_loadData' and 'WI_unloadData'
	{0x0003DD23, CODE_HOOK | HOOK_JMP_DOOM, 0x0003DEBF},
	{0x0003DD92, CODE_HOOK | HOOK_JMP_DOOM, 0x0003DDE6},
	{0x0003E15F, CODE_HOOK | HOOK_JMP_DOOM, 0x0003E308},
	{0x0003E247, CODE_HOOK | HOOK_JMP_DOOM, 0x0003E288},
	{0x0003E308, CODE_HOOK | HOOK_UINT16, 0x08EB},
	// import variables
	{0x0002C0D0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&playerstarts},
	{0x0002C154, DATA_HOOK | HOOK_IMPORT, (uint32_t)&deathmatchstarts},
	{0x0002C150, DATA_HOOK | HOOK_IMPORT, (uint32_t)&deathmatch_p},
	{0x0002A3C0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&nomonsters},
	{0x0002A3C4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&fastparm},
	{0x0002A3C8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&respawnparm},
	{0x0002B3FC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&deathmatch},
	{0x0002B3E8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gamemap},
	{0x0002B3F8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gameepisode},
	{0x0002B3E0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gameskill},
	{0x0002CF80, DATA_HOOK | HOOK_IMPORT, (uint32_t)&leveltime},
	{0x0002B3C8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalsecret},
	{0x0002B3D0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalitems},
	{0x0002B3D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalkills},
	{0x0005A170, DATA_HOOK | HOOK_IMPORT, (uint32_t)&skytexture},
	{0x0005A164, DATA_HOOK | HOOK_IMPORT, (uint32_t)&skyflatnum},
	{0x0002C11C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsides},
	{0x0002C134, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numlines},
	{0x0002C14C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsectors},
	{0x0002C120, DATA_HOOK | HOOK_IMPORT, (uint32_t)&lines},
	{0x0002C138, DATA_HOOK | HOOK_IMPORT, (uint32_t)&vertexes},
	{0x0002C118, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sides},
	{0x0002C148, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sectors},
	{0x0002C040, DATA_HOOK | HOOK_IMPORT, (uint32_t)&activeplats},
	{0x0002B840, DATA_HOOK | HOOK_IMPORT, (uint32_t)&activeceilings},
	{0x0002B40C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&usergame},
	{0x0002B400, DATA_HOOK | HOOK_IMPORT, (uint32_t)&netgame},
	{0x0002B2F8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&secretexit},
	{0x00012720, DATA_HOOK | HOOK_IMPORT, (uint32_t)&prndindex},
	{0x0002B3B8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&viewactive},
	{0x00012C5C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&automapactive},
	{0x0002B2EC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&d_skill},
	{0x0002B2F0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&d_map},
	{0x0002B2F4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&d_episode},
	// more variables
	{0x0002B990, DATA_HOOK | HOOK_IMPORT, (uint32_t)&nofit},
	{0x0002B994, DATA_HOOK | HOOK_IMPORT, (uint32_t)&crushchange},
	{0x0002B9E4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tmdropoffz},
	{0x0002C038, DATA_HOOK | HOOK_IMPORT, (uint32_t)&openrange},
	{0x0002C034, DATA_HOOK | HOOK_IMPORT, (uint32_t)&opentop},
	{0x0002C030, DATA_HOOK | HOOK_IMPORT, (uint32_t)&openbottom},
	{0x0002B9F4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&ceilingline},
	{0x0002B9F8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&linetarget},
	{0x0002B990, DATA_HOOK | HOOK_IMPORT, (uint32_t)&nofit},
	{0x0002C104, DATA_HOOK | HOOK_IMPORT, (uint32_t)&bmaporgx},
	{0x0002C108, DATA_HOOK | HOOK_IMPORT, (uint32_t)&bmaporgy},
	// map titles
	{0x00013C04, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mapnames},
	{0x00013CBC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&mapnames2},
	// finale, victory .. and others
	{0x0002ADB0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&wminfo},
	{0x0002929C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finaleflat},
	{0x000292A0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finaletext},
	{0x000292A4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finalecount},
	{0x000292A8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finalestage},
};

