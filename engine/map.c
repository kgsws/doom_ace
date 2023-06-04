// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "vesa.h"
#include "wadfile.h"
#include "decorate.h"
#include "mobj.h"
#include "inventory.h"
#include "animate.h"
#include "think.h"
#include "player.h"
#include "hitscan.h"
#include "saveload.h"
#include "config.h"
#include "demo.h"
#include "sound.h"
#include "special.h"
#include "sight.h"
#include "map.h"
#include "menu.h"
#include "stbar.h"
#include "render.h"
#include "draw.h"
#include "cheat.h"
#include "polyobj.h"
#include "extra3d.h"
#include "action.h"
#include "terrain.h"
#include "ldr_flat.h"
#include "ldr_texture.h"
#include "textpars.h"

#include "map_info.h"

#define WI_TITLEY	2
#define	FB	0
#define XNOD_SQRT_PRECISION	12	// smaller is better but slower

enum
{
	IT_U8,
	IT_U16,
	IT_MUSIC,
	IT_PATCH,
	IT_FLAT_LUMP,
	IT_TEXTURE,
	IT_MAP,
	IT_TEXT_CHAIN,
	IT_FLAG,
};

typedef struct
{
	const uint8_t *lock;
	const uint8_t *any;
	const uint8_t *message;
	const uint8_t *msg_data;
	const uint8_t *remote;
	const uint8_t *rmt_data;
	const uint8_t *mapcolor;
	const uint8_t *sound;
} lockdefs_kw_list_t;

typedef struct
{
	const uint8_t *name;
	uint32_t offset;
	uint16_t flag;
	uint8_t type;
} map_attr_t;

//

subsector_extra_t *e_subsectors;

map_lump_name_t map_lump;
int32_t map_lump_idx;
uint_fast8_t map_format;
map_level_t *map_level_info;
map_level_t *map_next_info;
uint16_t map_next_num;
uint8_t map_start_id;
uint8_t map_start_facing;
uint16_t map_next_levelnum;

uint_fast8_t map_skip_stuff;
uint_fast8_t is_title_map;
uint_fast8_t is_net_desync;
uint_fast8_t xnod_present;

uint_fast8_t survival;
uint_fast8_t net_inventory;
uint_fast8_t no_friendly_fire;
uint_fast8_t keep_keys;
uint_fast8_t weapons_stay;

static map_cluster_t *old_cl;
static map_cluster_t *new_cl;

uint32_t num_maps;
map_level_t *map_info;

uint32_t num_clusters;
map_cluster_t *map_cluster;

static uint32_t max_cluster;
static int32_t cluster_music;

void *lockdefs;
uint32_t lockdefs_size;
static uint32_t lockdefs_max = LOCKDEFS_DEF_SIZE;

static int32_t patch_finshed;
static int32_t patch_entering;

static uint32_t fake_game_mode;
static patch_t *victory_patch;

uint32_t map_episode_count;
map_episode_t map_episode_def[MAX_EPISODES];

// map check
static uint64_t map_wame_check[] =
{
	0x000053474E494854, // THINGS
	0x53464544454E494C, // LINEDEFS
	0x5346454445444953, // SIDEDEFS
	0x5345584554524556, // VERTEXES
	0x0000000053474553, // SEGS
	0x53524F5443455353, // SSECTORS
	0x0000005345444F4E, // NODES
	0x0053524F54434553, // SECTORS
	0x00005443454A4552, // REJECT
	0x50414D4B434F4C42, // BLOCKMAP
	0x524F495641484542, // BEHAVIOR (optional)
};

// unnamed map
static map_level_t map_info_unnamed =
{
	.name = "Unnamed",
	.next_normal = MAP_END_DOOM_CAST,
	.next_secret = MAP_END_DOOM_CAST,
	.title_lump = -1,
	.music_level = -1,
	.music_inter = -1,
	.win_lump = {-1, -1},
	.par_time = 0,
};

// default map
static map_level_t map_info_default;

// map attributes
static const map_attr_t map_attr[] =
{
	{.name = "levelnum", .type = IT_U8, .offset = offsetof(map_level_t, levelnum)},
	{.name = "cluster", .type = IT_U8, .offset = offsetof(map_level_t, cluster)},
	{.name = "par", .type = IT_U16, .offset = offsetof(map_level_t, par_time)},
	{.name = "titlepatch", .type = IT_PATCH, .offset = offsetof(map_level_t, title_lump)},
	{.name = "music", .type = IT_MUSIC, .offset = offsetof(map_level_t, music_level)},
	{.name = "intermusic", .type = IT_MUSIC, .offset = offsetof(map_level_t, music_inter)},
	{.name = "sky1", .type = IT_TEXTURE, .offset = offsetof(map_level_t, texture_sky[0])},
	{.name = "sky2", .type = IT_TEXTURE, .offset = offsetof(map_level_t, texture_sky[1])},
	{.name = "next", .type = IT_MAP, .offset = offsetof(map_level_t, next_normal)},
	{.name = "secretnext", .type = IT_MAP, .offset = offsetof(map_level_t, next_secret)},
	// flags
	{.name = "nointermission", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_NO_INTERMISSION},
	{.name = "fallingdamage", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_FALLING_DAMAGE},
	{.name = "monsterfallingdamage", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_MONSTER_FALL_DMG_KILL},
	{.name = "propermonsterfallingdamage", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_MONSTER_FALL_DMG},
	{.name = "nofreelook", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_NO_FREELOOK},
//	{.name = "filterstarts", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_FILTER_STARTS},
//	{.name = "useplayerstartz", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_USE_PLAYER_START_Z},
	{.name = "allowrespawn", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_ALLOW_RESPAWN},
	{.name = "noinfighting", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_NO_INFIGHTING},
	{.name = "totalinfighting", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_TOTAL_INFIGHTING},
//	{.name = "checkswitchrange", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_CHECK_SWITCH_RANGE},
	{.name = "resetinventory", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_RESET_INVENTORY},
//	{.name = "forgetstate", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_FORGET_STATE},
	{.name = "spawnwithweaponraised", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_SPAWN_WITH_WEAPON_RAISED},
	{.name = "strictmonsteractivation", .type = IT_FLAG, .offset = offsetof(map_level_t, flags), .flag = MAP_FLAG_NO_MONSTER_ACTIVATION},
	// terminator
	{.name = NULL}
};
// cluster attributes
static const map_attr_t clst_attr[] =
{
	{.name = "music", .type = IT_MUSIC, .offset = offsetof(map_cluster_t, lump_music)},
	{.name = "flat", .type = IT_FLAT_LUMP, .offset = offsetof(map_cluster_t, lump_flat)},
	{.name = "entertext", .type = IT_TEXT_CHAIN, .offset = offsetof(map_cluster_t, text_enter)},
	{.name = "exittext", .type = IT_TEXT_CHAIN, .offset = offsetof(map_cluster_t, text_leave)},
	{.name = "hub", .type = IT_FLAG, .offset = offsetof(map_cluster_t, flags), .flag = CLST_FLAG_HUB},
	// terminator
	{.name = NULL}
};
// episode attributes
static const map_attr_t epi_attr[] =
{
	{.name = "picname", .type = IT_PATCH, .offset = offsetof(map_episode_t, title_lump)},
	{.name = "noskillmenu", .type = IT_FLAG, .offset = offsetof(map_episode_t, flags), .flag = EPI_FLAG_NO_SKILL_MENU},
	// terminator
	{.name = NULL}
};

// LOCKDEFS parser
static const lockdefs_kw_list_t lockdefs_kw_list[] =
{
	{
		.lock = "l",
		.any = "a",
		.message = "m",
		.msg_data = "g",
		.remote = "r",
		.rmt_data = "h",
		.mapcolor = "c",
	},
	{
		.lock = "lock",
		.any = "any",
		.message = "message",
		.remote = "remotemessage",
		.mapcolor = "mapcolor",
		.sound = "lockedsound",
	}
};

//

static const uint8_t skillbits[] = {1, 1, 2, 4, 4};

//
static const hook_t patch_doom[];
static const hook_t patch_hexen[];

//
// line scroller

__attribute((regparm(2),no_caller_saved_registers))
void think_line_scroll(line_scroll_t *ls)
{
	side_t *side = sides + ls->line->sidenum[0];
	side->textureoffset += (fixed_t)ls->x * (FRACUNIT / 64);
	side->rowoffset += (fixed_t)ls->y * (FRACUNIT / 64);
}

static inline void spawn_line_scroll()
{
	for(uint32_t i = 0; i < numlines; i++)
	{
		line_t *ln = lines + i;
		int16_t x = 0;
		int16_t y = 0;

		if(map_format == MAP_FORMAT_DOOM)
		{
			if(ln->special == 48)
				x = 64;
		} else
		switch(ln->special)
		{
			case 100: // Scroll_Texture_Left
				x = ln->arg0;
			break;
			case 101: // Scroll_Texture_Right
				x = -(int16_t)ln->arg0;
			break;
			case 102: // Scroll_Texture_Up
				y = ln->arg0;
			break;
			case 103: // Scroll_Texture_Down
				y = -(int16_t)ln->arg0;
			break;
			case 225: // Scroll_Texture_Offsets
				x = -sides[ln->sidenum[0]].textureoffset >> 10;
				y = sides[ln->sidenum[0]].rowoffset >> 10;
			break;
		}

		if(x || y)
		{
			line_scroll_t *ls;

			ln->special = 0;

			ls = Z_Malloc(sizeof(line_scroll_t), PU_LEVEL, NULL);
			ls->line = ln;
			ls->x = x;
			ls->y = y;
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

map_cluster_t *map_find_cluster(uint32_t num)
{
	if(!num)
		return NULL;

	// do a backward search
	for(int32_t i = num_clusters - 1; i >= 0; i--)
	{
		map_cluster_t *cl = map_cluster + i;
		if(cl->idx == num)
			return cl;
	}

	return NULL;
}

//
// map things

static void spawn_map_thing(map_thinghex_t *mt, mapthing_t *ot)
{
	uint32_t idx;
	mobj_t *mo;
	mobjinfo_t *info;
	fixed_t x, y, z;
	angle_t angle;
	uint32_t hack;

	if(ot)
	{
		// convert old type to new
		static map_thinghex_t fake;

		mt = &fake;

		fake.x = ot->x;
		fake.y = ot->y;
		fake.angle = ot->angle;
		fake.type = ot->type;
		fake.flags = ot->options & 15;

		angle = ANG45 * (fake.angle / 45);

		if(ot->options & 16)
			fake.flags |= 512 | 1024;
		else
			fake.flags |= 256 | 512 | 1024;
	} else
		angle = (ANG45 / 45) * mt->angle;

	// deathmatch starts
	if(mt->type == 11)
	{
		if(deathmatch_p < deathmatchstarts + 10)
		{
			mapthing_t *dt = deathmatch_p;
			dt->x = mt->x;
			dt->y = mt->y;
			dt->angle = mt->angle;
			deathmatch_p++;
		}
		return;
	}

	// player starts
	if(mt->type && mt->type <= 4)
	{
		uint32_t idx;

		if(mt->arg[0] != map_start_id)
			return;

		idx = mt->type - 1;
		playerstarts[idx].x = mt->x;
		playerstarts[idx].y = mt->y;
		playerstarts[idx].angle = mt->angle;
		playerstarts[idx].type = mt->type;
		playerstarts[idx].options = mt->arg[0];
		if(!deathmatch && map_skip_stuff != 1)
			mobj_spawn_player(idx, mt->x * FRACUNIT, mt->y * FRACUNIT, angle);

		return;
	}

	// polyobjects
	if(mt && mt->type >= 9300 && mt->type <= 9303)
	{
		poly_object(mt);
		return;
	}

	// check for save/load
	if(map_skip_stuff)
	{
		if(!mt)
			return;
		if(mt->type != 1411)
			// don't skip sound sequence
			return;
	}

	hack = mt->type;

	// check network game
	if(netgame)
	{
		if(deathmatch)
		{
			if(!(mt->flags & 1024))
				return;
		} else
		{
			if(!(mt->flags & 512))
				return;
		}
	} else
	{
		if(!(mt->flags & 256))
			return;
	}

	// check skill level
	if(!(mt->flags & skillbits[gameskill]))
		return;

	// special types
	if(	mt && (
		mt->type == 1411 ||	// sound sequence
		mt->type == 9038 ||	// color setter
		mt->type == 9039	// fade setter
	)){
		subsector_t *ss;
		ss = R_PointInSubsector((fixed_t)mt->x << FRACBITS, (fixed_t)mt->y << FRACBITS);
		switch(mt->type)
		{
			case 1411:
				ss->sector->sndseq = mt->arg[0];
			break;
			case 9038:
				ss->sector->extra->color = (mt->arg[0] >> 4) | (mt->arg[1] & 0xF0) | ((uint16_t)(mt->arg[2] & 0xF0) << 4) | ((uint16_t)(mt->arg[3] & 0xF0) << 8);
			break;
			case 9039:
				ss->sector->extra->fade = (mt->arg[0] >> 4) | (mt->arg[1] & 0xF0) | ((uint16_t)(mt->arg[2] & 0xF0) << 4);
			break;
		}
		return;
	}

	// ZDoom types translated to teleport spot
	if(mt)
	{
		if(	mt->type == 9044 ||	// teleport with Z
			mt->type == 9001 ||	// map spot
			mt->type == 9025 ||	// security camera
			mt->type == 9070 ||	// interpolation point
			mt->type == 9071 ||	// path follower
			mt->type == 9072 ||	// moving camera
			mt->type == 9074 ||	// actor mover
			mt->type == 9997 ||	// actor leaves sector
			mt->type == 9998	// actor enters sector
		)
			mt->type = 14;
	}

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

	if(info->replacement)
		info = mobjinfo + info->replacement;

	// doom map format, coop, only monsters (and keys, lol)
	if(	ot &&
		netgame &&
		!deathmatch &&
		ot->options & 16 &&
		!(info->flags1 & MF1_ISMONSTER) &&
		info->extra_type != ETYPE_KEY
	)
		return;

	// 'not in deathmatch'
	if(deathmatch && info->flags & MF_NOTDMATCH)
		return;

	// '-nomonsters'
	if(nomonsters && info->flags1 & MF1_ISMONSTER)
		return;

	// position
	x = (fixed_t)mt->x << FRACBITS;
	y = (fixed_t)mt->y << FRACBITS;
	z = info->flags & MF_SPAWNCEILING ? 0x7FFFFFFF : 0x80000000;

	if(info->flags & MF_SPAWNCEILING)
		z = 0x7FFFFFFF;
	else
	if(mt && mt->z > 0)
	{
		subsector_t *ss;
		ss = R_PointInSubsector(x, y);
		z = ss->sector->floorheight + (fixed_t)mt->z * FRACUNIT;
	} else
		z = 0x80000000;

	// spawn
	mo = P_SpawnMobj(x, y, z, idx);
	mo->spawnpoint.x = mt->x;
	mo->spawnpoint.y = mt->y;
	mo->spawnpoint.type = hack;
	mo->spawnpoint.options = mo->type;
	mo->angle = angle;

	if(mt->flags & MTF_AMBUSH)
		mo->flags |= MF_AMBUSH;

	if(mt->flags & MTF_FRIENDLY)
		mo->iflags |= MFI_FRIENDLY;

	if(mt->flags & MTF_STANDSTILL)
		mo->iflags |= MFI_STANDSTILL;

	if(mt->flags & MTF_INACTIVE)
	{
		mo->flags1 |= MF1_DORMANT;
		if(mo->info->extra_type == ETYPE_SWITCHABLE)
		{
			if(mo->info->st_switchable.inactive)
			{
				state_t *st = states + mo->info->st_switchable.inactive;
				mo->state = st;
				mo->sprite = st->sprite;
				mo->frame = st->frame;
				mo->tics = st->tics == 0xFFFF ? -1 : st->tics;
			}
		} else
		if(mo->flags1 & MF1_ISMONSTER)
			mo->tics = -1;
	}

	if(mt->flags & MTF_SHADOW)
	{
		mo->render_style = RS_TRANSLUCENT;
		mo->render_alpha = 50;
	}

	if(!(mo->flags2 & MF2_SYNCHRONIZED) && mo->tics > 0)
		mo->tics = 1 + (P_Random() % mo->tics);

	mo->special.special = mt->special;
	for(uint32_t i = 0; i < 5; i++)
	{
		if(i < mo->info->args[5])
			mo->special.arg[i] = mo->info->args[i];
		else
			mo->special.arg[i] = mt->arg[i];
	}
	mo->special.tid = mt->tid;

	if(	hack == 9044 ||
		hack == 9001 ||
		hack == 9025 ||
		hack == 9070 ||
		hack == 9071 ||
		hack == 9072 ||
		hack == 9074
	)
		mo->flags |= MF_NOGRAVITY;

	if(hack == 9997)
		mo->subsector->sector->extra->action.leave = mo;

	if(hack == 9998)
		mo->subsector->sector->extra->action.enter = mo;

	if(	hack == 9070 ||
		hack == 9025
	){
		// mobj pitch
		int32_t ang = (int8_t)mt->arg[0];
		ang *= 360;
		ang /= -256;
		mo->pitch = ang << 23;
	}
}

//
// extended nodes

static inline fixed_t seg_offset(vertex_t *vs, vertex_t *vl)
{
	// TODO: optimize: horizontal / vertical / diagonal walls
	uint32_t xx, yy;
	uint32_t L, R;

	if(vs == vl)
		return 0;

	xx = abs(vs->x - vl->x);
	yy = abs(vs->y - vl->y);

	xx >>= XNOD_SQRT_PRECISION;
	yy >>= XNOD_SQRT_PRECISION;

	xx *= xx;
	yy *= yy;

	yy += xx;
	R = yy + 1;
	L = 0;

	while(L != R - 1)
	{
		uint64_t M = (L + R) / 2;

		if(M * M <= yy)
			L = M;
		else
			R = M;
	}

	return L << XNOD_SQRT_PRECISION;
}

static inline void prepare_nodes(int32_t map_lump)
{
	uint32_t tmp[3];

	xnod_present = 0;
	numvertexes = 0;

	if(W_LumpLength(map_lump + ML_SEGS))
		return;

	wad_read_lump(&tmp, map_lump + ML_NODES, 3 * sizeof(uint32_t));
	if(tmp[0] != 0x444F4E58) // XNOD
		return;

	numvertexes = tmp[1] + tmp[2];

	// mark as present; do not load yet
	xnod_present = 1;
	doom_printf("[MAP] extended nodes\n");
}

static inline void load_nodes(int32_t map_lump)
{
	void *xnod_data;
	void *xnod_ptr;
	vertex_t *vtx;
	uint32_t count;

	if(!xnod_present)
	{
		P_LoadSubsectors(map_lump + ML_SSECTORS);
		P_LoadNodes(map_lump + ML_NODES);
		P_LoadSegs(map_lump + ML_SEGS);
		goto extra_ssec;
	}

	xnod_data = W_CacheLumpNum(map_lump + ML_NODES, PU_STATIC);
	xnod_ptr = xnod_data + sizeof(uint32_t);

	vtx = vertexes;
	vtx += *((uint32_t*)xnod_ptr);
	xnod_ptr += sizeof(uint32_t);

	// vertexes
	count = *((uint32_t*)xnod_ptr);
	xnod_ptr += sizeof(uint32_t);
	for(uint32_t i = 0; i < count; i++)
	{
		vtx->x = *((fixed_t*)xnod_ptr);
		xnod_ptr += sizeof(fixed_t);
		vtx->y = *((fixed_t*)xnod_ptr);
		xnod_ptr += sizeof(fixed_t);
		vtx++;
	}

	// ssectors
	numsubsectors = *((uint32_t*)xnod_ptr);
	xnod_ptr += sizeof(uint32_t);
	subsectors = Z_Malloc(numsubsectors * sizeof(subsector_t), PU_LEVEL, NULL);
	count = 0;
	for(uint32_t i = 0; i < numsubsectors; i++)
	{
		subsector_t *ss = subsectors + i;
		ss->numlines = *((uint32_t*)xnod_ptr);
		ss->firstline = count;
		count += ss->numlines;
		xnod_ptr += sizeof(uint32_t);
	}

	// segs
	numsegs = *((uint32_t*)xnod_ptr);
	xnod_ptr += sizeof(uint32_t);
	segs = Z_Malloc(numsegs * sizeof(seg_t), PU_LEVEL, NULL);
	for(uint32_t i = 0; i < numsegs; i++)
	{
		seg_t *seg = segs + i;
		line_t *line = lines;
		uint8_t side;

		seg->v1 = vertexes + *((uint32_t*)xnod_ptr);
		xnod_ptr += sizeof(uint32_t);
		seg->v2 = vertexes + *((uint32_t*)xnod_ptr);
		xnod_ptr += sizeof(uint32_t);

		line += *((uint16_t*)xnod_ptr);
		xnod_ptr += sizeof(uint16_t);

		side = *((uint8_t*)xnod_ptr);
		xnod_ptr += sizeof(uint8_t);

		seg->angle = R_PointToAngle2(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y);
		seg->offset = seg_offset(seg->v1, (side ? line->v2 : line->v1));

		seg->linedef = line;
		seg->sidedef = sides + line->sidenum[side];

		seg->frontsector = sides[line->sidenum[side]].sector;
		if(line->flags & ML_TWOSIDED)
			seg->backsector = sides[line->sidenum[side^1]].sector;
		else
			seg->backsector = NULL;
	}

	// subsectors
	numnodes = *((uint32_t*)xnod_ptr);
	xnod_ptr += sizeof(uint32_t);
	nodes = Z_Malloc(numnodes * sizeof(node_t), PU_LEVEL, NULL);
	for(uint32_t i = 0; i < numnodes; i++)
	{
		node_t *node = nodes + i;
		fixed_t *dst = &node->x;
		uint32_t tmp;

		for(uint32_t j = 0; j < 12; j++)
		{
			*dst++ = (fixed_t)*((int16_t*)xnod_ptr) << FRACBITS;
			xnod_ptr += sizeof(int16_t);
		}

		tmp = *((uint32_t*)xnod_ptr);
		xnod_ptr += sizeof(uint32_t);
		node->children[0] = tmp | (tmp >> 16);

		tmp = *((uint32_t*)xnod_ptr);
		xnod_ptr += sizeof(uint32_t);
		node->children[1] = tmp | (tmp >> 16);
	}

	Z_Free(xnod_data);

extra_ssec:
	// allocate extra subsector info
	// NOTE: this should be part of 'subsector_t' but extending struct size in EXE hack is ... tedious
	e_subsectors = Z_Malloc(numsubsectors * sizeof(subsector_extra_t), PU_LEVEL, NULL);
	memset(e_subsectors, 0, numsubsectors * sizeof(subsector_extra_t));
}

//
// map loading

uint32_t map_check_lump(int32_t lump)
{
	if(lump < 0)
		return 0;

	for(uint32_t i = ML_THINGS; i < ML_BEHAVIOR; i++)
	{
		lump++;
		if(lump >= numlumps)
			return 0;
		if(lumpinfo[lump].wame != map_wame_check[i-1])
			return 0;
	}

	lump++;

	if(lump < numlumps && lumpinfo[lump].wame == map_wame_check[ML_BEHAVIOR-1])
		return MAP_FORMAT_HEXEN;

	return MAP_FORMAT_DOOM;
}

static inline void parse_sectors()
{
	sector_extra_t *se;

	se = Z_Malloc(numsectors * sizeof(sector_extra_t), PU_LEVEL, NULL);
	memset(se, 0, numsectors * sizeof(sector_extra_t));

	for(uint32_t i = 0; i < numsectors; i++, se++)
	{
		sector_t *sec = sectors + i;
		uint32_t plink_count = 0;

		sec->extra = se;
		sec->exfloor = NULL;
		sec->exceiling = NULL;
		sec->sndseq = 255;
		sec->ed3_multiple = 0;
		sec->e3d_origin = 0;

		if(sec->lightlevel >= 384)
			sec->lightlevel = 383;
		if(sec->lightlevel < 0)
			sec->lightlevel = 0;

		se->color = 0x0FFF;

		switch(sec->special & 255)
		{
			case 82: // dDamage_LavaWimpy
				se->damage.amount = 5;
				se->damage.tics = 16;
				se->damage.type = DAMAGE_FIRE | 0x80;
			break;
			case 83: // dDamage_LavaHefty
				se->damage.amount = 8;
				se->damage.tics = 16;
				se->damage.type = DAMAGE_FIRE | 0x80;
			break;
			case 85: // hDamage_Sludge
				se->damage.amount = 4;
				se->damage.tics = 32;
				se->damage.type = DAMAGE_SLIME;
			break;
			case 115: // Damage_InstantDeath
				se->damage.amount = 1;
				se->damage.tics = 1;
				se->damage.type = DAMAGE_INSTANT;
			break;
			case 196: // Sector_Heal
				se->damage.amount = -1;
				se->damage.tics = 32;
			break;
		}

		switch(sec->special & (256 | 512))
		{
			case 256:
				se->damage.amount = 5;
				se->damage.tics = 32;
				se->damage.type = DAMAGE_SLIME;
			break;
			case 512:
				se->damage.amount = 10;
				se->damage.tics = 32;
				se->damage.type = DAMAGE_SLIME;
			break;
			case 256 | 512:
				se->damage.amount = 20;
				se->damage.tics = 32;
				se->damage.type = DAMAGE_SLIME;
				se->damage.leak = 5;
			break;
		}

		M_ClearBox(se->bbox);
		for(uint32_t j = 0; j < sec->linecount; j++)
		{
			line_t *li = sec->lines[j];

			// should this be done in 'P_GroupLines'?
			M_AddToBox(se->bbox, li->v1->x, li->v1->y);
			M_AddToBox(se->bbox, li->v2->x, li->v2->y);

			if(map_format != MAP_FORMAT_DOOM)
			{
				// check for plane links
				if(	li->frontsector == sec &&
					li->special == 51 // Sector_SetLink
				){
					if(li->arg0 || !li->arg1 || li->arg1 == li->frontsector->tag || !li->arg3 || li->arg3 & 0xFC)
						engine_error("MAP", "Invalid use of 'Sector_SetLink'!");

					for(uint32_t k = 0; k < numsectors; k++)
					{
						if(sectors[k].tag == li->arg1)
							plink_count++;
					}
				}
			}
		}

		if(plink_count)
		{
			plane_link_t *plink;

			plink_count++;
			plink = Z_Malloc(plink_count * sizeof(plane_link_t), PU_LEVEL, NULL);
			se->plink = plink;

			for(uint32_t j = 0; j < sec->linecount; j++)
			{
				line_t *li = sec->lines[j];

				if(	li->frontsector == sec &&
					li->special == 51 // Sector_SetLink
				){
					for(uint32_t k = 0; k < numsectors; k++)
					{
						if(sectors[k].tag == li->arg1)
						{
							plink->target = sectors + k;
							plink->use_ceiling = li->arg2 > 0;
							plink->link_floor = li->arg3 & 1;
							plink->link_ceiling = li->arg3 & 2;
							plink++;
						}
					}
					li->special = 0;
					li->arg0 = 0;
					li->args = 0;
				}
			}

			plink->target = NULL;
		}
	}
}

static inline void deathmatch_spawn()
{
	if(!deathmatch)
		return;

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		if(playeringame[i])
		{
			players[i].mo = NULL;
			G_DeathMatchSpawnPlayer(i);
		}
	}
}

static inline void P_LoadThings(int32_t lump)
{
	void *buff;
	uint32_t count;

	// spawn all things
	if(map_format == MAP_FORMAT_DOOM)
	{
		mapthing_t *mt;

		buff = W_CacheLumpNum(lump, PU_STATIC);
		count = lumpinfo[lump].size / sizeof(mapthing_t);
		mt = buff;

		for(uint32_t i = 0; i < count; i++)
			spawn_map_thing(NULL, mt + i);
	} else
	{
		map_thinghex_t *mt;

		buff = W_CacheLumpNum(lump, PU_STATIC);
		count = lumpinfo[lump].size / sizeof(map_thinghex_t);
		mt = buff;

		for(uint32_t i = 0; i < count; i++)
			spawn_map_thing(mt + i, NULL);
	}

	Z_Free(buff);
}

static inline void P_LoadLineDefs(int lump)
{
	int nl;
	line_t *ln;
	map_linehex_t *ml;
	void *buff;

	if(map_format == MAP_FORMAT_DOOM)
	{
		doom_LoadLineDefs(lump);
		return;
	}

	nl = lumpinfo[lump].size / sizeof(map_linehex_t);
	ln = Z_Malloc(nl * sizeof(line_t), PU_LEVEL, NULL);
	buff = W_CacheLumpNum(lump, PU_STATIC);
	ml = buff;

	numlines = nl;
	lines = ln;

	for(uint32_t i = 0; i < nl; i++, ln++, ml++)
	{
		vertex_t *v1 = vertexes + ml->v1;
		vertex_t *v2 = vertexes + ml->v2;

		ln->v1 = v1;
		ln->v2 = v2;
		ln->dx = v2->x - v1->x;
		ln->dy = v2->y - v1->y;
		ln->flags = ml->flags;
		ln->special = ml->special;
		ln->iflags = 0;
		ln->arg0 = ml->arg0;
		ln->args = ml->args;
		ln->sidenum[0] = ml->sidenum[0];
		ln->sidenum[1] = ml->sidenum[1];
		ln->validcount = 0;
		ln->e3d_tag = 0;

		if(ln->special == 121) // Line_SetIdentification
		{
			if(ln->arg1 & 32) // midtex3d
				ln->iflags |= MLI_3D_MIDTEX;
			ln->special = 0;
			ln->id = ln->arg0;
			ln->arg0 = 0;
			ln->args = 0;
		} else
			ln->id = 0;

		if(v1->x < v2->x)
		{
			ln->bbox[BOXLEFT] = v1->x;
			ln->bbox[BOXRIGHT] = v2->x;
		} else
		{
			ln->bbox[BOXLEFT] = v2->x;
			ln->bbox[BOXRIGHT] = v1->x;
		}
		if(v1->y < v2->y)
		{
			ln->bbox[BOXBOTTOM] = v1->y;
			ln->bbox[BOXTOP] = v2->y;
		} else
		{
			ln->bbox[BOXBOTTOM] = v2->y;
			ln->bbox[BOXTOP] = v1->y;
		}

		if(!ln->dx)
			ln->slopetype = ST_VERTICAL;
		else
		if(!ln->dy)
			ln->slopetype = ST_HORIZONTAL;
		else
		{
			if(FixedDiv(ln->dy, ln->dx) > 0)
				ln->slopetype = ST_POSITIVE;
			else
				ln->slopetype = ST_NEGATIVE;
		}

		if(ln->sidenum[0] != 0xFFFF)
			ln->frontsector = sides[ln->sidenum[0]].sector;
		else
			ln->frontsector = NULL;

		if(ln->sidenum[1] != 0xFFFF)
			ln->backsector = sides[ln->sidenum[1]].sector;
		else
			ln->backsector = NULL;
	}

	Z_Free(buff);
}

static inline void P_LoadVertexes(int32_t lump)
{
	mapvertex_t *mv;
	vertex_t *vtx;
	void *data;

	if(!numvertexes)
		numvertexes = W_LumpLength(lump) / sizeof(mapvertex_t);

	vertexes = Z_Malloc(numvertexes * sizeof(vertex_t), PU_LEVEL, NULL);
	data = W_CacheLumpNum(lump, PU_STATIC);

	mv = data;
	vtx = vertexes;

	for(uint32_t i = 0; i < numvertexes; i++)
	{
		vtx->x = (fixed_t)mv->x << FRACBITS;
		vtx->y = (fixed_t)mv->y << FRACBITS;
		vtx++;
		mv++;
	}

	Z_Free(data);
}

uint32_t map_load_setup(uint32_t new_game)
{
	map_cluster_t *cluster;
	uint32_t cache;

	if(paused)
	{
		paused = 0;
		S_ResumeSound();
	}

	if(gameskill > sk_nightmare)
		gameskill = sk_nightmare;

	spec_autosave = 0;
	totalkills = 0;
	totalitems = 0;
	totalsecret = 0;
	think_freeze_mode = 0;

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		players[i].killcount = 0;
		players[i].itemcount = 0;
		players[i].secretcount = 0;
	}

	// reset stuff
	mobj_netid = 0;
	leveltime = 0;
	viewactive = 1;
	automapactive = 0;
	am_lastlevel = -1;
	respawnmonsters = gameskill == sk_nightmare || respawnparm;
	terrain_reset();
	cheat_reset();

	if(is_net_desync)
	{
		is_net_desync = 0;
		prndindex = 0;
	}

	if(gameepisode)
	{
		int32_t lump;

		is_title_map = 0;
		cache = !demoplayback;

		strupr(map_lump.name);

		// loading
		lump = W_CheckNumForName("WILOADIN");
		if(lump >= 0)
		{
			patch_t *patch = W_CacheLumpNum(lump, PU_CACHE);
			vesa_copy();
			V_DrawPatchDirect(0, 0, patch);
			vesa_update();
		}
	} else
	{
		// titlemap can be visited by other means
		// so this has to be different
		map_lump.wame = 0x50414D454C544954; // TITLEMAP
		is_title_map = 1;
		demoplayback = 0;
		netgame = 0;
		cache = 0;
		playeringame[0] = 1;
		for(uint32_t i = 1; i < MAXPLAYERS; i++)
			playeringame[i] = 0;
		consoleplayer = 0;
		displayplayer = 0;
	}

	// reset player aim cache
	act_cc_tick = 0xFFFFFFFF;

	// stop sounds
	S_Start();

	// free level memory
	Z_FreeTags(PU_LEVEL, PU_PURGELEVEL-1);
	zone_info();

	// find map lump
	map_lump_idx = W_CheckNumForName(map_lump.name);
	map_format = map_check_lump(map_lump_idx);
	if(!map_format)
		goto map_load_error;
	doom_printf("[MAP] %s map format\n", map_format == MAP_FORMAT_DOOM ? "Doom" : "Hexen");

	// setup level info
	map_level_info = map_get_info(map_lump_idx);

	// hack for original levels
	// to keep 'A_BossDeath' working properly
	if(map_level_info->levelhack)
	{
		if(map_level_info->levelhack & 0x80)
		{
			fake_game_mode = 1;
			gameepisode = 1;
			gamemap = map_level_info->levelhack & 0x7F;
		} else
		{
			fake_game_mode = 0;
			gameepisode = map_level_info->levelhack >> 4;
			gamemap = map_level_info->levelhack & 15;
		}
	} else
	{
		gameepisode = 1;
		gamemap = 1;
	}

	if(new_game)
	{
		cluster = map_find_cluster(map_level_info->cluster);
		if(cluster && cluster->flags & CLST_FLAG_HUB)
			saveload_clear_cluster(cluster->idx);
	}

	// music
	start_music(map_level_info->music_level, 1);

	// sky
	skytexture = map_level_info->texture_sky[0];

	// think clear
	P_InitThinkers();
	thcap.prev = &thcap;
	thcap.next = &thcap;

	// poly clear
	poly_reset();

	if(map_format == MAP_FORMAT_DOOM)
	{
		map_start_id = 0;
		map_start_facing = 0;
		utils_install_hooks(patch_doom, 0);
	} else
		utils_install_hooks(patch_hexen, 0);

	// clear player mobjs
	for(uint32_t i = 0; i < MAXPLAYERS; i++)
		players[i].mo = NULL;

	// check for extended nodes
	prepare_nodes(map_lump_idx);

	// level
	P_LoadBlockMap(map_lump_idx + ML_BLOCKMAP);
	P_LoadVertexes(map_lump_idx + ML_VERTEXES);
	P_LoadSectors(map_lump_idx + ML_SECTORS);
	P_LoadSideDefs(map_lump_idx + ML_SIDEDEFS);
	P_LoadLineDefs(map_lump_idx + ML_LINEDEFS);

	// nodes
	load_nodes(map_lump_idx);

	// reject
	if(W_LumpLength(map_lump_idx + ML_REJECT))
		rejectmatrix = W_CacheLumpNum(map_lump_idx + ML_REJECT, PU_LEVEL);
	else
		rejectmatrix = NULL;

	// finalize
	P_GroupLines();

	// extra hacks
	parse_sectors();

	// spawn extra floors
	e3d_create();

	// things
	bodyqueslot = 0;
	deathmatch_p = deathmatchstarts;
	P_LoadThings(map_lump_idx + ML_THINGS);

	// polyobjects
	if(map_format != MAP_FORMAT_DOOM)
		poly_create();

	// multiplayer
	deathmatch_spawn();

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
		if(map_format == MAP_FORMAT_DOOM)
			P_SpawnSpecials();
		// TODO: ZDoom specials

		// texture scrollers
		spawn_line_scroll();

		// check for player starts
		for(uint32_t i = 0; i < MAXPLAYERS; i++)
		{
			if(playeringame[i] && !players[i].mo)
				goto map_load_error;
		}

		// colored light
		render_setup_light_color(0);
	}

	// autosave
	if(!is_title_map && !map_skip_stuff)
		save_auto(new_game);

	// input
	memset(gamekeydown, 0, sizeof(gamekeydown));
	memset(mousebuttons, 0, sizeof(mousebuttons));

	// in the level
	gamestate = GS_LEVEL;
	usergame = !is_title_map && !demoplayback;
	return 0;

map_load_error:

	doom_printf("[Bad map] %s\n", map_lump.name);
	map_skip_stuff = 0;
	if(is_title_map)
	{
		// actually start title
		gameaction = ga_nothing;
		demosequence = -1;
		advancedemo = 1;
	} else
		map_start_title();
	wipegamestate = gamestate;
	M_StartMessage("Requested map is invalid!", NULL, 0);

	return 1;
}

//
// hooks

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
			text = dtxt_PD_BLUEK;
		break;
		case 27:
		case 34:
			if(!pl)
				return 0;
			k0 = 49;
			k1 = 50;
			text = dtxt_PD_YELLOWK;
		break;
		case 28:
		case 33:
			if(!pl)
				return 0;
			k0 = 48;
			k1 = 51;
			text = dtxt_PD_REDK;
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
			text = dtxt_PD_BLUEO;
		break;
		case 134:
		case 135:
			if(!pl)
				return 0;
			k0 = 48;
			k1 = 51;
			text = dtxt_PD_REDO;
		break;
		case 136:
		case 137:
			if(!pl)
				return 0;
			k0 = 49;
			k1 = 50;
			text = dtxt_PD_YELLOWO;
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

	gameaction = ga_nothing;

	lump = W_CheckNumForName("TITLEMAP");
	if(lump < 0)
	{
		demosequence = -1;
		advancedemo = 1;
		return;
	}

	if(gameepisode)
		wipegamestate = -1;
	else
		wipegamestate = GS_LEVEL;

	gameskill = sk_medium;
	fastparm = 0;
	respawnparm = 0;
	nomonsters = 0;
	deathmatch = 0;
	survival = 0;
	gameepisode = 0;
	prndindex = 0;
	netgame = 0;
	netdemo = 0;

	map_next_levelnum = 0;
	map_start_id = 0;
	map_start_facing = 0;

	consoleplayer = 0;
	memset(players, 0, sizeof(player_t) * MAXPLAYERS);
	players[0].state = PST_REBORN;

	memset(playeringame, 0, sizeof(uint32_t) * MAXPLAYERS);
	playeringame[0] = 1;

	map_load_setup(1);
}

//
// default map setup

static void setup_episode(uint32_t start, uint32_t episode, uint32_t secret, uint8_t **names)
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
		info->title_lump = wad_check_lump(text);

		doom_sprintf(text, "SKY%u", episode);
		info->texture_sky[0] = texture_num_get(text);
		info->texture_sky[1] = info->texture_sky[0];

		doom_sprintf(text, dtxt_mus_pfx, S_music[(episode - 1) * 9 + i + 1].name);
		info->music_level = wad_check_lump(text);
		info->music_inter = map_info_unnamed.music_inter;
		info->par_time = pars[info->levelnum + 10];

		if(i == 8)
		{
			info->next_normal = start + secret;
			info->next_secret = start + secret;
		} else
		if(i == 7)
		{
			int32_t victory = MAP_END_CUSTOM_PIC_N;
			info->win_lump[0] = -1;
			switch(episode)
			{
				case 1:
					info->win_lump[0] = W_CheckNumForName(dtxt_help2);
					if(info->win_lump[0] < 0)
						info->win_lump[0] = W_CheckNumForName("CREDIT"); // retail
				break;
				case 2:
					info->win_lump[0] = W_CheckNumForName(dtxt_victory2);
				break;
				case 3:
					victory = MAP_END_BUNNY_SCROLL;
				break;
			}
			info->win_lump[1] = info->win_lump[0];
			info->next_normal = victory;
			info->next_secret = victory;
			info->flags = MAP_FLAG_NO_INTERMISSION;
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
		info->title_lump = wad_check_lump(text);

		doom_sprintf(text, "SKY%u", sky);
		info->texture_sky[0] = texture_num_get(text);
		info->texture_sky[1] = info->texture_sky[0];

		doom_sprintf(text, dtxt_mus_pfx, S_music[start + i + 33].name);
		info->music_level = wad_check_lump(text);
		info->music_inter = map_info_unnamed.music_inter;
		info->par_time = cpars[info->levelnum - 1];

		switch(start + i)
		{
			case MAPD2_MAP15:
				info->next_normal = MAPD2_MAP16;
				info->next_secret = MAPD2_MAP31;
			break;
			case MAPD2_MAP30:
				info->next_normal = MAP_END_DOOM_CAST;
				info->next_secret = MAP_END_DOOM_CAST;
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
// attribute parser

static uint32_t parse_attributes(const map_attr_t *attr_def, void *dest)
{
	const map_attr_t *attr;
	uint8_t *kw;
	uint32_t value;

	kw = tp_get_keyword();
	if(!kw)
		return 1;

	if(kw[0] != '{')
		engine_error("ZMAPINFO", "Expected '%c' found '%s'!", '{', kw);

	while(1)
	{
		// get attribute name
		kw = tp_get_keyword_lc();
		if(!kw)
			return 1;

		if(kw[0] == '}')
			return 0;

		// find this attribute
		attr = attr_def;
		while(attr->name)
		{
			if(!strcmp(attr->name, kw))
				break;
			attr++;
		}

		if(!attr->name)
		{
			if(!strncmp(kw, "compat_", 7)) // ZDoom 'compat_*' hack
				continue;
			engine_error("ZMAPINFO", "Unknown attribute '%s'!", kw);
		}

		if(attr->type != IT_FLAG)
		{
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(kw[0] != '=')
				engine_error("ZMAPINFO", "Expected '%c' found '%s'!", '=', kw);
		}

		switch(attr->type)
		{
			case IT_U8:
				kw = tp_get_keyword();
				if(!kw)
					return 1;
				if(doom_sscanf(kw, "%u", &value) != 1 || value > 255)
					engine_error("ZMAPINFO", "Unable to parse number '%s'!", kw);
				*((uint8_t*)(dest + attr->offset)) = value;
			break;
			case IT_U16:
				kw = tp_get_keyword();
				if(!kw)
					return 1;
				if(doom_sscanf(kw, "%u", &value) != 1 || value > 65535)
					engine_error("ZMAPINFO", "Unable to parse number '%s'!", kw);
				*((uint16_t*)(dest + attr->offset)) = value;
			break;
			case IT_MUSIC:
			case IT_PATCH:
				kw = tp_get_keyword();
				if(!kw)
					return 1;
				// it would be nice to check format here ...
				*((int32_t*)(dest + attr->offset)) = wad_check_lump(kw);
			break;
			case IT_FLAT_LUMP:
				kw = tp_get_keyword();
				if(!kw)
					return 1;
				*((int32_t*)(dest + attr->offset)) = flatlump[flat_num_get(kw)];
			break;
			case IT_TEXTURE:
				kw = tp_get_keyword();
				if(!kw)
					return 1;
				*((uint16_t*)(dest + attr->offset)) = texture_num_get(kw);
			break;
			case IT_MAP:
			{
				uint16_t next;

				kw = tp_get_keyword_lc();
				if(!kw)
					return 1;

				if(!strcmp("endpic", kw))
				{
					map_level_t *info = dest;
					uint32_t is_secret = attr->offset == offsetof(map_level_t, next_secret);

					kw = tp_get_keyword_lc();
					if(!kw)
						return 1;
					if(kw[0] != ',')
						engine_error("ZMAPINFO", "Expected '%c' found '%s'!", ',', kw);

					kw = tp_get_keyword_lc();
					if(!kw)
						return 1;

					next = is_secret ? MAP_END_CUSTOM_PIC_S : MAP_END_CUSTOM_PIC_N;
					info->win_lump[is_secret] = wad_check_lump(kw);
				} else
				if(!strcmp("endgamec", kw))
				{
					next = MAP_END_DOOM_CAST;
				} else
				if(!strcmp("endgame3", kw))
				{
					next = MAP_END_BUNNY_SCROLL;
				} else
				if(!strcmp("endtitle", kw))
				{
					next = MAP_END_TO_TITLE;
				} else
				{
					int32_t lump;
					map_level_t *info;

					info = map_get_info(wad_check_lump(kw));
					if(info == &map_info_unnamed)
						next = MAP_END_TO_TITLE;
					else
						next = info - map_info;
				}

				*((uint16_t*)(dest + attr->offset)) = next;
			}
			break;
			case IT_TEXT_CHAIN:
			{
				// NOTE: there's no padding for 32bits
				uint8_t *base_ptr = dec_es_ptr;
				uint8_t *newline = NULL;

				while(1)
				{
					uint8_t *text;
					uint32_t len;

					// text line
					kw = tp_get_keyword();
					if(!kw)
						return 1;

					if(newline)
						*newline = '\n';

					len = strlen(kw) + 1;
					text = dec_es_alloc(len);
					strcpy(text, kw);

					newline = text + len - 1;

					// check for next line
					kw = tp_get_keyword();
					if(!kw)
						return 1;

					if(kw[0] != ',')
					{
						tp_push_keyword(kw);
						break;
					}
				}

				*((uint8_t**)(dest + attr->offset)) = base_ptr;
			}
			break;
			case IT_FLAG:
				*((uint16_t*)(dest + attr->offset)) |= attr->flag;
			break;
		}
	}
}

//
// LOCKDEFS

static uint16_t *lockdefs_add_text(uint16_t *bptr, uint16_t *end, uint8_t *text, uint16_t code)
{
	uint32_t len;

	len = strlen(text) + 2; // NUL + align
	len /= 2; // uint16_t

	if(bptr + len + 1 > end)
		return NULL;

	*bptr++ = code | len;
	strcpy((uint8_t*)bptr, text);

	return bptr + len;
}

static void parse_lockdefs(const lockdefs_kw_list_t *kwl)
{
	uint8_t *kw;
	uint32_t id, offs;
	uint16_t buffer[256];
	uint16_t *bptr;
	lockdef_t ld;

	while(1)
	{
		// keyword
		kw = tp_get_keyword_lc();
		if(!kw)
			return;

		if(!strcmp(kw, "clearlocks"))
		{
			lockdefs_size = 0;
			continue;
		}

		if(!strcmp(kw, kwl->lock))
		{
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;

			if(doom_sscanf(kw, "%u", &id) != 1 || !id || id > 255)
				engine_error("LOCKDEFS", "Invalid lock ID '%s'!", kw);

			kw = tp_get_keyword();
			if(!kw)
				goto error_end;

			if(kw[0] != '{')
				engine_error("LOCKDEFS", "Expected '%c' found '%s'!", '{', kw);

			bptr = buffer;
			ld.id = id;
			ld.color = r_color_duplicate;
			ld.sound = 0;

			while(1)
			{
				kw = tp_get_keyword();
				if(!kw)
					goto error_end;

				if(kw[0] == '}')
					break;

				if(!strcasecmp(kw, kwl->message) || !strcasecmp(kw, kwl->remote))
				{
					uint16_t *ret;
					uint16_t code = kw[0] == kwl->message[0] ? KEYLOCK_MESSAGE : KEYLOCK_REMTMSG;

					kw = tp_get_keyword();
					if(!kw)
						goto error_end;

					ret = lockdefs_add_text(bptr, buffer + 256, kw, code);
					if(!ret)
						goto error_overflow;

					bptr = ret;
				} else
				if(kwl->msg_data && (!strcasecmp(kw, kwl->msg_data) || !strcasecmp(kw, kwl->rmt_data)))
				{
					uint16_t *ret;
					uint32_t addr;
					uint16_t code = kw[0] == kwl->msg_data[0] ? KEYLOCK_MESSAGE : KEYLOCK_REMTMSG;

					kw = tp_get_keyword();
					if(!kw)
						goto error_end;

					doom_sscanf(kw, "%X", &addr);

					ret = lockdefs_add_text(bptr, buffer + 256, (uint8_t*)addr + doom_data_segment, code);
					if(!ret)
						goto error_overflow;

					bptr = ret;
				} else
				if(!strcasecmp(kw, kwl->mapcolor))
				{
					uint32_t r, g, b;

					kw = tp_get_keyword();
					if(!kw)
						goto error_end;

					if(doom_sscanf(kw, "%u", &r) != 1 || r > 255)
						goto error_value;

					kw = tp_get_keyword();
					if(!kw)
						goto error_end;

					if(doom_sscanf(kw, "%u", &g) != 1 || g > 255)
						goto error_value;

					kw = tp_get_keyword();
					if(!kw)
						goto error_end;

					if(doom_sscanf(kw, "%u", &b) != 1 || b > 255)
						goto error_value;

					ld.color = r_find_color(r, g, b);
				} else
				if(kwl->sound && !strcasecmp(kw, kwl->sound))
				{
					kw = tp_get_keyword();
					if(!kw)
						goto error_end;

					ld.sound = sfx_by_name(kw);
				} else
				if(!strcasecmp(kw, kwl->any))
				{
					uint16_t *value = bptr++;

					if(bptr >= buffer + 256)
						goto error_overflow;

					kw = tp_get_keyword();
					if(!kw)
						goto error_end;

					if(kw[0] != '{')
						engine_error("LOCKDEFS", "Expected '%c' found '%s'!", '{', kw);

					*value = KEYLOCK_KEYLIST;
					while(1)
					{
						int32_t type;

						kw = tp_get_keyword();
						if(!kw)
							goto error_end;

						if(kw[0] == '}')
						{
							*value = KEYLOCK_KEYLIST | ((bptr - value) - 1);
							break;
						}

						if(bptr >= buffer + 256)
							goto error_overflow;

						type = mobj_check_type(tp_hash64(kw));
						if(type < 0 || mobjinfo[type].extra_type != ETYPE_KEY)
							goto error_type;

						*bptr++ = type;
					}
				} else
				{
					int32_t type;

					if(bptr + 2 >= buffer + 256)
						goto error_overflow;

					type = mobj_check_type(tp_hash64(kw));
					if(type < 0 || mobjinfo[type].extra_type != ETYPE_KEY)
						goto error_type;

					*bptr++ = KEYLOCK_KEYLIST | 1;
					*bptr++ = type;
				}
			}

			if(bptr >= buffer + 256)
				goto error_overflow;
			*bptr++ = 0;

			id = (bptr - buffer) * sizeof(uint16_t);
			ld.size = sizeof(lockdef_t) + id;

			offs = lockdefs_size;
			lockdefs_size += ld.size;
			if(lockdefs_size < lockdefs_max)
			{
				lockdefs_max += LOCKDEFS_DEF_SIZE;
				lockdefs = ldr_realloc(lockdefs, lockdefs_max);
			}

			memcpy(lockdefs + offs, &ld, sizeof(lockdef_t));
			offs += sizeof(lockdef_t);
			memcpy(lockdefs + offs, buffer, id);

			continue;
		}

		break;
	}

	engine_error("LOCKDEFS", "Unknown keyword '%s'!", kw);
error_overflow:
	engine_error("LOCKDEFS", "Data overflow in lock %u!", id);
error_end:
	engine_error("LOCKDEFS", "Incomplete definition!");
error_value:
	engine_error("LOCKDEFS", "Bad number '%s'!", kw);
error_type:
	engine_error("LOCKDEFS", "Bad key type '%s'!", kw);
}

//
// callbacks

static void cb_count_stuff(lumpinfo_t *li)
{
	uint8_t *kw;

	tp_load_lump(li);
	tp_enable_math = 1;

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return;

		if(!strcmp("map", kw))
		{
			map_level_t *info;
			int32_t lump;

			// map lump
			kw = tp_get_keyword();
			if(!kw)
				break;

			lump = wad_check_lump(kw);
			if(lump >= 0)
			{
				// check for replacement
				info = map_get_info(lump);
				if(info == &map_info_unnamed)
				{
					// add new entry
					uint32_t idx = num_maps++;
					map_info = ldr_realloc(map_info, num_maps * sizeof(map_level_t));
					info = map_info + idx;
				}

				// set defaults
				*info = map_info_default;

				// set lump
				info->lump = lump;
			}

			// map name
			kw = tp_get_keyword();
			if(!kw)
				break;
skip_block:
			// block entry
			kw = tp_get_keyword();
			if(!kw)
				break;

			if(kw[0] != '{')
				engine_error("ZMAPINFO", "Broken syntax!");

			if(tp_skip_code_block(1))
				break;

			continue;
		}

		if(!strcmp("cluster", kw))
		{
			// cluster number
			kw = tp_get_keyword();
			if(!kw)
				break;

			max_cluster++;

			goto skip_block;
		}

		if(!strcmp("episode", kw))
		{
			// episode map
			kw = tp_get_keyword();
			if(!kw)
				break;

			goto skip_block;
		}

		if(!strcmp("defaultmap", kw))
		{
			// must parse this here
			if(parse_attributes(map_attr, &map_info_default))
				break;
			continue;
		}

		if(!strcmp("clearepisodes", kw))
			continue;

		engine_error("ZMAPINFO", "Unknown block '%s'!", kw);
	}

	engine_error("ZMAPINFO", "Incomplete definition!");
}

static void cb_mapinfo(lumpinfo_t *li)
{
	uint8_t *kw;
	uint8_t *text;
	uint32_t tmp;
	int32_t lump;

	tp_load_lump(li);
	tp_enable_math = 1;

	while(1)
	{
		// keyword
		kw = tp_get_keyword_lc();
		if(!kw)
			return;

		if(!strcmp("map", kw))
		{
			map_level_t *info;
			uint32_t levelnum = 0;

			// map lump
			kw = tp_get_keyword_lc();
			if(!kw)
				break;

			lump = wad_check_lump(kw);

			// parse level number
			if(	kw[0] == 'e' &&
				kw[1] >= '1' && kw[1] <= '9' &&
				kw[2] == 'm' &&
				kw[3] >= '0' && kw[3] <= '9' &&
				kw[4] == 0
			)
				levelnum = (kw[1] - '1') * 10 + (kw[3] - '0');
			else
			if(	kw[0] == 'm' &&
				kw[1] == 'a' &&
				kw[2] == 'p' &&
				kw[3] >= '0' && kw[3] <= '9'
			)
				doom_sscanf(kw + 3, "%u\n", &levelnum);

			// map title
			kw = tp_get_keyword();
			if(!kw)
				break;

			if(lump < 0)
			{
				// skip properties
				kw = tp_get_keyword_lc();
				if(!kw)
					return;

				if(kw[0] != '{')
					engine_error("ZMAPINFO", "Expected '%c' found '%s'!", '{', kw);

				if(tp_skip_code_block(1))
					break;

				continue;
			}

			tmp = strlen(kw);
			tmp += 4;
			tmp &= ~3;

			text = dec_es_alloc(tmp);
			strcpy(text, kw);

			info = map_get_info(lump);
			if(info == &map_info_unnamed)
				engine_error("ZMAPINFO", "Miscounted!");

			info->name = text;

			// get properties
			if(parse_attributes(map_attr, info))
				break;

			// copy to secret exit
			if(info->next_secret == MAP_END_UNDEFINED)
				info->next_secret = info->next_normal;

			continue;
		}

		if(!strcmp("cluster", kw))
		{
			map_cluster_t *clst;

			// cluster number
			kw = tp_get_keyword();
			if(!kw)
				break;

			if(doom_sscanf(kw, "%u", &tmp) != 1 || tmp > 255)
				engine_error("ZMAPINFO", "Unable to parse number '%s'!", kw);

			// check
			if(num_clusters >= max_cluster)
				engine_error("ZMAPINFO", "Miscounted!");

			// set defauls
			clst = map_cluster + num_clusters;
			memset(clst, 0, sizeof(map_cluster_t));
			clst->idx = tmp;
			clst->lump_music = cluster_music;
			clst->lump_flat = flatlump[0]; // this is different than ZDoom

			// get properties
			if(parse_attributes(clst_attr, clst))
				break;

			num_clusters++;

			continue;
		}

		if(!strcmp("defaultmap", kw))
		{
			// block entry
			kw = tp_get_keyword();
			if(!kw)
				break;

			if(kw[0] != '{')
				engine_error("ZMAPINFO", "Broken syntax!");

			if(tp_skip_code_block(1))
				break;

			continue;
		}

		if(!strcmp("episode", kw))
		{
			map_episode_t *epi;

			if(map_episode_count >= MAX_EPISODES)
				engine_error("ZMAPINFO", "Too many episodes defined!");

			epi = map_episode_def + map_episode_count;

			// episode map
			kw = tp_get_keyword();
			if(!kw)
				break;

			lump = wad_check_lump(kw);
			epi->map_lump = lump;

			// get properties
			if(parse_attributes(epi_attr, epi))
				break;

			map_episode_count++;

			continue;
		}

		if(!strcmp("clearepisodes", kw))
		{
			map_episode_count = 0;
			continue;
		}

		engine_error("ZMAPINFO", "Unknown keyword '%s'!", kw);
	}

	engine_error("ZMAPINFO", "Incomplete definition!");
}

static void cb_lockdefs(lumpinfo_t *li)
{
	tp_load_lump(li);
	parse_lockdefs(lockdefs_kw_list + 1);
}

//
// API

void init_map()
{
	uint8_t text[16];
	const def_cluster_t *def_clusters;
	int32_t temp;

	dec_es_ptr = EXTRA_STORAGE_PTR;

	doom_printf("[ACE] init MAPs\n");
	ldr_alloc_message = "Map and game info";

	// think_clear
	thcap.prev = &thcap;
	thcap.next = &thcap;
	thinkercap.prev = &thinkercap;
	thinkercap.next = &thinkercap;

	//
	// PASS 1

	// find some GFX
	patch_finshed = wad_get_lump(dtxt_wif);
	patch_entering = wad_get_lump(dtxt_wienter);

	// default clusters and maps
	if(gamemode)
	{
		doom_sprintf(text, dtxt_mus_pfx, dtxt_dm2int);
		map_info_unnamed.music_inter = wad_check_lump(text);
		doom_sprintf(text, dtxt_mus_pfx, dtxt_read_m);
		num_clusters = D2_CLUSTER_COUNT;
		def_clusters = d2_clusters;
		num_maps = NUM_D2_MAPS;
	} else
	{
		doom_sprintf(text, dtxt_mus_pfx, dtxt_inter);
		map_info_unnamed.music_inter = wad_check_lump(text);
		doom_sprintf(text, dtxt_mus_pfx, dtxt_victor);
		num_clusters = D1_CLUSTER_COUNT;
		def_clusters = d1_clusters;
		num_maps = NUM_D1_MAPS;
	}

	// defaults
	cluster_music = wad_check_lump(text);
	max_cluster = num_clusters;

	map_info_unnamed.texture_sky[0] = texture_num_get("SKY1");
	map_info_unnamed.texture_sky[1] = map_info_unnamed.texture_sky[0];

	map_info_default = map_info_unnamed;
	map_info_default.texture_sky[0] = numtextures - 1; // invalid texture
	map_info_default.texture_sky[1] = map_info_default.texture_sky[0];
	map_info_default.next_secret = MAP_END_UNDEFINED;

	// allocate maps
	map_info = ldr_malloc(num_maps * sizeof(map_level_t));
	memset(map_info, 0, num_maps * sizeof(map_level_t));

	// prepare default maps
	if(gamemode)
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
		// episodes
		map_episode_count = 1;
		map_episode_def[0].map_lump = wad_check_lump("MAP01");
		map_episode_def[0].title_lump = wad_check_lump("M_EPID2");
	} else
	{
		// episode 1
		setup_episode(MAPD1_E1M1, 1, 3, mapnames + MAPD1_E1M1);
		// episode 2
		setup_episode(MAPD1_E2M1, 2, 5, mapnames + MAPD1_E2M1);
		// episode 3
		setup_episode(MAPD1_E3M1, 3, 6, mapnames + MAPD1_E3M1);
		// this is not the same as in ZDoom, but cast is broken in D1
		map_info_unnamed.next_normal = MAP_END_TO_TITLE;
		map_info_unnamed.next_secret = MAP_END_TO_TITLE;
		// episodes
		map_episode_count = 3;
		map_episode_def[0].map_lump = wad_check_lump("E1M1");
		map_episode_def[0].title_lump = wad_check_lump(dtxt_m_epi1);
		map_episode_def[1].map_lump = wad_check_lump("E2M1");
		map_episode_def[1].title_lump = wad_check_lump(dtxt_m_epi2);
		map_episode_def[2].map_lump = wad_check_lump("E3M1");
		map_episode_def[2].title_lump = wad_check_lump(dtxt_m_epi3);
	}

	// count clusters, add map names
	wad_handle_lump("ZMAPINFO", cb_count_stuff);

	//
	// PASS 2

	// allocate clusters
	map_cluster = ldr_malloc(max_cluster * sizeof(map_cluster_t));
	memset(map_cluster, 0, max_cluster * sizeof(map_cluster_t));

	// prepare default clusters
	for(uint32_t i = 0; i < num_clusters; i++)
	{
		map_cluster_t *clst = map_cluster + i;
		const def_cluster_t *defc = def_clusters + i;

		clst->lump_music = cluster_music;
		clst->idx = defc->idx;

		if(defc->type)
			clst->text_leave = defc->text;
		else
			clst->text_enter = defc->text;

		clst->lump_flat = flatlump[flat_num_get(defc->flat)];
	}

	// default, dummy
	map_level_info = &map_info_unnamed;

	// reset extra storage
	dec_es_ptr = EXTRA_STORAGE_PTR;

	// parse maps and clusters
	wad_handle_lump("ZMAPINFO", cb_mapinfo);

	// allocate extra storage
	temp = dec_es_ptr - EXTRA_STORAGE_PTR;
	if(temp)
	{
		void *target = ldr_malloc(temp);

		memcpy(target, EXTRA_STORAGE_PTR, temp);

		// relocate clusters
		for(uint32_t i = 0; i < num_clusters; i++)
		{
			map_cluster_t *clst = map_cluster + i;
			clst->text_leave = dec_reloc_es(target, clst->text_leave);
			clst->text_enter = dec_reloc_es(target, clst->text_enter);
		}

		// relocate maps
		for(uint32_t i = 0; i < num_maps; i++)
		{
			map_level_t *info = map_info + i;
			info->name = dec_reloc_es(target, info->name);
		}
	}

	// prepare episode menu
	if(!map_episode_count)
		engine_error("ZMAPINFO", "No episodes defined!");
	menu_setup_episodes();

	// prepare LOCKDEFS memory
	lockdefs = ldr_malloc(LOCKDEFS_DEF_SIZE);

	// parse LOCKDEFS (internal)
	tp_use_text(engine_lockdefs);
	parse_lockdefs(lockdefs_kw_list + 0);

	// parse LOCKDEFS
	wad_handle_lump("LOCKDEFS", cb_lockdefs);
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
static void do_new_game()
{
	gameaction = ga_nothing;

	demoplayback = 0;
	netdemo = 0;
	is_net_desync = 0;

	if(!netgame)
	{
		deathmatch = 0;
		respawnparm = 0;
		fastparm = 0;
		nomonsters = 0;

		consoleplayer = 0;
		for(uint32_t i = 0; i < MAXPLAYERS; i++)
			playeringame[i] = 0;
		playeringame[0] = 1;
	}

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
		players[i].state = PST_REBORN;

	gameskill = d_skill;
	gameepisode = 1;
	wipegamestate = -1;

	map_next_levelnum = 0;
	map_start_id = 0;
	map_start_facing = 0;

	map_load_setup(1);
}

__attribute((regparm(2),no_caller_saved_registers))
static void do_autostart_game()
{
	for(uint32_t i = 0; i < MAXPLAYERS; i++)
		players[i].state = PST_REBORN;

	if(!map_lump.name[0])
	{
		if(!old_game_mode)
		{
			if(startmap > 9)
				startmap = 1;
			doom_sprintf(map_lump.name, "E%uM%u", startepisode, startmap);
		} else
		{
			if(startmap > 99)
				startmap = 1;
			doom_sprintf(map_lump.name, "MAP%02u", startmap);
		}
	}

	gameskill = startskill;
	gameepisode = 1;
	wipegamestate = -1;

	map_next_levelnum = 0;
	map_start_id = 0;
	map_start_facing = 0;

	map_load_setup(1);
}

__attribute((regparm(2),no_caller_saved_registers))
static void set_world_done()
{
	int32_t music_lump = -1;

	gameaction = ga_worlddone;

	// mark secret level visited
	if(secretexit)
	{
		for(uint32_t i = 0; i < MAXPLAYERS; i++)
			players[i].didsecret = 1;
	}

	finaletext = NULL;

	if(old_cl != new_cl)
	{
		if(new_cl && new_cl->text_enter)
		{
			finaletext = new_cl->text_enter;
			finaleflat = new_cl->lump_flat;
			music_lump = new_cl->lump_music;
		} else
		if(old_cl)
		{
			finaletext = old_cl->text_leave;
			finaleflat = old_cl->lump_flat;
			music_lump = old_cl->lump_music;
		}
		// check for cluster-hub
		if(new_cl && new_cl->flags & CLST_FLAG_HUB)
			saveload_clear_cluster(new_cl->idx);
	} else
	if(old_cl && old_cl->flags & CLST_FLAG_HUB)
		save_hub_level();

	// finale?
	if(finaletext || map_next_num > MAP_END_TO_TITLE)
	{
		gameaction = ga_nothing;
		gamestate = GS_FINALE;
		viewactive = 0;
		automapactive = 0;
		finalecount = 0;

		if(!finaletext)
			finalestage = 1;
		else
			finalestage = 0;

		switch(map_next_num)
		{
			case MAP_END_BUNNY_SCROLL:
			{
				uint8_t text[12];
				fake_game_mode = 0;
				gameepisode = 3;
				doom_sprintf(text, dtxt_mus_pfx, S_music[30].name);
				music_lump = W_CheckNumForName(text);
			}
			break;
			case MAP_END_DOOM_CAST:
				fake_game_mode = 1;
				gamemap = 30;
				if(!finaletext)
				{
					F_StartCast();
					music_lump = -2;
				}
			break;
			case MAP_END_CUSTOM_PIC_N:
			case MAP_END_CUSTOM_PIC_S:
			{
				int32_t lump;
				fake_game_mode = 0;
				gameepisode = 1;
				lump = map_level_info->win_lump[map_next_num == MAP_END_CUSTOM_PIC_S];
				if(lump < 0)
					lump = W_GetNumForName(dtxt_interpic);
				victory_patch = W_CacheLumpNum(lump, PU_CACHE);
			}
			break;
			default:
				fake_game_mode = 1;
				gamemap = 1;
			break;
		}
		if(music_lump >= -1) // -1 means STOP
			start_music(music_lump, 1); // technically, 'mus_bunny' should not loop ...
	}
}

__attribute((regparm(2),no_caller_saved_registers))
static void do_completed()
{
	uint16_t next;

	gameaction = ga_nothing;

	// leave automap
	if(automapactive)
		AM_Stop();

	// check next level
	if(map_next_levelnum)
	{
		int32_t i;
		for(i = num_maps - 1; i >= 0; i--)
		{
			if(map_info[i].levelnum == map_next_levelnum)
			{
				next = i;
				break;
			}
		}
		if(i < 0)
			map_next_info = NULL;
		map_next_levelnum = 0;
	} else
	if(secretexit)
		next = map_level_info->next_secret;
	else
		next = map_level_info->next_normal;

	if(next < num_maps)
		map_next_info = map_info + next;
	else
		map_next_info = NULL;
	map_next_num = next;

	// check for cluser change
	old_cl = map_find_cluster(map_level_info->cluster);
	if(map_next_info)
		new_cl = map_find_cluster(map_next_info->cluster);
	else
		new_cl = NULL;

	// clean-up players
	for(uint32_t i = 0; i < MAXPLAYERS; i++)
		player_finish(players + i, !old_cl || old_cl != new_cl || !(old_cl->flags & CLST_FLAG_HUB));

	// intermission
	gamestate = GS_INTERMISSION;
	viewactive = 0;

	// endgame

	if(	map_level_info->flags & MAP_FLAG_NO_INTERMISSION ||
		(old_cl && old_cl == new_cl && old_cl->flags & CLST_FLAG_HUB)
	){
		set_world_done();
		return;
	}

	// setup intermission
	wminfo.didsecret = players[consoleplayer].didsecret;
	wminfo.epsd = gameepisode - 1;
	wminfo.last = gamemap - 1;
	wminfo.next = gamemap;
	wminfo.maxkills = totalkills;
	wminfo.maxitems = totalitems;
	wminfo.maxsecret = totalsecret;
	wminfo.maxfrags = 0;
	wminfo.partime = map_level_info->par_time * 35;
	wminfo.pnum = consoleplayer;

	if(map_next_info && map_next_info->levelhack && map_next_info->levelhack < 0x80)
	{
		fake_game_mode = 0;
		wminfo.next = (map_next_info->levelhack & 15) - 1;
	} else
	{
		fake_game_mode = 1;
		if(!map_next_info)
			// use of 30 disables 'entering' text
			wminfo.next = 30;
	}

	for(uint32_t i = 0; i < MAXPLAYERS; i++)
	{
		wminfo.plyr[i].in = playeringame[i];
		wminfo.plyr[i].skills = players[i].killcount;
		wminfo.plyr[i].sitems = players[i].itemcount;
		wminfo.plyr[i].ssecret = players[i].secretcount;
		wminfo.plyr[i].stime = leveltime;
		// TODO: frags - these are no longer in player structure
	}

	WI_Start(&wminfo);
}

__attribute((regparm(2),no_caller_saved_registers))
static void do_world_done()
{
	map_cluster_t *cluster;

	if(!map_next_info)
	{
		// there's no next level
		map_start_title();
		return;
	}

	gameaction = ga_nothing;
	map_lump.wame = lumpinfo[map_next_info->lump].wame;

	cluster = map_find_cluster(map_next_info->cluster);
	if(!cluster || !(cluster->flags & CLST_FLAG_HUB) || load_hub_level())
		map_load_setup(0);
}

__attribute((regparm(2),no_caller_saved_registers))
static void music_intermission()
{
	start_music(map_level_info->music_inter, 1);
}

__attribute((regparm(2),no_caller_saved_registers))
static void draw_finished()
{
	int32_t y = WI_TITLEY;
	patch_t *patch;

	if(map_level_info->title_lump >= 0)
	{
		patch = W_CacheLumpNum(map_level_info->title_lump, PU_CACHE);
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

	if(map_next_info && map_next_info->title_lump >= 0)
	{
		y += (5 * patch->height) / 4;

		patch = W_CacheLumpNum(map_next_info->title_lump, PU_CACHE);
		V_DrawPatch((SCREENWIDTH - patch->width) / 2, y, FB, patch);
	}
}

//
// line iterator

__attribute((regparm(3),no_caller_saved_registers)) // three!
uint32_t P_BlockLinesIterator(int32_t x, int32_t y, line_func_t func)
{
	uint16_t *list;
	uint32_t idx;

	if(x < 0)
		return 1;

	if(y < 0)
		return 1;

	if(x >= bmapwidth)
		return 1;

	if(y >= bmapheight)
		return 1;

	idx = y * bmapwidth + x;

	list = blockmaplump + blockmap[idx];

	// apparently, first entry is always zero and does not represent line IDX
	if(!*list)
		list++;

	// go trough lines
	for( ; *list != 0xFFFF; list++)
	{
		line_t *ld = lines + *list;

		if(ld->validcount == validcount)
			continue;

		ld->validcount = validcount;

		if(!func(ld))
			return 0;
	}

	// go trough polyobjects
	if(polybmap && polybmap[idx] && !poly_BlockLinesIterator(x, y, func))
		return 0;

	return 1;
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
static void hit_slide_line(line_t *ln)
{
	if(ln->slopetype == ST_HORIZONTAL)
	{
		tmymove = 0;
		return;
	}

	if(ln->slopetype == ST_VERTICAL)
	{
		tmxmove = 0;
		return;
	}

	P_HitSlideLine(ln);
}

//
// hooks

static const hook_t patch_doom[] =
{
	// restore line special handlers
	{0x0002B340, CODE_HOOK | HOOK_CALL_DOOM, 0x0002F500},
	{0x00027286, CODE_HOOK | HOOK_CALL_DOOM, 0x00030710},
	{0x0002BCFE, CODE_HOOK | HOOK_CALL_DOOM, 0x00030710},
	// terminator
	{0}
};

static const hook_t patch_hexen[] =
{
	// replace line special handlers
	{0x0002B340, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)spec_line_cross},
	{0x00027286, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)spec_line_use}, // by monster
	{0x0002BCFE, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)spec_line_use}, // by player
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
	{0x000200A9, CODE_HOOK | HOOK_UINT8, 0xC0},
	{0x000200AA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)map_load_setup},
	// disable line scroller and stuff cleanup in 'P_SpawnSpecials'
	{0x00030155, CODE_HOOK | HOOK_JMP_DOOM, 0x000301E1},
	// replace 'P_BlockLinesIterator'
	{0x0002C520, CODE_HOOK | HOOK_UINT32, 0xD98951},
	{0x0002C523, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_BlockLinesIterator},
	{0x0002C528, CODE_HOOK | HOOK_UINT16, 0xC359},
	// replace 'P_AimLineAttack'
	{0x0002BB80, CODE_HOOK | HOOK_UINT32, 0xD98951},
	{0x0002BB83, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_AimLineAttack},
	{0x0002BB88, CODE_HOOK | HOOK_UINT16, 0xC359},
	// replace size of 'slopetype' in 'P_BoxOnLineSide'
	{0x0002C0DB, CODE_HOOK | HOOK_UINT32, 0x2A42B60F},
	{0x0002C0DF, CODE_HOOK | HOOK_UINT16, 0x033C},
	// replace size of 'slopetype' in 'P_HitSlideLine'
	{0x0002B3D7, CODE_HOOK | HOOK_UINT16, 0x23EB},
	{0x0002B715, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hit_slide_line},
	// replace key checks in 'EV_VerticalDoor'
	{0x00026C85, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)check_door_key},
	{0x00026C8A, CODE_HOOK | HOOK_UINT16, 0xC085},
	{0x00026C8D, CODE_HOOK | HOOK_JMP_DOOM, 0x00026E97},
	{0x00026C8C, CODE_HOOK | HOOK_UINT16, 0x840F},
	{0x00026C92, CODE_HOOK | HOOK_JMP_DOOM, 0x00026D3E},
	// replace calls to 'EV_DoLockedDoor'
	{0x00030B0E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_obj_key},
	{0x00030E55, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_obj_key},
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
	// update INTERPIC for linear buffer
	{0x0003CABC, CODE_HOOK | HOOK_ABSADDR_DATA, 0x00074FCC},
	{0x0003CED0, CODE_HOOK | HOOK_ABSADDR_DATA, 0x00074FCC},
	{0x0003D9D6, CODE_HOOK | HOOK_ABSADDR_DATA, 0x00074FCC},
	{0x0003E6BA, CODE_HOOK | HOOK_ABSADDR_DATA, 0x00074FCC},
	{0x0003DD00, CODE_HOOK | HOOK_UINT32, 2},
	// replace 'commercial' check in intermission code
	{0x0003C568, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	{0x0003C60E, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	{0x0003C727, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	{0x0003CA80, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	{0x0003CAF0, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	{0x0003CE22, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	{0x0003D4CB, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	{0x0003D911, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	{0x0003DCCB, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	{0x0003DD1B, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	// replace 'commercial' check in finale code
	{0x0001C4A8, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	{0x0001C51C, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	// replace 'commercial' check in 'A_BossDeath'
	{0x000288FA, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	{0x000289A1, CODE_HOOK | HOOK_UINT32, (uint32_t)&fake_game_mode},
	// replace 'commercial' check in 'R_FillBackScreen'
	{0x00034ED2, CODE_HOOK | HOOK_UINT32, (uint32_t)&old_game_mode},
	// replace EP1 end pic in 'F_Drawer'
	{0x0001CFD8, CODE_HOOK | HOOK_UINT8, 0xA1},
	{0x0001CFD9, CODE_HOOK | HOOK_UINT32, (uint32_t)&victory_patch},
	// disable use of 'lnames' in 'WI_loadData' and 'WI_unloadData'
	{0x0003DD23, CODE_HOOK | HOOK_JMP_DOOM, 0x0003DEBF},
	{0x0003DD92, CODE_HOOK | HOOK_JMP_DOOM, 0x0003DDE6},
	{0x0003E15F, CODE_HOOK | HOOK_JMP_DOOM, 0x0003E308},
	{0x0003E247, CODE_HOOK | HOOK_JMP_DOOM, 0x0003E288},
	{0x0003E308, CODE_HOOK | HOOK_UINT16, 0x08EB},
	// automap is fullscreen
	{0x00012C64, DATA_HOOK | HOOK_UINT32, SCREENHEIGHT},
	// fix 'A_Tracer' - make it leveltime based
	{0x00027E2A, CODE_HOOK | HOOK_ABSADDR_DATA, 0x0002CF80},
	// disable snap-to-floor in 'P_Move'
	{0x000272BD, CODE_HOOK | HOOK_UINT16, 0x09EB},
	// replace 'P_CheckSight' on multiple places
	{0x0002708A, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_CheckSight},
	{0x000270AA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_CheckSight},
	{0x000275D8, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_CheckSight},
	{0x000276F0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_CheckSight},
	{0x00027907, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_CheckSight},
	{0x00027B82, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_CheckSight},
	{0x00027BC2, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_CheckSight},
	{0x000282B6, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_CheckSight},
	{0x0002838C, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_CheckSight},
	{0x0002BDC6, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)P_CheckSight},
	// replace 'P_PathTraverse' on multiple places
	{0x0002B5F6, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_SlideMove
	{0x0002B616, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_SlideMove
	{0x0002B636, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_SlideMove
	{0x0002BC89, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_LineAttack
	{0x0002BD55, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_UseLines
	// replace pointers to 'PTR_SlideTraverse' in 'P_SlideMove'
	{0x0002B5DD, CODE_HOOK | HOOK_UINT32, (uint32_t)hs_slide_traverse},
	{0x0002B5FC, CODE_HOOK | HOOK_UINT32, (uint32_t)hs_slide_traverse},
	{0x0002B61C, CODE_HOOK | HOOK_UINT32, (uint32_t)hs_slide_traverse},
	// replace pointers to 'PTR_ShootTraverse' in 'P_SlideMove'
	{0x0002BC52, CODE_HOOK | HOOK_UINT32, (uint32_t)hs_shoot_traverse},
	// enable sliding on things
	{0x0002B5EA, CODE_HOOK | HOOK_UINT8, PT_ADDLINES | PT_ADDTHINGS},
	{0x0002B60F, CODE_HOOK | HOOK_UINT8, PT_ADDLINES | PT_ADDTHINGS},
	{0x0002B62C, CODE_HOOK | HOOK_UINT8, PT_ADDLINES | PT_ADDTHINGS},
};

