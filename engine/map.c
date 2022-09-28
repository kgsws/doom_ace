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

static int32_t *finaleflat; // type is changed!
static uint8_t **finaletext;
static uint32_t *finalecount;
static uint32_t *finalestage;

map_lump_name_t map_lump;
int32_t map_lump_idx;
map_level_t *map_level_info;

uint_fast8_t map_skip_stuff;
uint_fast8_t is_title_map;

uint32_t num_maps = DEF_MAP_COUNT;
map_level_t *map_info;

uint32_t num_clusters = DEF_CLUSTER_COUNT;
map_cluster_t *map_cluster;

static int32_t cluster_music;

//
static uint8_t **mapnames;
static uint8_t **mapnames2;

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
	return map_info + MAPDEF_UNNAMED;
}

static map_cluster_t *map_find_cluster(uint32_t num)
{
	for(uint32_t i = 0; i < num_clusters; i++)
	{
		map_cluster_t *cl = map_cluster + i;
		if(cl->cluster_num == num)
			return cl;
	}
	return map_cluster + CLUSTER_NONE;
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
	*gamemap = 1; // fake

	if(*gameepisode)
	{
		is_title_map = 0;
		cache = !*demoplayback;
	} else
	{
		map_lump.wame = 0x50414D454C544954; // TITLEMAP
		*gameepisode = 1;
		*netgame = 0;
		*netdemo = 0;
		is_title_map = 1;
		cache = 0;
	}

	map_lump_idx = W_CheckNumForName(map_lump.name);

	if(map_lump_idx < 0)
		goto map_load_error;
	// TODO: check map validity

	// setup level info
	map_level_info = map_get_info(map_lump_idx);

	// sky
	*skytexture = map_level_info->texture_sky;

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

	lump = W_CheckNumForName("TITLEMAP");
	if(lump < 0)
	{
		*gameaction = ga_nothing;
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

	*consoleplayer = 0;
	memset(players, 0, sizeof(player_t) * MAXPLAYERS);
	players[0].playerstate = PST_REBORN;

	memset(playeringame, 0, sizeof(uint32_t) * MAXPLAYERS);
	playeringame[0] = 1;

	map_load_setup();
}

//
// default map setup

static void setup_episode(uint32_t start, uint32_t episode, uint32_t secret, uint8_t **names)
{
	uint8_t text[12];

	for(uint32_t i = 0; i < 9; i++)
	{
		map_level_t *info = map_info + start + i;
		int32_t lump;

		info->name = names[i];
		info->cluster = episode;
		info->levelnum = (episode - 1) * 10 + i;

		doom_sprintf(text, "E%uM%u", episode, i + 1);
		info->lump = wad_check_lump(text);

		doom_sprintf(text, "WILV%02u", info->levelnum - 1);
		info->title_patch = wad_check_lump(text);

		doom_sprintf(text, "SKY%u", episode);
		info->texture_sky = texture_num_get(text);

		info->music_lump = -1; // TODO
		info->par_time = 123; // TODO

		if(i == 8)
		{
			info->next_normal = start + secret;
			info->next_secret = start + secret;
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
		int32_t lump;

		info->name = mapnames2[start + i - MAPDEF_MAP01];
		info->cluster = cluster;
		info->levelnum = (start + i) - (MAPDEF_MAP01 - 1);

		doom_sprintf(text, "MAP%02u", info->levelnum);
		info->lump = wad_check_lump(text);

		doom_sprintf(text, "CWILV%02u", info->levelnum - 1);
		info->title_patch = wad_check_lump(text);

		doom_sprintf(text, "SKY%u", sky);
		info->texture_sky = texture_num_get(text);

		info->music_lump = -1; // TODO
		info->par_time = 123; // TODO

		switch(start + i)
		{
			case MAPDEF_MAP15:
				info->next_normal = MAPDEF_MAP16;
				info->next_secret = MAPDEF_MAP31;
			break;
			case MAPDEF_UNNAMED:
				info->levelnum = 0;
				info->name = "Unnamed";
			case MAPDEF_MAP30:
				info->next_normal = MAPDEF_END_DOOM2;
				info->next_secret = MAPDEF_END_DOOM2;
			break;
			case MAPDEF_MAP31:
				info->next_normal = MAPDEF_MAP16;
				info->next_secret = MAPDEF_MAP32;
			break;
			case MAPDEF_MAP32:
				info->next_normal = MAPDEF_MAP16;
				info->next_secret = MAPDEF_MAP16;
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

	doom_printf("[ACE] init MAPs\n");
	ldr_alloc_message = "Map and game info memory allocation failed!";

	// default cluster music
	if(*gamemode)
		doom_sprintf(text, "D_%s", (void*)0x00024B40 + doom_data_segment);
	else
		doom_sprintf(text, "D_%s", (void*)0x00024A30 + doom_data_segment);
	cluster_music = wad_check_lump(text);

	// allocate clusters
	map_cluster = ldr_malloc(DEF_CLUSTER_COUNT * sizeof(map_cluster_t));

	// prepare default clusters
	for(uint32_t i = 0; i < DEF_CLUSTER_COUNT; i++)
	{
		const uint8_t *flat;

		map_cluster[i] = doom_cluster[i];
		map_cluster[i].lump_music = cluster_music;

		if(map_cluster[i].text_leave)
			map_cluster[i].text_leave += doom_data_segment;

		if(map_cluster[i].text_enter)
			map_cluster[i].text_enter += doom_data_segment;

		if(cluster_flat[i])
			flat = cluster_flat[i] + doom_data_segment;
		else
			flat = "MFLR8_3";
		map_cluster[i].flat_num = flatlump[flat_num_get(flat)];
	}
	map_cluster[CLUSTER_D1_EPISODE4].text_leave = ep4_text;

	// allocate maps
	map_info = ldr_malloc(DEF_MAP_COUNT * sizeof(map_level_t));

	// prepare default maps

	// episode 1
	setup_episode(MAPDEF_E1M1, 1, 3, mapnames + MAPDEF_E1M1);

	// episode 2
	setup_episode(MAPDEF_E2M1, 2, 5, mapnames + MAPDEF_E2M1);

	// episode 3
	setup_episode(MAPDEF_E3M1, 3, 6, mapnames + MAPDEF_E3M1);

	// episode 4 // TODO

	// cluster 5
	setup_cluster(MAPDEF_MAP01, MAPDEF_MAP07 - MAPDEF_MAP01, 5, 1);

	// cluster 6
	setup_cluster(MAPDEF_MAP07, MAPDEF_MAP12 - MAPDEF_MAP07, 6, 1);

	// cluster 7
	setup_cluster(MAPDEF_MAP12, MAPDEF_MAP21 - MAPDEF_MAP12, 7, 2);

	// cluster 8
	setup_cluster(MAPDEF_MAP21, MAPDEF_MAP31 - MAPDEF_MAP21, 8, 3);

	// cluster 9
	setup_cluster(MAPDEF_MAP31, 1, 9, 3);

	// cluster 10
	setup_cluster(MAPDEF_MAP32, 1, 10, 3);

	// cluster 0; unknown maps
	setup_cluster(MAPDEF_UNNAMED, 1, 0, 1);

	// default, dummy
	map_level_info = map_info + MAPDEF_UNNAMED;
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
	uint16_t next;
	map_cluster_t *old_cl, *new_cl;

	*gameaction = ga_worlddone;

	old_cl = map_find_cluster(map_level_info->cluster);

	if(*secretexit)
	{
		for(uint32_t i = 0; i < MAXPLAYERS; i++)
			players[i].didsecret = 1;
		next = map_level_info->next_secret;
	} else
		next = map_level_info->next_normal;

	if(next < num_maps)
		new_cl = map_find_cluster(map_info[next].cluster);
	else
		new_cl = NULL;

	if(old_cl != new_cl)
	{
		if(new_cl && new_cl->text_enter)
		{
			*finaletext = new_cl->text_enter;
			*finaleflat = new_cl->flat_num;
		} else
		{
			*finaletext = old_cl->text_leave;
			*finaleflat = old_cl->flat_num;
		}

		if(*finaletext)
		{
			*gameaction = ga_nothing;
			*gamestate = GS_FINALE;
			*viewactive = 0;
			*automapactive = 0;
			*finalestage = 0;
			*finalecount = 0;
			// TODO: S_ChangeMusic
		}
	}

	if(next >= num_maps || map_info[next].lump < 0)
	{
		map_start_title();
		return;
	}

	map_lump.wame = (*lumpinfo)[map_info[next].lump].wame;
}

__attribute((regparm(2),no_caller_saved_registers))
static void do_world_done()
{
	*gameaction = ga_nothing;
	map_load_setup();
}

__attribute((regparm(2),no_caller_saved_registers))
static void music_intermission()
{
	doom_printf("TODO: intermission music\n");
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
	// replace call to 'G_DoNewGame' in 'G_Ticker'
	{0x000204B2, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)do_new_game},
	{0x000204B7, CODE_HOOK | HOOK_UINT16, 0x57EB},
	// replace call to 'G_DoNewGame' in 'D_DoomMain'
	{0x0001EB07, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)do_autostart_game},
	// replace call to 'G_WorldDone'
	{0x0003C98A, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_world_done},
	{0x0003DCAA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_world_done},
	// replace call to 'G_DoWorldDone' in 'G_Ticker'
	{0x00020547, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)do_world_done},
	{0x0002054C, CODE_HOOK | HOOK_UINT16, 0x1BEB},
	// replace call to 'S_ChangeMusic' in 'WI_Ticker'
	{0x0003DBFA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)music_intermission},
	// use 'map_level_info->name' in 'HU_Start'
	{0x0003B5D2, CODE_HOOK | HOOK_UINT8, 0xA1},
	{0x0003B5D3, CODE_HOOK | HOOK_UINT32, (uint32_t)&map_level_info},
	{0x0003B5D7, CODE_HOOK | HOOK_UINT32, 0x708B | (offsetof(map_level_t,name) << 16)},
	{0x0003B5DA, CODE_HOOK | HOOK_UINT16, 0x2DEB},
	// replace call to 'W_CacheLumpName' in 'F_TextWrite' with 'W_CacheLumpNum'
	{0x0001C5A3, CODE_HOOK | HOOK_CALL_DOOM, 0x00038D00},
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
	// finale
	{0x0002929C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finaleflat},
	{0x000292A0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finaletext},
	{0x000292A4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finalecount},
	{0x000292A8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&finalestage},
};

