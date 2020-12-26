// kgsws' Doom ACE
// Decorate parser.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "sound.h"
#include "textpars.h"
#include "decorate.h"
#include "action.h"
#include "mobj.h"

// extra
#include "d_actornames.h"

#define CUSTOM_STATE_MAX_NAME	16
//#define debug_printf	doom_printf

// TODO: stuff to fix
// weapon selection from ticcmd
// P_DamageMobj - rewrite and remove existing hooks
// P_KillMobj - merge into new P_DamageMobj and remove existing hooks
// special sprite "####" "#" and frame

#define ATTR_MASK_DEFAULT	0x0001
#define ATTR_MASK_INVENTORY	0x0002
#define ATTR_MASK_WEAPON	0x0004
#define ATTR_MASK_PLAYERCLASS	0x0008

typedef struct
{
	uint32_t sprite;
	int32_t frame;
	int32_t tics;
	void *action;
	uint32_t nextstate;
	int32_t misc1;
	int32_t misc2;
} old_state_t;

typedef struct
{
	uint32_t doomednum;
	uint32_t spawnstate;
	int32_t spawnhealth;
	uint32_t seestate;
	uint32_t seesound;
	uint32_t reactiontime;
	uint32_t attacksound;
	uint32_t painstate;
	uint32_t painchance;
	uint32_t painsound;
	uint32_t meleestate;
	uint32_t missilestate;
	uint32_t deathstate;
	uint32_t xdeathstate;
	uint32_t deathsound;
	int32_t speed;
	uint32_t radius;
	uint32_t height;
	uint32_t mass;
	int32_t damage;
	uint32_t activesound;
	uint32_t flags;
	uint32_t raisestate;
} old_mobjinfo_t;

enum
{
	// basic numbers
	PROPTYPE_INT32,
	PROPTYPE_INT16,
	PROPTYPE_INT8,
	// doom special
	PROPTYPE_FIXED,
	// flags special
	PROPTYPE_FLAGCOMBO,
	// drop item list
	PROPTYPE_DROPLIST,
	// state definitions
	PROPTYPE_STATES,
	// sound special
	PROPTYPE_SOUND,
	// strings
	PROPTYPE_STRING,
	// flags
	PROPFLAG_IGNORED = 1,
};

typedef struct
{
	uint8_t *match;
	uint8_t type;
	uint8_t attr_mask;
	uint8_t flags;
	uint32_t offset;
} actor_property_t;

typedef struct
{
	uint8_t *match;
	uint32_t flags;
	uint8_t extra;
} __attribute__((packed)) actor_flag_t;

typedef struct
{
	uint8_t *match;
	uint8_t extra;
} __attribute__((packed)) actor_animation_t;

typedef struct
{
	uint8_t *match;
	uint32_t *count;
	mobjinfo_t *template;
	void *extra_template;
	uint8_t attr_mask;
} actor_parent_t;

typedef struct code_ptr_s
{
	uint8_t *match;
	void *func;
	void *(*parser)(void*, uint8_t*, uint8_t*);
} code_ptr_t;

typedef struct
{
	union
	{
		uint8_t name[CUSTOM_STATE_MAX_NAME];
		uint64_t wame[CUSTOM_STATE_MAX_NAME / sizeof(uint64_t)];
	};
	uint32_t state;
} custom_state_list_t;

typedef struct
{
	uint32_t state;
	void *ptr;
	void *extra;
} wpn_codefix_t;

// keywords
static uint8_t mark_section[] = {'{', '}'};
static uint8_t kw_actor[] = "actor";
static uint8_t kw_goto[] = "goto";
static uint8_t kw_loop[] = "loop";
static uint8_t kw_stop[] = "stop";
static uint8_t kw_wait[] = "wait";
static uint8_t kw_fail[] = "fail";
static uint8_t kw_bright[] = "bright";

static uint8_t kw_clearplayerclasses[] = "clearplayerclasses";
static uint8_t kw_addplayerclass[] = "addplayerclass";

static uint8_t kw_null_sprite[] = {'T', 'N', 'T', '1'}; // not used in parser

//
// new weapon info
static uint8_t wpn_info_idx[NUMWEAPONS] =
{
	0xFF,	// fist - no actor
	0xFF,	// pistol - no actor
	77,	// shotgun
	73,	// chaingun
	75,	// rocket launcher
	76,	// plasma rifle
	72,	// BFG
	74,	// chainsaw
	78,	// super shotgun
};

static dextra_weapon_t wpn_info_extra[NUMWEAPONS] =
{
	{	// (0) FIST
		.type = DECORATE_EXTRA_WEAPON,
		.flags = 0,
		.motype = 0,
		.ammotype = {0xFFFF, 0xFFFF},
		.ammogive = {0, 0},
		.ammouse = {0, 0},
		.pickupsound = sfx_wpnup,
		.upsound = 0,
		.readysound = 0,
		.kickback = 100,
		.selection = 0xFFFF, // TODO
		.ready_state = 2,
		.deselect_state = 3,
		.select_state = 4,
		.fire1_state = 5,
		.flash1_state = 0,
		.message = NULL
	},
	{	// (1) PISTOL
		.type = DECORATE_EXTRA_WEAPON,
		.flags = 0,
		.motype = 0,
		.ammotype = {0xFFFF, 0xFFFF}, // TODO
		.ammogive = {0, 0}, // TODO
		.ammouse = {0, 0}, // TODO
		.pickupsound = sfx_wpnup,
		.upsound = 0,
		.readysound = 0,
		.kickback = 100,
		.selection = 0xFFFF, // TODO
		.ready_state = 10,
		.deselect_state = 11,
		.select_state = 12,
		.fire1_state = 13,
		.flash1_state = 17,
		.message = NULL
	},
	{	// (2) SHOTGUN
		.type = DECORATE_EXTRA_WEAPON,
		.flags = 0,
		.motype = 0,
		.ammotype = {0xFFFF, 0xFFFF}, // TODO
		.ammogive = {0, 0}, // TODO
		.ammouse = {0, 0}, // TODO
		.pickupsound = sfx_wpnup,
		.upsound = 0,
		.readysound = 0,
		.kickback = 100,
		.selection = 0xFFFF, // TODO
		.ready_state = 18,
		.deselect_state = 19,
		.select_state = 20,
		.fire1_state = 21,
		.flash1_state = 30,
		.message = (void*)0x00023104
	},
	{	// (3) CHAINGUN
		.type = DECORATE_EXTRA_WEAPON,
		.flags = 0,
		.motype = 0,
		.ammotype = {0xFFFF, 0xFFFF}, // TODO
		.ammogive = {0, 0}, // TODO
		.ammouse = {0, 0}, // TODO
		.pickupsound = sfx_wpnup,
		.upsound = 0,
		.readysound = 0,
		.kickback = 100,
		.selection = 0xFFFF, // TODO
		.ready_state = 49,
		.deselect_state = 50,
		.select_state = 51,
		.fire1_state = 52,
		.flash1_state = 55,
		.message = (void*)0x00023094
	},
	{	// (4) ROCKET LAUNCHER
		.type = DECORATE_EXTRA_WEAPON,
		.flags = WPNFLAG_NOAUTOFIRE,
		.motype = 0,
		.ammotype = {0xFFFF, 0xFFFF}, // TODO
		.ammogive = {0, 0}, // TODO
		.ammouse = {0, 0}, // TODO
		.pickupsound = sfx_wpnup,
		.upsound = 0,
		.readysound = 0,
		.kickback = 100,
		.selection = 0xFFFF, // TODO
		.ready_state = 57,
		.deselect_state = 58,
		.select_state = 59,
		.fire1_state = 60,
		.flash1_state = 63,
		.message = (void*)0x000230CC
	},
	{	// (5) PLASMA RIFLE
		.type = DECORATE_EXTRA_WEAPON,
		.flags = 0,
		.motype = 0,
		.ammotype = {0xFFFF, 0xFFFF}, // TODO
		.ammogive = {0, 0}, // TODO
		.ammouse = {0, 0}, // TODO
		.pickupsound = sfx_wpnup,
		.upsound = 0,
		.readysound = 0,
		.kickback = 100,
		.selection = 0xFFFF, // TODO
		.ready_state = 74,
		.deselect_state = 75,
		.select_state = 76,
		.fire1_state = 77,
		.flash1_state = 79,
		.message = (void*)0x000230EC
	},
	{	// (6) BFG
		.type = DECORATE_EXTRA_WEAPON,
		.flags = WPNFLAG_NOAUTOFIRE,
		.motype = 0,
		.ammotype = {0xFFFF, 0xFFFF}, // TODO
		.ammogive = {0, 0}, // TODO
		.ammouse = {0, 0}, // TODO
		.pickupsound = sfx_wpnup,
		.upsound = 0,
		.readysound = 0,
		.kickback = 100,
		.selection = 0xFFFF, // TODO
		.ready_state = 81,
		.deselect_state = 82,
		.select_state = 83,
		.fire1_state = 84,
		.flash1_state = 88,
		.message = (void*)0x00023074
	},
	{	// (7) CHAINSAW
		.type = DECORATE_EXTRA_WEAPON,
		.flags = 0,
		.motype = 0,
		.ammotype = {0xFFFF, 0xFFFF},
		.ammogive = {0, 0},
		.ammouse = {0, 0},
		.pickupsound = sfx_wpnup,
		.upsound = sfx_sawup,
		.readysound = sfx_sawidl,
		.kickback = 0,
		.selection = 0xFFFF, // TODO
		.ready_state = 67,
		.deselect_state = 69,
		.select_state = 70,
		.fire1_state = 71,
		.flash1_state = 0,
		.message = (void*)0x000230AC
	},
	{	// (8) SUPER SHOTGUN
		.type = DECORATE_EXTRA_WEAPON,
		.flags = 0,
		.motype = 0,
		.ammotype = {0xFFFF, 0xFFFF}, // TODO
		.ammogive = {0, 0}, // TODO
		.ammouse = {0, 0}, // TODO
		.pickupsound = sfx_wpnup,
		.upsound = 0,
		.readysound = 0,
		.kickback = 100,
		.selection = 0xFFFF, // TODO
		.ready_state = 32,
		.deselect_state = 33,
		.select_state = 34,
		.fire1_state = 35,
		.flash1_state = 47,
		.message = (void*)0x0002311C
	},
};

static arg_doom_bullet_t wpn_arg_pistol =
{
	.sound = sfx_pistol,
	.count = 1,
	.flags = 1,
	.hs = 18,
	.vs = 0
};

static arg_doom_bullet_t wpn_arg_shotgun =
{
	.sound = sfx_shotgn,
	.count = 7,
	.flags = 3,
	.hs = 18,
	.vs = 0
};

static arg_doom_bullet_t wpn_arg_chgn =
{
	.sound = sfx_pistol,
	.count = 1,
	.flags = 0,
	.hs = 18,
	.vs = 0
};

static arg_doom_bullet_t wpn_arg_shotgun2 =
{
	.sound = sfx_dshtgn,
	.count = 20,
	.flags = 3,
	.hs = 19,
	.vs = 5
};

static wpn_codefix_t wpn_codefix[] =
{
	// (0) FIST
	{2, A_WeaponReady},
	{3, A_Lower},
	{4, A_Raise},
	{6, A_Punch},
	{9, A_ReFire},
	// (1) PISTOL
	{10, A_WeaponReady},
	{11, A_Lower},
	{12, A_Raise},
	{14, A_DoomBullets, &wpn_arg_pistol},
	{16, A_ReFire},
	{17, A_Light1},
	// (2) SHOTGUN
	{18, A_WeaponReady},
	{19, A_Lower},
	{20, A_Raise},
	{22, A_DoomBullets, &wpn_arg_shotgun},
	{29, A_ReFire},
	{30, A_Light1},
	{31, A_Light2},
	// (3) CHAINGUN
	{49, A_WeaponReady},
	{50, A_Lower},
	{51, A_Raise},
	{52, A_DoomBullets, &wpn_arg_pistol},
	{53, A_DoomBullets, &wpn_arg_chgn},
	{54, A_ReFire},
	{55, A_Light1},
	{56, A_Light2},
	// (4) ROCKET LAUNCHER
	{57, A_WeaponReady},
	{58, A_Lower},
	{59, A_Raise},
	{60, A_GunFlash},
	{61, A_NoBlocking}, // TODO
	{62, A_ReFire},
	{63, A_Light1},
	{65, A_Light2},
	{66, A_Light2},
	// (5) PLASMA RIFLE
	{74, A_WeaponReady},
	{75, A_Lower},
	{76, A_Raise},
	{77, A_GunFlash}, // TODO
	{78, A_ReFire},
	{79, A_Light1},
	{80, A_Light1},
	// (6) BFG
	{81, A_WeaponReady},
	{82, A_Lower},
	{83, A_Raise},
	{84 | 0x80000000, A_PlaySound, (void*)sfx_bfg},
	{85, A_GunFlash},
	{86, A_NoBlocking}, // TODO
	{87, A_ReFire},
	{88, A_Light1},
	{89, A_Light2},
	// (7) CHAINSAW
	{67, A_WeaponReady},
	{68, A_WeaponReady},
	{69, A_Lower},
	{70, A_Raise},
	{71, A_Saw},
	{72, A_Saw},
	{73, A_ReFire},
	// (8) SUPER SHOTGUN
	{32, A_WeaponReady},
	{33, A_Lower},
	{34, A_Raise},
	{36, A_DoomBullets, &wpn_arg_shotgun2},
	{38, A_NoBlocking}, // A_CheckReload
	{39 | 0x80000000, A_PlaySound, (void*)sfx_dbopn},
	{41 | 0x80000000, A_PlaySound, (void*)sfx_dbload},
	{43 | 0x80000000, A_PlaySound, (void*)sfx_dbcls},
	{44, A_ReFire},
	{47, A_Light1},
	{48, A_Light2},
	// LIGHT DONE
	{1, A_Light0},
	// terminator
	{0}
};

//
// default mobjinfo
static mobjinfo_t info_default_actor =
{
	.doomednum = -1,
	.spawnid = 0,
	.spawnstate = 0,
	.spawnhealth = 1000,
	.seestate = 0,
	.seesound = 0,
	.attacksound = 0,
	.reactiontime = 8,
	.extrainfo = NULL,
	.painstate = 0,
	.painchance = 0,
	.activesound = 0,
	.painsound = 0,
	.deathsound = 0,
	.meleestate = 0,
	.missilestate = 0,
	.deathstate = 0,
	.xdeathstate = 0,
	.extra = NULL,
	.speed = 0,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.mass = 100,
	.damage = 0,
	.flags2 = 0,
	.flags = 0,
	.raisestate = 0,
};

static mobjinfo_t info_playerpawn_actor =
{
	.doomednum = -1,
	.spawnid = 0,
	.spawnstate = 0,
	.spawnhealth = 100,
	.seestate = 0,
	.seesound = sfx_oof,
	.attacksound = 0,
	.reactiontime = 8,
	.extrainfo = NULL,
	.painstate = 0,
	.painchance = 255,
	.activesound = sfx_noway,
	.painsound = sfx_plpain,
	.deathsound = sfx_pldeth,
	.meleestate = 0,
	.missilestate = 0,
	.deathstate = 0,
	.xdeathstate = 0,
	.extra = NULL,
	.speed = 1 << FRACBITS,
	.radius = 16 << FRACBITS,
	.height = 56 << FRACBITS,
	.mass = 100,
	.damage = 0,
	.flags2 = MF2_TELESTOMP, // FRIENDLY
	.flags = MF_SOLID | MF_SHOOTABLE | MF_DROPOFF | MF_PICKUP | MF_NOTDMATCH | MF_SLIDE,
	.raisestate = 0,
};

static mobjinfo_t info_doomweapon_actor =
{
	.doomednum = -1,
	.spawnid = 0,
	.spawnstate = 0,
	.spawnhealth = 1000,
	.seestate = 0,
	.seesound = 0,
	.attacksound = 0,
	.reactiontime = 8,
	.extrainfo = NULL,
	.painstate = 0,
	.painchance = 0,
	.activesound = 0,
	.painsound = 0,
	.deathsound = 0,
	.meleestate = 0,
	.missilestate = 0,
	.deathstate = 0,
	.xdeathstate = 0,
	.extra = NULL,
	.speed = 0,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.mass = 100,
	.damage = 0,
	.flags2 = 0,
	.flags = MF_SPECIAL,
	.raisestate = 0,
};

static mobjinfo_t info_inventory_actor =
{
	.doomednum = -1,
	.spawnid = 0,
	.spawnstate = 0,
	.spawnhealth = 1000,
	.seestate = 0,
	.seesound = 0,
	.attacksound = 0,
	.reactiontime = 8,
	.extrainfo = NULL,
	.painstate = 0,
	.painchance = 0,
	.activesound = 0,
	.painsound = 0,
	.deathsound = 0,
	.meleestate = 0,
	.missilestate = 0,
	.deathstate = 0,
	.xdeathstate = 0,
	.extra = NULL,
	.speed = 0,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.mass = 100,
	.damage = 0,
	.flags2 = 0,
	.flags = MF_SPECIAL,
	.raisestate = 0,
};

//
// default extra info
static dextra_inventory_t extra_fakeinv =
{
	.type = DECORATE_EXTRA_INVENTORY,
	.flags = 0,
	.maxcount = 0,
	.pickupcount = 0,
	.pickupsound = sfx_itemup,
	.usesound = 0,
	.message = NULL
};

static dextra_playerclass_t extra_playercl =
{
	.type = DECORATE_EXTRA_PLAYERCLASS,
	.flags = 0,
	.motype = 0,
	.menuidx = -1,
	.viewheight = 41 << FRACBITS,
	.attackz = 32 << FRACBITS,
	.jumpz = 8 << FRACBITS,
	.maxhealth = 100,
	.spawnclass = 0
};

static dextra_weapon_t extra_doomweapon =
{
	.type = DECORATE_EXTRA_WEAPON,
	.flags = 0,
	.motype = 0,
	.ammotype = {0xFFFF, 0xFFFF},
	.ammogive = {0, 0},
	.ammouse = {0, 0},
	.pickupsound = sfx_wpnup,
	.upsound = 0,
	.readysound = 0,
	.kickback = 100,
	.selection = 0xFFFF
};

//
// some defaults
static dextra_playerclass_t playerclass_doomplayer =
{
	.type = DECORATE_EXTRA_PLAYERCLASS,
	.flags = 0,
	.motype = 0,
	.menuidx = 0,
	.viewheight = 41 << FRACBITS,
	.attackz = 32 << FRACBITS,
	.jumpz = 0 << FRACBITS,
	.maxhealth = 100,
	.spawnclass = 0
};

// these are all valid actor properties
// all names must be lowercase
static actor_property_t actor_prop_list[] =
{
	//
	{"spawnid", PROPTYPE_INT16, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, spawnid)},
	//
	{"health", PROPTYPE_INT32, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, spawnhealth)},
	{"reactiontime", PROPTYPE_INT32, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, reactiontime)},
	{"painchance", PROPTYPE_INT16, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, painchance)},
	{"damage", PROPTYPE_INT32, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, damage)},
	{"speed", PROPTYPE_FIXED, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, speed)},
	//
	{"radius", PROPTYPE_FIXED, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, radius)},
	{"height", PROPTYPE_FIXED, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, height)},
	{"mass", PROPTYPE_INT32, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, mass)},
	//
	{"activesound", PROPTYPE_SOUND, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, activesound)},
	{"attacksound", PROPTYPE_SOUND, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, attacksound)},
	{"deathsound", PROPTYPE_SOUND, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, deathsound)},
	{"painsound", PROPTYPE_SOUND, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, painsound)},
	{"seesound", PROPTYPE_SOUND, ATTR_MASK_DEFAULT, 0, offsetof(mobjinfo_t, seesound)},
	//
	{"monster", PROPTYPE_FLAGCOMBO, ATTR_MASK_DEFAULT, 0, MF_SHOOTABLE | MF_COUNTKILL | MF_SOLID | MF_ISMONSTER},
	{"projectile", PROPTYPE_FLAGCOMBO, ATTR_MASK_DEFAULT, 0, MF_NOBLOCKMAP | MF_NOGRAVITY | MF_DROPOFF | MF_MISSILE | MF_NOTELEPORT},
	{"states", PROPTYPE_STATES, ATTR_MASK_DEFAULT, 0, 0},
	//
	{"obituary", PROPTYPE_STRING, ATTR_MASK_DEFAULT, PROPFLAG_IGNORED, 0},
	{"hitobituary", PROPTYPE_STRING, ATTR_MASK_DEFAULT, PROPFLAG_IGNORED, 0},
	//
	{"dropitem", PROPTYPE_DROPLIST, ATTR_MASK_DEFAULT, 0, 0},
	//
	{"inventory.pickupsound", PROPTYPE_SOUND, ATTR_MASK_INVENTORY, 0, offsetof(dextra_inventory_t, pickupsound)},
	{"inventory.usesound", PROPTYPE_SOUND, ATTR_MASK_INVENTORY, 0, offsetof(dextra_inventory_t, usesound)},
	{"inventory.pickupmessage", PROPTYPE_STRING, ATTR_MASK_INVENTORY, 0, offsetof(dextra_inventory_t, message)},
	//
	{"inventory.pickupsound", PROPTYPE_SOUND, ATTR_MASK_WEAPON, 0, offsetof(dextra_weapon_t, pickupsound)},
	{"inventory.pickupmessage", PROPTYPE_STRING, ATTR_MASK_WEAPON, 0, offsetof(dextra_weapon_t, message)},
	{"weapon.upsound", PROPTYPE_SOUND, ATTR_MASK_WEAPON, 0, offsetof(dextra_weapon_t, upsound)},
	{"weapon.readysound", PROPTYPE_SOUND, ATTR_MASK_WEAPON, 0, offsetof(dextra_weapon_t, readysound)},
	{"weapon.kickback", PROPTYPE_INT16, ATTR_MASK_WEAPON, 0, offsetof(dextra_weapon_t, readysound)},
	{"weapon.selectionorder", PROPTYPE_INT16, ATTR_MASK_WEAPON, 0, offsetof(dextra_weapon_t, selection)},
	{"weapon.selectionorder", PROPTYPE_INT16, ATTR_MASK_WEAPON, 0, offsetof(dextra_weapon_t, selection)},
	//
	{"player.attackzoffset", PROPTYPE_FIXED, ATTR_MASK_PLAYERCLASS, 0, offsetof(dextra_playerclass_t, attackz)},
	{"player.viewheight", PROPTYPE_FIXED, ATTR_MASK_PLAYERCLASS, 0, offsetof(dextra_playerclass_t, viewheight)},
	{"player.jumpz", PROPTYPE_FIXED, ATTR_MASK_PLAYERCLASS, 0, offsetof(dextra_playerclass_t, jumpz)},
	{"player.maxhealth", PROPTYPE_INT32, ATTR_MASK_PLAYERCLASS, 0, offsetof(dextra_playerclass_t, maxhealth)},
	{"player.spawnclass", PROPTYPE_INT32, ATTR_MASK_PLAYERCLASS, 0, offsetof(dextra_playerclass_t, spawnclass)},
	{"player.soundclass", PROPTYPE_INT32, ATTR_MASK_PLAYERCLASS, PROPFLAG_IGNORED, 0},
	// terminator
	{NULL}
};

// these are all valid actor flags
// all names must be lowercase
static actor_flag_t actor_flag_list[] =
{
	// flags
	{"special", MF_SPECIAL},
	{"solid", MF_SOLID},
	{"shootable", MF_SHOOTABLE},
	{"nosector", MF_NOSECTOR},
	{"noblockmap", MF_NOBLOCKMAP},
	{"ambush", MF_AMBUSH},
	{"justhit", MF_JUSTHIT},
	{"justattacked", MF_JUSTATTACKED},
	{"spawnceiling", MF_SPAWNCEILING},
	{"nogravity", MF_NOGRAVITY},
	{"dropoff", MF_DROPOFF},
	{"pickup", MF_PICKUP},
	{"noclip", MF_NOCLIP},
	{"slidesonwalls", MF_SLIDE},
	{"float", MF_FLOAT},
	{"teleport", MF_TELEPORT},
	{"missile", MF_MISSILE},
	{"dropped", MF_DROPPED},
	{"shadow", MF_SHADOW},
	{"noblood", MF_NOBLOOD},
	{"corpse", MF_CORPSE},
	{"countkill", MF_COUNTKILL},
	{"countitem", MF_COUNTITEM},
	{"skullfly", MF_SKULLFLY},
	{"notdmatch", MF_NOTDMATCH},
	{"noteleport", MF_NOTELEPORT},
	{"ismonster", MF_ISMONSTER},
	{"boss", MF_BOSS},
	{"seekermissile", MF_SEEKERMISSILE},
	{"randomize", MF_RANDOMIZE},
	{"notarget", MF_NOTARGET},
	// flags2
	{"dormant", MF2_INACTIVE, 1},
	{"telestomp", MF2_TELESTOMP, 1},
	// ignored flags
	{"floorclip", 0},
	{"noskin", 0},
	// inventory and weapon flags
	{"inventory.noscreenflash", INVFLAG_NO_SCREEN_FLASH, 2},
	{"inventory.quiet", INVFLAG_QUIET, 2},
	{"inventory.alwayspickup", INVFLAG_ALWAYSPICKUP, 2},
	{"inventory.ignoreskill", INVFLAG_IGNORESKILL, 2},
	// inventory flags
	{"inventory.autoactivate", INVFLAG_ACTIVATE, 2},
	{"inventory.invbar", INVFLAG_INVBAR, 2},
	{"inventory.hubpower", INVFLAG_HUBPOWER, 2},
	{"inventory.persistentpower", INVFLAG_PERSISTENT, 2},
	{"inventory.keepdepleted", INVFLAG_KEEPDEPLETED, 2},
	// weapon flags
	{"weapon.noautofire", WPNFLAG_NOAUTOFIRE, 2},
	{"weapon.dontbob", WPNFLAG_DONTBOB, 2},
	{"weapon.noalert", WPNFLAG_NOALERT, 2},
	{"weapon.ammo_optional", WPNFLAG_AMMO_OPTIONAL, 2},
	{"weapon.alt_ammo_optional", WPNFLAG_ALT_AMMO_OPTIONAL, 2},
	{"weapon.ammo_checkboth", WPNFLAG_AMMO_CHECKBOTH, 2},
	{"weapon.primary_uses_both", WPNFLAG_PRIMARY_USES_BOTH, 2},
	{"weapon.alt_uses_both", WPNFLAG_ALT_USES_BOTH, 2},
	{"weapon.noautoaim", WPNFLAG_NOAUTOAIM, 2},
	{"weapon.meleeweapon", 0, 0}, // ignored
	{"weapon.wimpy_weapon", 0, 0}, // ignored
	{"weapon.explosive", 0, 0}, // ignored
	{"weapon.meleeweapon", 0, 0}, // ignored
	{"weapon.bfg", 0, 0}, // ignored
	{"weapon.no_auto_switch", 0, 0}, // ignored
	// terminator
	{NULL}
};

// these are all valid actor animations
// keep in sync with enum in mobj.h
// all names must be lowercase
static actor_animation_t actor_anim_list[] =
{
	// base
	{"spawn", 0},
	{"see", 0},
	{"pain", 0},
	{"melee", 0},
	{"missile", 0},
	{"death", 0},
	{"xdeath", 0},
	{"raise", 0},
	// extra
	{"heal", 1},
	{"death.fire", 1},
	{"death.ice", 1},
	{"death.disintegrate", 1},
	// inventory
	{"pickup", 0},
	{"use", 0},
	// weapon
	{"ready", 0},
	{"deselect", 0},
	{"select", 0},
	{"fire", 0},
	{"altfire", 0},
	{"hold", 0},
	{"althold", 0},
	{"flash", 0},
	{"altflash", 0},
	{"deadlowered", 0},
	// terminator
	{NULL}
};

// these are only supported parent classes
static actor_parent_t actor_parent_list[] =
{
	{"", NULL, &info_default_actor, NULL, ATTR_MASK_DEFAULT}, // default
	{"PlayerPawn", &decorate_playerclass_count, &info_playerpawn_actor, &extra_playercl, ATTR_MASK_DEFAULT | ATTR_MASK_PLAYERCLASS},
	{"DoomWeapon", &decorate_weapon_count, &info_doomweapon_actor, &extra_doomweapon, ATTR_MASK_DEFAULT | ATTR_MASK_WEAPON},
	{"FakeInventory", &decorate_inventory_count, &info_inventory_actor, &extra_fakeinv, ATTR_MASK_DEFAULT | ATTR_MASK_INVENTORY},
	// terminator
	{NULL}
};

// code pointers for normal actors
// all names must be lowercase
static code_ptr_t codeptr_list_doom[] =
{
	// common
	{"a_look", (void*)0x000276C0, NULL},
	{"a_chase", (void*)0x00027790, NULL},
	{"a_pain", (void*)0x000288A0, NULL},
	{"a_scream", (void*)0x00028820, NULL},
	{"a_xscream", (void*)0x00028890, NULL},
	{"a_facetarget", (void*)0x00027970, NULL},
	{"a_posattack", (void*)0x000279D0, NULL},
	{"a_sposattack", (void*)0x00027A50, NULL},
	{"a_cposattack", (void*)0x00027AE0, NULL},
	{"a_cposrefire", (void*)0x00027B60, NULL},
	{"a_spidrefire", (void*)0x00027BA0, NULL},
	{"a_bspiattack", (void*)0x00027BE0, NULL},
	{"a_troopattack", (void*)0x00027C10, NULL},
	{"a_sargattack", (void*)0x00027C80, NULL},
	{"a_headattack", (void*)0x00027CD0, NULL},
	{"a_cyberattack", (void*)0x00027D30, NULL},
	{"a_bruisattack", (void*)0x00027D60, NULL},
	{"a_skelmissile", (void*)0x00027DC0, NULL},
	{"a_tracer", (void*)0x00027E20, NULL},
	{"a_skelwhoosh", (void*)0x00027FA0, NULL},
	{"a_skelfist", (void*)0x00027FC0, NULL},
//	{"a_vilechase", (void*)0x00028100, NULL}, // TODO: healstate
	{"a_vilestart", (void*)0x00028250, NULL},
	{"a_startfire", (void*)0x00028260, NULL},
	{"a_firecrackle", (void*)0x00028280, NULL},
	{"a_fire", (void*)0x000282A0, NULL},
	{"a_viletarget", (void*)0x00028320, NULL},
	{"a_vileattack", (void*)0x00028370, NULL},
	{"a_fatraise", (void*)0x00028440, NULL},
	{"a_fatattack1", (void*)0x00028460, NULL},
	{"a_fatattack2", (void*)0x000284E0, NULL},
	{"a_fatattack3", (void*)0x00028560, NULL},
	{"a_skullattack", (void*)0x00028620, NULL},
	{"a_painattack", (void*)0x000287C0, NULL},
	{"a_paindie", (void*)0x000287E0, NULL},
	{"a_explode", (void*)0x000288D0, NULL}, // TODO: custom
	{"a_bossdeath", (void*)0x000288F0, NULL},
	{"a_hoof", (void*)0x00028A20, NULL},
	{"a_metal", (void*)0x00028A40, NULL},
	{"a_babymetal", (void*)0x00028A60, NULL},
	{"a_brainawake", (void*)0x00028AC0, NULL},
	{"a_brainpain", (void*)0x00028B20, NULL},
//	{"a_BrainScream", (void*)0x00028B30, NULL}, // TODO: some state
//	{"a_BrainExplode", (void*)0x00028BD0, NULL}, // TODO: some state
	{"a_braindie", (void*)0x00028C50, NULL},
	{"a_brainspit", (void*)0x00028C60, NULL},
	{"a_spawnsound", (void*)0x00028D00, NULL},
	{"a_spawnfly", (void*)0x00028D20, NULL},
	{"a_playerscream", (void*)0x00028E40, NULL},
	// terminator
	{NULL}
};

// code pointers for normal actors
// all names must be lowercase
static code_ptr_t codeptr_list_ace[] =
{
	{"a_noblocking", A_NoBlocking, arg_NoBlocking},
	{"a_fall", A_NoBlocking, NULL},
	{"a_spawnprojectile", A_SpawnProjectile, arg_SpawnProjectile},
	{"a_jumpifcloser", A_JumpIfCloser, arg_JumpIfCloser},
	{"a_jump", A_Jump, arg_Jump},
	{"a_playsound", A_PlaySound, arg_PlaySound},
	// weapons
	{"a_raise", A_Raise, NULL}, // TODO: speed
	{"a_lower", A_Lower, NULL}, // TODO: speed
	{"a_weaponready", A_WeaponReady, NULL}, // TODO: flags
	{"a_refire", A_ReFire, NULL},
	{"a_gunflash", A_GunFlash, NULL}, // TODO: flags
	{"a_light0", A_Light0, NULL},
	{"a_light1", A_Light1, NULL},
	{"a_light2", A_Light2, NULL},
	{"a_punch", A_Punch, NULL},
	{"a_saw", A_Saw, NULL}, // TODO: stuff
	// terminator
	{NULL}
};

// modifications for custom tables
static hook_t hook_update_tables[] =
{
	// states
	{0x0001c731, CODE_HOOK | HOOK_UINT32, 0}, // F_StartCast
	{0x0001c816, CODE_HOOK | HOOK_UINT32, 0}, // F_CastTicker
	{0x0001c843, CODE_HOOK | HOOK_UINT32, 0}, // F_CastTicker
	{0x0001cae4, CODE_HOOK | HOOK_UINT32, 0}, // F_CastTicker
	{0x0001cafa, CODE_HOOK | HOOK_UINT32, 0}, // F_CastTicker
	{0x0001cb28, CODE_HOOK | HOOK_UINT32, 0}, // F_CastTicker
	{0x0001cb7e, CODE_HOOK | HOOK_UINT32, 0}, // F_CastTicker
	{0x0001cbaa, CODE_HOOK | HOOK_UINT32, 0}, // F_CastTicker
	{0x0001cc1b, CODE_HOOK | HOOK_UINT32, 0}, // F_CastTicker
	{0x0002a72f, CODE_HOOK | HOOK_UINT32, 0}, // P_DamageMobj - will be replaced
	{0x0002d062, CODE_HOOK | HOOK_UINT32, 0}, // P_SetPsprite
	{0x00030ec0, CODE_HOOK | HOOK_UINT32, 0}, // P_SetMobjState
	{0x000315d9, CODE_HOOK | HOOK_UINT32, 0}, // P_SpawnMobj
	{0x0002daf3, CODE_HOOK | HOOK_UINT32, 0x5B0}, // A_FireCGun
	// mobjinfo
	{0x00031587, CODE_HOOK | HOOK_UINT32, 0x01000000}, // P_SpawnMobj
	{0x0003177c, CODE_HOOK | HOOK_UINT32, 0x01000000}, // P_RespawnSpecials
	{0x00030f1e, CODE_HOOK | HOOK_UINT32, 0x01000000 | offsetof(mobjinfo_t, deathstate)}, // P_ExplodeMissile // TODO: animation
	// terminator
	{0}
};

// actual pointers
mobjinfo_t *mobjinfo;
state_t *states;

// processing
uint8_t *actor_names_ptr;
uint32_t decorate_num_mobjinfo;
uint32_t decorate_mobjinfo_idx;
void *func_extra_data;
static uint32_t decorate_state_idx;
static uint8_t *actor_name;
static uint32_t actor_name_len;
static actor_parent_t *actor_parent;
static uint32_t actor_ednum;
static uint32_t actor_first_state;
static uint32_t state_storage_free;
static void *state_storage_ptr;
static state_t *state_storage;

uint32_t decorate_playerclass_count;
uint32_t decorate_weapon_count;
uint32_t decorate_inventory_count;

// extra data for codepointers
static arg_droplist_t *droplist;
static uint32_t *droplist_next;

// extra data for mobjinfo
void *decorate_extra_info[DECORATE_NUM_EXTRA];
static uint32_t extra_info_size[DECORATE_NUM_EXTRA] =
{
	sizeof(dextra_inventory_t),
	sizeof(dextra_playerclass_t),
	sizeof(dextra_weapon_t),
};

// for hooks
fixed_t *viletryx;

#define MAX_SPRITES	(STORAGE_ZLIGHT/4)
uint32_t decorate_num_sprites;
static uint32_t last_sprite = 0xFFFFFFFF;
static uint32_t last_sprite_idx;

#define MAX_CUSTOM_STATES	(STORAGE_VISSPRITES/sizeof(custom_state_list_t)) // should be 320
#define STATE_ANIMATION_TARGET	0x80000000
#define STATE_CUSTOM_SOURCE	0x40000000
#define STATE_CUSTOM_TARGET	0x20000000
static uint32_t custom_state_idx;

// find actor by name
int32_t decorate_get_actor(uint8_t *name)
{
	uint32_t idx = 0;
	uint8_t *match = storage_drawsegs;

	while(match < actor_names_ptr)
	{
		uint8_t *end = match + *match + 1;
		uint8_t *ptr = name;

		match++;

		while(1)
		{
			if(*match != *ptr)
				break;
			ptr++;
			match++;
			if(match == end)
			{
				if(!*ptr)
					return idx;
				else
					break;
			}
		}
		match = end;
		idx++;
	}

	return -1;
}

// find actor parent
actor_parent_t *get_actor_parent(uint8_t *name, uint8_t *end)
{
	actor_parent_t *list = actor_parent_list;

	while(list->match)
	{
		uint8_t *ptr = name;
		uint8_t *match = list->match;

		while(1)
		{
			if(*match != *ptr)
				break;
			ptr++;
			match++;
			if(ptr == end)
			{
				if(!*match)
					return list;
				else
					break;
			}
		}
		list++;
	}

	// not found
	*end = 0;
	actor_name[actor_name_len] = 0;
	I_Error("[ACE] DECORATE: actor '%s' unsupported parent class '%s'", actor_name, name);
}

// relocate code pointer list
static void codeptr_list_reloc(code_ptr_t *list, uint32_t base)
{
	while(list->match)
	{
		list->match = list->match + ace_segment;
		list->func = list->func + base;
		if(list->parser)
			list->parser = (void*)list->parser + ace_segment;
		list++;
	}
}

static void *codeptr_parse(code_ptr_t *list, uint8_t *text, uint8_t *end)
{
	uint8_t backup = *tp_func_name_end;

	func_extra_data = NULL;
	*tp_func_name_end = 0;

	while(list->match)
	{
		if(tp_nc_compare(text, list->match))
		{
			void *ret = list->func;
			if(list->parser)
				ret = list->parser(list->func, backup == '(' ? tp_func_name_end + 1 : NULL, end);
			else
			if(backup == '(')
				ret = NULL;
			if(ret)
			{
				*tp_func_name_end = backup;
				return ret;
			}
		}
		list++;
	}
	*tp_func_name_end = backup;
	return NULL;
}

// find custom table
static int32_t custom_state_by_name(uint64_t *wame, uint32_t mask)
{
	custom_state_list_t *custom_state = storage_vissprites;
	uint32_t i;

	for(i = 0; i < custom_state_idx; i++)
	{
		if(custom_state[i].state & mask && custom_state[i].wame[0] == wame[0] && custom_state[i].wame[1] == wame[1])
			return i;
	}
	return -1;
}

// clear custom state table
static void custom_state_clear()
{
	custom_state_idx = 0;
}

// add custom state destination
static void custom_state_add(uint8_t *name, uint8_t *end, uint32_t num)
{
	custom_state_list_t *custom_state = storage_vissprites;
	int32_t i;
	uint64_t wame[2];

	if(name == end)
	{
		actor_name[actor_name_len] = 0;
		I_Error("[ACE] DECORATE: actor '%s' invalid custom state name", actor_name);
	}

	{
		uint8_t *dst = (uint8_t*)wame;

		for(i = 0; i < CUSTOM_STATE_MAX_NAME && name < end; i++, dst++, name++)
			*dst = *name;
		for(; i < CUSTOM_STATE_MAX_NAME; i++, dst++)
			*dst = 0;
	}

	i = custom_state_by_name(wame, STATE_CUSTOM_SOURCE);
	if(i >= 0)
	{
		// overwrite old one
		custom_state[i].state = STATE_CUSTOM_SOURCE | num;
		return;
	}

	if(custom_state_idx == MAX_CUSTOM_STATES)
	{
		actor_name[actor_name_len] = 0;
		I_Error("[ACE] DECORATE: actor '%s' too many custom state references", actor_name);
	}

	// create new one
	custom_state[custom_state_idx].wame[0] = wame[0];
	custom_state[custom_state_idx].wame[1] = wame[1];
	custom_state[custom_state_idx].state = STATE_CUSTOM_SOURCE | num;
	custom_state_idx++;
}

// add custom state reference
uint32_t decorate_custom_state_find(uint8_t *name, uint8_t *end)
{
	custom_state_list_t *custom_state = storage_vissprites;
	int32_t i;
	uint64_t wame[2];

	if(name == end)
	{
		actor_name[actor_name_len] = 0;
		I_Error("[ACE] DECORATE: actor '%s' invalid custom state name", actor_name);
	}

	{
		uint8_t *dst = (uint8_t*)wame;

		for(i = 0; i < CUSTOM_STATE_MAX_NAME && name < end && *name; i++, dst++, name++)
			*dst = *name;
		for(; i < CUSTOM_STATE_MAX_NAME; i++, dst++)
			*dst = 0;
	}
	i = custom_state_by_name(wame, STATE_CUSTOM_TARGET);
	if(i >= 0)
	{
		// use existing
		return custom_state[i].state;
	}

	if(custom_state_idx == MAX_CUSTOM_STATES)
	{
		actor_name[actor_name_len] = 0;
		I_Error("[ACE] DECORATE: actor '%s' too many custom state references", actor_name);
	}

	// create new one
	i = STATE_CUSTOM_TARGET | (custom_state_idx << 16);
	custom_state[custom_state_idx].wame[0] = wame[0];
	custom_state[custom_state_idx].wame[1] = wame[1];
	custom_state[custom_state_idx].state = i;
	custom_state_idx++;

	return i;
}

// find animation state marker
uint32_t decorate_animation_state_find(uint8_t *name, uint8_t *end)
{
	actor_animation_t *anim = actor_anim_list;
	while(1)
	{
		if(tp_ncompare_skip(name, end, anim->match))
			break;
		anim++;
		if(!anim->match)
		{
			*end = 0;
			actor_name[actor_name_len] = 0;
			I_Error("[ACE] DECORATE: actor '%s' invalid jump label '%s'", actor_name, name);
		}
	}
	return ((anim - actor_anim_list) << 16) | STATE_ANIMATION_TARGET;
}

// get state by relative offset
static uint32_t state_by_offset(uint32_t state, uint32_t offset)
{
	uint32_t last_state = decorate_state_idx;
	while(1)
	{
		if(state >= last_state)
		{
			actor_name[actor_name_len] = 0;
			I_Error("[ACE] DECORATE: actor '%s' invalid state destination", actor_name);
		}

		if(states[state].sprite == 0xFFFF)
		{
			// skip state storage
			// this is the reason this function exist
			state += states[state].frame;
			continue;
		} else
		{
			if(!offset)
				return state;
			offset--;
		}

		state++;
	}
}

// link for jumps
static uint32_t link_state_jump(mobjinfo_t *info, uint32_t state, uint32_t from)
{
	custom_state_list_t *custom_state = storage_vissprites;
	int32_t idx;
	if(state & STATE_CUSTOM_TARGET)
	{
		// jump to custom state
		uint32_t dst_name_idx = (state >> 16) & 0x0FFF;
		idx = custom_state_by_name(custom_state[dst_name_idx].wame, STATE_CUSTOM_SOURCE);
		if(idx < 0)
			state = 0;
		else
			state = state_by_offset(custom_state[idx].state & 0x00FFFFFF, 0);
	} else
	if(state & STATE_ANIMATION_TARGET)
	{
		// jump to animation label
		uint32_t anim_idx = (state >> 16) & 0x0FFF;
		state = *P_GetAnimPtr(anim_idx, info);
		state = MOBJ_ANIM_STATE(anim_idx, state);
	} else
	{
		// relative offset
		state = state_by_offset(from, state);
	}
	return state;
}

// link everything
static void link_actor(mobjinfo_t *info)
{
	custom_state_list_t *custom_state = storage_vissprites;
	uint32_t last_state = decorate_state_idx;

	// extra info
	if(info->extra)
	{
		switch(info->extra->type)
		{
			case DECORATE_EXTRA_PLAYERCLASS:
				// store mobj type too
				info->extra->playerclass.motype = info - mobjinfo;
				// limit spawnclass
				if(info->extra->playerclass.spawnclass > 3)
					info->extra->playerclass.spawnclass = 0;
			break;
			case DECORATE_EXTRA_WEAPON:
				info->extra->weapon.motype = info - mobjinfo;
				// disable ammo use if no ammo is defined
				if(info->extra->weapon.ammotype[0] == 0xFFFF)
					info->extra->weapon.ammouse[0] = 0;
				if(info->extra->weapon.ammotype[1] == 0xFFFF)
					info->extra->weapon.ammouse[1] = 0;
			break;
		}
	}

	// go trough every single added state
	for(uint32_t i = actor_first_state; i < last_state; i++)
	{
		if(states[i].sprite == 0xFFFF)
		{
			i += states[i].frame - 1;
			continue;
		}
		if(states[i].action == ACTFIX_NoBlocking)
		{
			// very special handling for this function
			if(!states[i].extra)
				states[i].extra = droplist;
			else
				states[i].extra = NULL;
			states[i].action = A_NoBlocking;
		}
		if(states[i].action == ACTFIX_JumpIfCloser)
		{
			// very special handling for this function
			arg_jump_distance_t *extra = states[i].extra;
			extra->state = link_state_jump(info, extra->state, i);
			states[i].action = A_JumpIfCloser;
		}
		if(states[i].action == ACTFIX_Jump)
		{
			// very special handling for this function
			arg_jump_random_t *extra = states[i].extra;
			for(int j = 0; j < extra->count; j++)
				extra->state[j] = link_state_jump(info, extra->state[j], i);
			states[i].action = A_Jump;
		}
		if(states[i].nextstate & STATE_CUSTOM_TARGET)
		{
			// parse custom state destination
			int32_t idx;
			uint32_t dst_name_idx = (states[i].nextstate >> 16) & 0x0FFF;

			// find this state
			idx = custom_state_by_name(custom_state[dst_name_idx].wame, STATE_CUSTOM_SOURCE);
			if(idx < 0)
			{
				// not found, STOP
				states[i].nextstate = 0;
			} else
			{
				// found, recalculate destination
				states[i].nextstate = state_by_offset(custom_state[idx].state & 0x00FFFFFF, states[i].nextstate & 0xFFFF);
			}
		} else
		if(states[i].nextstate & STATE_ANIMATION_TARGET)
		{
			// parse label state destination
			uint32_t anim_idx = (states[i].nextstate >> 16) & 0x0FFF;
			uint32_t state = *P_GetAnimPtr(anim_idx, info);

			// recalculate destination
			state = state_by_offset(state, states[i].nextstate & 0xFFFF);
			states[i].nextstate = MOBJ_ANIM_STATE(anim_idx, state);
		}
	}
}

//
// special callbacks

static uint32_t decorate_get_sprite(uint8_t *spr, uint8_t *end)
{
	uint32_t ret;
	uint32_t *check;
	uint32_t name = *((uint32_t*)spr);

	// TODO: maybe process special sprites

	if(last_sprite == name)
		return last_sprite_idx;

	// TODO: (better) check for valid sprite name
	if(end - spr != 4)
	{
		*end = 0;
		actor_name[actor_name_len] = 0;
		I_Error("[ACE] DECORATE: actor '%s' invalid sprite name '%s'", actor_name, spr);
	}

	// find in existing names
	check = storage_zlight;
	for(ret = 0; ret < decorate_num_sprites; ret++, check++)
		if(name == *check)
			break;
	if(ret == decorate_num_sprites)
	{
		// check limit
		if(decorate_num_sprites == MAX_SPRITES)
		{
			actor_name[actor_name_len] = 0;
			I_Error("[ACE] DECORATE: actor '%s' out of sprite names", actor_name);
		}
		// add new
		*check = name;
		decorate_num_sprites++;
	}

	// faster repeated access
	last_sprite = name;
	last_sprite_idx = ret;

	return ret;
}

static state_t *decorate_get_state()
{
	// allocate single state
	state_t *ret = states + decorate_state_idx;
	decorate_state_idx++;

	Z_Enlarge(states, decorate_state_idx * sizeof(state_t));

	// storage chain was broken
	state_storage = NULL;

	return ret;
}

void *decorate_get_storage(uint32_t size)
{
	// allocate memory in 'states'
	void *ret;
	uint32_t count;

	if(state_storage)
	{
		ret = state_storage_ptr;
		if(size <= state_storage_free)
		{
			// requested size will fit
			state_storage_free -= size;
			state_storage_ptr += size;
		} else
		{
			// requested size won't fit
			state_storage_ptr += size;
			size -= state_storage_free;
			count = (size + (sizeof(state_t)-1)) / sizeof(state_t);
			state_storage_free = (count * sizeof(state_t)) - size;
			decorate_state_idx += count;
			Z_Enlarge(states, decorate_state_idx * sizeof(state_t));
			// update marker
			state_storage->frame += count;
		}
		return ret;
	}

	// new allocation
	size += 4; // size of marker
	count = (size + (sizeof(state_t)-1)) / sizeof(state_t);

	ret = states + decorate_state_idx;

	state_storage_free = (count * sizeof(state_t)) - size;
	state_storage_ptr = ret + size;

	decorate_state_idx += count;
	Z_Enlarge(states, decorate_state_idx * sizeof(state_t));

	// mark as 'not a state'
	state_storage = ret;
	state_storage->sprite = 0xFFFF;
	state_storage->frame = count;

	return ret + 4;
}

static mobjinfo_t *decorate_new_actor()
{
	mobjinfo_t *ret = mobjinfo + decorate_mobjinfo_idx;

	// check count
	if(decorate_mobjinfo_idx == decorate_num_mobjinfo)
		I_Error("[ACE] DECORATE: miscounted actors");
	decorate_mobjinfo_idx++;

	// return this mobjinfo
	return ret;
}

static void decorate_add_actor_name(uint8_t *name, uint32_t nlen)
{
	// checks
	if(nlen > 255)
	{
		actor_name[actor_name_len] = 0;
		I_Error("[ACE] DECORATE: actor '%s' too long name", actor_name);
	}
	if(actor_names_ptr + nlen + 1 > (uint8_t*)storage_drawsegs + STORAGE_DRAWSEGS)
	{
		actor_name[actor_name_len] = 0;
		I_Error("[ACE] DECORATE: out of actor names (actor '%s')", actor_name);
	}

	// add actor name to the table
	*actor_names_ptr++ = nlen;
	do
	{
		*actor_names_ptr++ = *name++;
	} while(--nlen);
}

// count actors and actor name table
static uint8_t *decparse_count(uint8_t *start, uint8_t *end)
{
	uint8_t *err;

	decorate_num_mobjinfo++;

	start = tp_skip_section_full(start, end, mark_section, &err);
	if(!start)
	{
		actor_name[actor_name_len] = 0;
		I_Error("[ACE] DECORATE: %s (actor '%s')", err, actor_name);
	}

	decorate_add_actor_name(actor_name, actor_name_len);

	return start;
}

// full actor parsing
static uint8_t *decparse_full(uint8_t *start, uint8_t *end)
{
	mobjinfo_t *info;
	uint8_t *tmp;
	uint8_t *ptr = start;
#ifdef debug_printf
	uint8_t backup;
#endif

	// get actor mobjinfo
	info = decorate_new_actor();
	// use default by parent
	*info = *actor_parent->template;
	// use extra info
	if(actor_parent->extra_template)
	{
		uint16_t type = *((uint16_t*)actor_parent->extra_template);
		uint32_t idx = *actor_parent->count - 1;
		info->extra = decorate_extra_info[type] + idx * extra_info_size[type];
		memcpy(info->extra, actor_parent->extra_template, extra_info_size[type]);
	}
	// set ednum
	info->doomednum = actor_ednum;

	// reset some stuff
	actor_first_state = decorate_state_idx;
	droplist = NULL;

	ptr++; // skip '{'

	while(ptr < end)
	{
		// skip WS
		ptr = tp_skip_wsc(ptr, end);
		if(ptr == end)
			goto end_eof;

		// check for ending
		if(*ptr == '}')
			// done
			return ptr + 1;

		// must get actor property
		tmp = tp_get_keyword(ptr, end);
		if(tmp == end || tmp == ptr)
		{
			actor_name[actor_name_len] = 0;
			I_Error("[ACE] DECORATE: actor '%s' property error", actor_name);
		}

#ifdef debug_printf
		backup = *tmp;
		*tmp = 0;
		debug_printf("got property '%s' ", ptr);
		*tmp = backup;
#endif
		// check for flags
		if(*ptr == '+' || *ptr == '-')
		{
			uint32_t *fdst;
			register uint8_t mode = *ptr++;
			if(ptr == end)
				goto end_eof;
			// find this flag
			actor_flag_t *flag = actor_flag_list;
			while(1)
			{
				if(tp_ncompare_skip(ptr, end, flag->match))
					break;
				flag++;
				if(!flag->match)
				{
					actor_name[actor_name_len] = 0;
					*tmp = 0;
unknown_flag:
					I_Error("[ACE] DECORATE: actor '%s' unknown flag '%s'", actor_name, ptr);
				}
			}

			switch(flag->extra)
			{
				case 0:
					fdst = &info->flags;
				break;
				case 1:
					fdst = &info->flags2;
				break;
				case 2:
					if(!info->extra)
						goto unknown_flag;
					// 32bit access is not an issue for flag changes
					fdst = (void*)&info->extra->flags;
				break;
			}

			if(mode == '+')
				*fdst |= flag->flags;
			else
				*fdst &= ~flag->flags;

			ptr = tmp;
#ifdef debug_printf
			debug_printf("\n");
#endif
		} else
		{
			void *prop_base;

			// find this property
			actor_property_t *prop = actor_prop_list;
			while(1)
			{
				if(prop->attr_mask & actor_parent->attr_mask && tp_ncompare_skip(ptr, end, prop->match))
					break;
				prop++;
				if(!prop->match)
				{
					actor_name[actor_name_len] = 0;
					*tmp = 0;
					I_Error("[ACE] DECORATE: actor '%s' unknown property '%s'", actor_name, ptr);
				}
			}

			// get base pointer
			if(prop->attr_mask != ATTR_MASK_DEFAULT)
				prop_base = info->extra;
			else
				prop_base = info;

			// skip WS
			ptr = tp_skip_wsc(tmp, end);
			if(ptr == end)
				goto end_eof;

			// parse this property
			if(prop->type < PROPTYPE_FIXED)
			{
				uint32_t val;
				int is_negative;

				// check negative
				if(*ptr == '-')
				{
					ptr++;
					if(ptr == end)
						goto end_eof;
					is_negative = 1;
				} else
					is_negative = 0;

				// parse number
				ptr = tp_get_uint(ptr, end, &val);
				if(ptr == end)
					goto end_eof;
				if(!ptr || !tp_is_ws_next(ptr, end, 1))
				{
					actor_name[actor_name_len] = 0;
					I_Error("[ACE] DECORATE: actor '%s' invalid integer for property '%s'", actor_name, prop->match);
				}

				if(is_negative)
					val = -val;

				if(!(prop->flags & PROPFLAG_IGNORED))
				switch(prop->type)
				{
					case PROPTYPE_INT32:
						*((uint32_t*)(prop_base + prop->offset)) = val;
					break;
					case PROPTYPE_INT16:
						*((uint16_t*)(prop_base + prop->offset)) = val;
					break;
					case PROPTYPE_INT8:
						*((uint8_t*)(prop_base + prop->offset)) = val;
					break;
				}
#ifdef debug_printf
				debug_printf("%d (%u)\n", val, val);
#endif
			} else
			if(prop->type == PROPTYPE_FIXED)
			{
				fixed_t val;

				// parse number
				ptr = tp_get_fixed(ptr, end, &val);
				if(ptr == end)
					goto end_eof;
				if(!ptr || !tp_is_ws_next(ptr, end, 1))
				{
					actor_name[actor_name_len] = 0;
					I_Error("[ACE] DECORATE: actor '%s' invalid number for property '%s'", actor_name, prop->match);
				}

				*((fixed_t*)(prop_base + prop->offset)) = val;
#ifdef debug_printf
				debug_printf("%d. (fixed_t)\n", val >> FRACBITS);
#endif
			} else
			if(prop->type == PROPTYPE_FLAGCOMBO)
			{
				// nothing to parse
				info->flags |= prop->offset;
#ifdef debug_printf
				debug_printf("\n");
#endif
			} else
			if(prop->type == PROPTYPE_DROPLIST)
			{
				uint8_t *dropname;
				uint32_t prob = 255; // negative probability (= no drop) is not supported

				// item name
				tmp = tp_get_string(ptr, end);
				if(!tmp)
				{
					actor_name[actor_name_len] = 0;
					I_Error("[ACE] DECORATE: actor '%s' invalid string for property '%s'", actor_name, prop->match);
				}
				dropname = ptr;

#ifdef debug_printf
				backup = *tmp;
				*tmp = 0;
				debug_printf("%s ", ptr);
				*tmp = backup;
#endif
				// skip WS
				ptr = tp_skip_wsc(tmp, end);
				if(ptr == end)
					goto end_eof;

				// optional probability
				if(*ptr == ',')
				{
					ptr++;
					if(ptr == end)
						goto end_eof;
					// skip WS
					ptr = tp_skip_wsc(ptr, end);
					if(ptr == end)
						goto end_eof;
					// parse number
					ptr = tp_get_uint(ptr, end, &prob);
					if(ptr == end)
						goto end_eof;
					if(!ptr || !tp_is_ws_next(ptr, end, 1))
					{
						actor_name[actor_name_len] = 0;
						I_Error("[ACE] DECORATE: actor '%s' invalid integer for property '%s'", actor_name, prop->match);
					}
					if(prob < 255)
						prob = 255;
				}
#ifdef debug_printf
				debug_printf("%u\n", prob);
#endif
				// 'amount' is not supported
				int aidx = decorate_get_actor(tp_clean_string(dropname));
				if(aidx >= 0)
				{
					if(!droplist)
					{
						// new droplist
						droplist = decorate_get_storage(sizeof(arg_droplist_t) + sizeof(uint32_t));
						droplist->count = 0; // this means 1
						droplist->typecombo[0] = aidx | (prob << 24);
						droplist_next = droplist->typecombo + 1;
					} else
					{
						// add entry to existing list
						if(decorate_get_storage(sizeof(uint32_t)) != droplist_next)
						{
							// list is not contiguous
							actor_name[actor_name_len] = 0;
							I_Error("[ACE] DECORATE: actor '%s' DropItem list was split", actor_name);
						}
						*droplist_next = aidx | (prob << 24);
						droplist_next++;
						droplist->count++;
					}
				}
			} else
			if(prop->type == PROPTYPE_STATES)
			{
				state_t *dstate = NULL;
				uint32_t astate = decorate_state_idx;
				uint32_t lstate = decorate_state_idx; // only for 'loop' keyword
#ifdef debug_printf
				debug_printf("\n");
#endif
				// skip WS
				ptr = tp_skip_wsc(ptr, end);
				if(ptr == end)
					goto end_eof;

				// section start
				if(*ptr++ != '{' || ptr == end)
				{
bad_states:
					actor_name[actor_name_len] = 0;
					I_Error("[ACE] DECORATE: actor '%s' invalid states section", actor_name);
				}

				// reset custom states
				custom_state_clear();

				while(1)
				{
					// skip WS
					ptr = tp_skip_wsc(ptr, end);
					if(ptr == end)
						goto end_eof;

					if(*ptr == '}')
					{
						ptr++;
						if(ptr == end)
							goto end_eof;
						// final linkage
						link_actor(info);
						break;
					}

					// single line parsing
					tp_nl_is_ws = 0;

					// get stuff
					tmp = tp_get_keyword(ptr, end);
					if(tmp == end || tmp == ptr)
						goto bad_states;

					// check what follows
					if(tmp[-1] == ':')
					{
						if(*ptr == '_')
						{
							// custom states
							astate = decorate_state_idx;
							lstate = astate;
							custom_state_add(ptr + 1, tmp - 1, astate);
						} else
						{
							// engine states
							lstate = decorate_state_idx;
							tmp[-1] = ' ';
							actor_animation_t *anim = actor_anim_list;
							while(1)
							{
								if(tp_ncompare_skip(ptr, end, anim->match))
									break;
								anim++;
								if(!anim->match)
								{
									tmp[-1] = 0;
									actor_name[actor_name_len] = 0;
									I_Error("[ACE] DECORATE: actor '%s' invalid label '%s'", actor_name, ptr);
								}
							}
							tmp[-1] = ':';
							if(anim->extra && !info->extrainfo)
								// allocate extra info
								info->extrainfo = decorate_get_storage(sizeof(mobj_extra_info_t));
							astate = ((anim - actor_anim_list) << 16) | STATE_ANIMATION_TARGET;
							*P_GetAnimPtr(anim - actor_anim_list, info) = decorate_state_idx;
						}
#ifdef debug_printf
						tmp[-1] = 0;
						debug_printf("label '%s' state %u\n", ptr, decorate_state_idx);
						tmp[-1] = ':';
#endif
						ptr = tmp;
					} else
					if(tp_ncompare_skip(ptr, end, kw_stop))
					{
						if(dstate)
							dstate->nextstate = 0;
						dstate = NULL;
						ptr = tmp;
					} else
					if(tp_ncompare_skip(ptr, end, kw_fail))
					{
						if(dstate)
							dstate->nextstate = 0; // TODO
						dstate = NULL;
						ptr = tmp;
					} else
					if(tp_ncompare_skip(ptr, end, kw_loop))
					{
						if(dstate)
							dstate->nextstate = lstate;
						dstate = NULL;
						ptr = tmp;
					} else
					if(tp_ncompare_skip(ptr, end, kw_wait))
					{
						if(dstate)
							dstate->nextstate = dstate - states;
						dstate = NULL;
						ptr = tmp;
					} else
					if(tp_ncompare_skip(ptr, end, kw_goto))
					{
						uint32_t base;
						uint32_t offset = 0;
						// skip WS
						ptr = tp_skip_wsc(tmp, end);
						if(ptr == end)
							goto end_eof;
						// get state name
						tmp = tp_get_keyword(ptr, end);	// sorry, space must follow
						if(tmp == end || tmp == ptr)
							goto bad_states;
						if(*ptr == '_')
						{
							// custom label
							base = decorate_custom_state_find(ptr + 1, tmp);
						} else
						{
							// find requested label
							base = decorate_animation_state_find(ptr, end);
						}
						// skip WS
						ptr = tp_skip_wsc(tmp, end);
						if(ptr == end)
							goto end_eof;
						// check for offset
						if(*ptr == '+')
						{
							ptr++;
							if(ptr == end)
								goto end_eof;
							// skip WS
							ptr = tp_skip_wsc(ptr, end);
							if(ptr == end)
								goto end_eof;
							// get number
							ptr = tp_get_uint(ptr, end, &offset);
							if(ptr == end)
								goto end_eof;
							if(!ptr)
							{
								*tmp = 0;
								actor_name[actor_name_len] = 0;
								I_Error("[ACE] DECORATE: actor '%s' invalid goto label '%s' offset", actor_name, ptr);
							}
						}

						// change next
						if(dstate)
							dstate->nextstate = base | (offset & 0xFFFF);
						dstate = NULL;
					} else
					{
						uint8_t *frames, *frend;
						uint8_t *bright = NULL;
						uint32_t sprnum;
						uint32_t duration;
						void *func;

						// add sprite
						sprnum = decorate_get_sprite(ptr, tmp);

						// skip WS, for frames
						frames = tp_skip_wsc(tmp, end);
						if(frames == end)
							goto end_eof;

						// get frames (ending)
						tmp = tp_get_keyword(frames, end);
						if(tmp == end || tmp == ptr)
							goto bad_states;
						frend = tmp;

						// skip WS
						ptr = tp_skip_wsc(tmp, end);
						if(ptr == end)
							goto end_eof;

						// check negative duration
						if(*ptr == '-')
						{
							ptr++;
							if(ptr == end)
								goto end_eof;
							bright = ptr; // mark negative
						}

						// get duration
						ptr = tp_get_uint(ptr, end, &duration);
						if(ptr == end)
							goto end_eof;
						if(!ptr)
						{
							actor_name[actor_name_len] = 0;
							I_Error("[ACE] DECORATE: actor '%s' invalid frame duration", actor_name);
						}
						duration &= 0x0FFFFFFF;
						if(bright)
							duration |= 0x80000000;

						// skip WS
						ptr = tp_skip_wsc(ptr, end);
						if(ptr == end)
							goto end_eof;

						// check for "bright"
						bright = tp_ncompare_skip(ptr, end, kw_bright);
						if(bright)
						{
							// skip WS
							ptr = tp_skip_wsc(bright, end);
							if(ptr == end)
								goto end_eof;
						}

						// check for code pointer
						tmp = tp_get_function(ptr, end);
						if(!tmp || (*tp_func_name_end == '(' && tmp[-1] != ')'))
						{
							actor_name[actor_name_len] = 0;
							I_Error("[ACE] DECORATE: actor '%s' broken codepointer function", actor_name);
						}

						if(tmp != ptr)
						{
							func = codeptr_parse(codeptr_list_doom, ptr, tmp);
							if(!func)
								func = codeptr_parse(codeptr_list_ace, ptr, tmp);
							if(!func)
							{
								*tp_func_name_end = 0;
								actor_name[actor_name_len] = 0;
								I_Error("[ACE] DECORATE: actor '%s' codepointer '%s' error", actor_name, ptr);
							}
							ptr = tmp;
						} else
						{
							func = NULL;
							func_extra_data = NULL;
						}

						// add every frame
						// no archvile frames supported
						while(frames < frend)
						{
							register uint8_t frame = *frames++;

							if(frame > 'Z' || frame < 'A')
							{
								actor_name[actor_name_len] = 0;
								I_Error("[ACE] DECORATE: actor '%s' invalid sprite frame '%c'", actor_name, frame);
							}

							if(dstate)
								dstate->nextstate = decorate_state_idx;

							dstate = decorate_get_state();
							dstate->sprite = sprnum;
							dstate->frame = frame - 'A';
							if(bright)
								dstate->frame |= FF_FULLBRIGHT;
							if(duration & 0x80000000)
								dstate->tics = -1;
							else
								dstate->tics = duration;
							dstate->action = func;
							dstate->extra = func_extra_data;
							dstate->nextstate = 0;
							dstate->misc1 = 0;
							dstate->misc2 = 0;
						}
					}
					// back to multi line parsing
					tp_nl_is_ws = 1;
				}
			} else
			{
				// string value
				tmp = tp_get_string(ptr, end);
				if(!tmp)
				{
					actor_name[actor_name_len] = 0;
					I_Error("[ACE] DECORATE: actor '%s' invalid string for property '%s'", actor_name, prop->match);
				}
#ifdef debug_printf
				backup = *tmp;
				*tmp = 0;
				debug_printf("%s\n", ptr);
				*tmp = backup;
#endif
				if(!(prop->flags & PROPFLAG_IGNORED))
				{
					switch(prop->type)
					{
						case PROPTYPE_SOUND:
							// sound in sndtab
							*((uint16_t*)(prop_base + prop->offset)) = sound_get_id(tp_clean_string(ptr));
						break;
						case PROPTYPE_STRING:
							// string that has to be allocated
							tp_clean_string(ptr);
							if(tp_string_len)
							{
								uint8_t *aloc = decorate_get_storage(tp_string_len + 1);
								*((uint8_t**)(prop_base + prop->offset)) = aloc;
								memcpy(aloc, ptr, tp_string_len);
								aloc[tp_string_len] = 0;
							} else
								// empty
								*((uint32_t*)(prop_base + prop->offset)) = 0;
						break;
					}
				}

				ptr = tmp;
			}
		}

		ptr = tp_skip_wsc(ptr, end);
		if(ptr == end)
			goto end_eof;
	}

end_eof:
	actor_name[actor_name_len] = 0;
	I_Error("[ACE] DECORATE: unexpected end of file (actor '%s')", actor_name);
}

// basic decorate parser
static void decorate_process(uint8_t *start, uint8_t *end, uint8_t* (*cb)(uint8_t*,uint8_t*))
{
	uint8_t *ptr = start;
	uint8_t *tmp;
#ifdef debug_printf
	uint8_t backup;
#endif

	actor_name = "-";
	actor_name_len = 1;

	tp_nl_is_ws = 1;
	tp_kw_is_func = 0;

	while(1)
	{
		// must get "actor" keyword or EOF
		ptr = tp_skip_wsc(ptr, end);
		if(ptr == end)
			goto end_ok;
		ptr = tp_ncompare_skip(ptr, end, kw_actor);
		if(!ptr)
		{
			actor_name[actor_name_len] = 0;
			I_Error("[ACE] DECORATE: missing 'actor' keyword (after actor '%s')", actor_name);
		}

		// must get actor name
		ptr = tp_skip_wsc(ptr, end);
		tmp = tp_get_keyword(ptr, end);
		if(tmp == end || tmp == ptr)
		{
			actor_name[actor_name_len] = 0;
			I_Error("[ACE] DECORATE: actor name error (after actor '%s')", actor_name);
		}

#ifdef debug_printf
		backup = *tmp;
		*tmp = 0;
		debug_printf("got actor '%s' ", ptr);
		*tmp = backup;
#endif
		// TODO: check bad characters in actor name, check for duplicate names
		actor_name = ptr;
		actor_name_len = tmp - ptr;

		// skip ws
		ptr = tp_skip_wsc(tmp, end);
		if(ptr == end)
			goto end_eof;

		// check for 'parentclassname'
		if(*ptr == ':')
		{
			// must get actor name
			ptr = tp_skip_wsc(ptr + 1, end);
			tmp = tp_get_keyword(ptr, end);
			if(tmp == end || tmp == ptr)
			{
				actor_name[actor_name_len] = 0;
				I_Error("[ACE] DECORATE: actor '%s' parent name error", actor_name);
			}

#ifdef debug_printf
			backup = *tmp;
			*tmp = 0;
			debug_printf("with parent '%s' ", ptr);
			*tmp = backup;
#endif
			// find parent name
			actor_parent = get_actor_parent(ptr, tmp);
			if(actor_parent->count)
				*actor_parent->count = *actor_parent->count + 1;

			// skip ws
			ptr = tp_skip_wsc(tmp, end);
			if(ptr == end)
				goto end_eof;
		} else
			actor_parent = actor_parent_list;

		// default 'doomednum'
		actor_ednum = 0xFFFFFFFF;

		// check what we got next
		if(*ptr != '{')
		{
			// only 'doomednum' is supported
			ptr = tp_get_uint(ptr, end, &actor_ednum);
			if(ptr == end)
				goto end_eof;
			if(!ptr)
			{
				actor_name[actor_name_len] = 0;
				I_Error("[ACE] DECORATE: actor '%s', expecting doomednum or nothing");
			}
		}

#ifdef debug_printf
		debug_printf("ednum %d\n", actor_ednum);
#endif

		// skip ws
		ptr = tp_skip_wsc(ptr, end);
		if(ptr == end)
			goto end_eof;

		// parse entire actor
		if(*ptr != '{')
		{
			actor_name[actor_name_len] = 0;
			I_Error("[ACE] DECORATE: missing section for actor '%s'", actor_name);
		}

		ptr = cb(ptr, end);
	}

end_eof:
	actor_name[actor_name_len] = 0;
	I_Error("[ACE] DECORATE: unexpected end of file (actor '%s')", actor_name);
end_ok:
	return;
}

void decorate_count_actors(uint8_t *start, uint8_t *end)
{
	decorate_playerclass_count = 0;
	decorate_weapon_count = NUMWEAPONS;	// also count original weapons
	decorate_inventory_count = 0;
	decorate_process(start, end, decparse_count);
}

void decorate_parse(uint8_t *start, uint8_t *end)
{
	decorate_playerclass_count = 0;
	decorate_weapon_count = NUMWEAPONS;	// also count original weapons
	decorate_inventory_count = 0;
	decorate_process(start, end, decparse_full);
}

// stuff required before any parsing
void decorate_prepare()
{
	// setup original doom actor names
	memcpy(storage_drawsegs, doom_actor_names, sizeof(doom_actor_names)-1);
	actor_names_ptr = storage_drawsegs + (sizeof(doom_actor_names)-1);

	// relocate early tables
	{
		actor_parent_t *tab = actor_parent_list;
		while(tab->match)
		{
			tab->match = tab->match + ace_segment;
			tab->count = (void*)tab->count + ace_segment;
			tab->template = (void*)tab->template + ace_segment;
			if(tab->extra_template)
				tab->extra_template = tab->extra_template + ace_segment;
			tab++;
		}
	}
}

// few required things
// call after counting extra actors
void decorate_init(int enabled)
{
	decorate_mobjinfo_idx = NUMMOBJTYPES;
	decorate_state_idx = NUMSTATES;
	state_storage = NULL;

	// generate original sprite table
	{
		char **table = spr_names;

		decorate_num_sprites = 0;
		decorate_get_sprite(kw_null_sprite, kw_null_sprite + 4); // always 0, always invisible

		while(*table)
		{
			decorate_get_sprite(*table, *table + 4);
			table++;
		}
	}

	if(enabled)
	{
		state_t *st;
		old_state_t *os;
		uint32_t count;

		{
			actor_parent_t *tab = actor_parent_list;
			while(tab->match)
			{
				if(tab->extra_template)
				{
					uint32_t slot = *((uint16_t*)tab->extra_template); // .type
					if(!decorate_extra_info[slot])
						decorate_extra_info[slot] = Z_Malloc(*tab->count * extra_info_size[slot], PU_STATIC, NULL);
				}
				tab++;
			}
		}

		mobjinfo = Z_Malloc(decorate_num_mobjinfo * sizeof(mobjinfo_t), PU_STATIC, NULL);
		states = Z_Malloc(NUMSTATES * sizeof(state_t), PU_STATIC, NULL); // must be last, will get enlarged

		// copy original mobjinfo
		for(count = 0; count < NUMMOBJTYPES; count++)
			mobjinfo[count] = e_mobjinfo[count];

		// copy original states, with compensation
		os = (old_state_t*)e_states;
		st = states;
		count = NUMSTATES;
		do
		{
			st->frame = os->frame;
			st->sprite = os->sprite + 1; // sprite[0] is always invisible
			st->tics = os->tics;
			st->action = os->action;
			st->nextstate = os->nextstate;
			st->misc1 = os->misc1;
			st->misc2 = os->misc2;
			os++;
			st++;
		} while(--count);

		// hooks
		hook_t *hook = hook_update_tables;
		while(hook->addr)
		{
			switch(hook->value >> 24)
			{
				case 0:
					hook->value = (hook->value & 0x00FFFFFF) + (uint32_t)states;
				break;
				case 1:
					hook->value = (hook->value & 0x00FFFFFF) + (uint32_t)mobjinfo;
				break;
			}
			hook++;
		}
		utils_install_hooks(hook_update_tables);

		// relocate tables
		{
			actor_property_t *tab = actor_prop_list;
			while(tab->match)
			{
				tab->match = tab->match + ace_segment;
				tab++;
			}
		}
		{
			actor_flag_t *tab = actor_flag_list;
			while(tab->match)
			{
				tab->match = tab->match + ace_segment;
				tab++;
			}
		}
		{
			actor_animation_t *tab = actor_anim_list;
			while(tab->match)
			{
				tab->match = tab->match + ace_segment;
				tab++;
			}
		}
		codeptr_list_reloc(codeptr_list_doom, doom_code_segment);
		codeptr_list_reloc(codeptr_list_ace, ace_segment);
	} else
	{
		// use original pointers
		states = e_states;
		mobjinfo = e_mobjinfo;

		// update original states
		// compensate for changes 'sprite' and 'frame' in 'mobj_t' and 'state_t'
		old_state_t *os = (old_state_t*)states;
		uint32_t count = NUMSTATES;

		do
		{
			os->sprite = ((os->sprite + 1) << 16) | os->frame;
			os++;
		} while(--count);
	}

	// state 0 is invisible
	states[0].sprite = 0;

	// fix all mobjtypes

	mobjinfo[0].flags |= MF_SLIDE; // player
	mobjinfo[18].flags |= MF_ISMONSTER; // lost soul
	mobjinfo[19].flags |= MF_BOSS; // big spider
	mobjinfo[21].flags |= MF_BOSS; // cyberdemon
	mobjinfo[3].flags |= MF_NOTARGET; // archvile

	for(uint32_t i = 0; i < NUMMOBJTYPES; i++)
	{
		old_mobjinfo_t *old = (old_mobjinfo_t*)(mobjinfo + i);
		// monsters
		if(mobjinfo[i].flags & (MF_COUNTKILL | MF_ISMONSTER))
		{
			mobjinfo[i].flags |= MF_ISMONSTER;
			// monster speed
			mobjinfo[i].speed <<= FRACBITS;
		}
		// projectiles
		if(mobjinfo[i].flags & MF_MISSILE)
			mobjinfo[i].flags |= MF_RANDOMIZE;
		// set spawnid
		mobjinfo[i].spawnid = doom_spawn_id[i];
		// fix sounds
		mobjinfo[i].attacksound = old->attacksound;
		mobjinfo[i].deathsound = old->deathsound;
		mobjinfo[i].activesound = old->activesound;
		// clear new stuff
		mobjinfo[i].flags2 = 0;
		mobjinfo[i].extra = NULL;
		mobjinfo[i].extrainfo = NULL;
	}

	// extra player stuff
	mobjinfo[0].flags2 = MF2_TELESTOMP;
	mobjinfo[0].extra = (void*)&playerclass_doomplayer;
	mobjinfo[0].activesound = sfx_noway; // use fail
	mobjinfo[0].seesound = sfx_oof; // when falling
	mobjinfo[0].meleestate = 155; // add melee state - this one is used with 'gun flash'

	// fix player animations for new animation system
	// this could be done for monsters too, but it seems unnecessary
	states[154].nextstate = MOBJ_ANIM_STATE(MOANIM_SPAWN, 0);
	states[157].nextstate = MOBJ_ANIM_STATE(MOANIM_SPAWN, 0);

	// generate original weapons
	for(uint32_t i = 0; i < NUMWEAPONS; i++)
	{
		dextra_weapon_t *wpn = decorate_extra_info[DECORATE_EXTRA_WEAPON];
		wpn += i;
		memcpy(wpn, wpn_info_extra + i, sizeof(dextra_weapon_t));
		if(wpn->message)
			wpn->message = wpn->message + doom_data_segment;
		if(wpn_info_idx[i] < 0xFF)
		{
			mobjinfo[wpn_info_idx[i]].extra = (void*)wpn;
			wpn->motype = wpn_info_idx[i];
		}
	}

	// replace original weapon codeptrs
	{
		wpn_codefix_t *fix = wpn_codefix;
		while(fix->state)
		{
			uint32_t idx = fix->state & 0x7FFFFFFF;
			states[idx].action = fix->ptr + ace_segment;
			if(fix->state & 0x80000000)
				states[idx].extra = fix->extra;
			else
			if(fix->extra)
				states[idx].extra = fix->extra + ace_segment;
			fix++;
		}
	}
	// chaingun flash state
	states[55].nextstate = 56;
	states[56].tics = 4;
	// super shotgun flash state
	states[47].tics = 4;
	states[48].tics = 3;
}

//
// this is only used for custom player classes
void decorate_keyconf(uint8_t *ptr, uint8_t *end)
{
	uint8_t *tmp;
	uint32_t class_idx = 1; // starts with DoomPlayer
	uint16_t first_class = 0; // TODO: remove

	tp_nl_is_ws = 1;
	tp_kw_is_func = 0;

	while(1)
	{
		// skip WS
		ptr = tp_skip_wsc(ptr, end);
		if(ptr == end)
			break;

		// check keywords
		tmp = tp_ncompare_skip(ptr, end, kw_clearplayerclasses);
		if(tmp)
		{
			// remove all classes from the menu
			dextra_playerclass_t *pc = decorate_extra_info[DECORATE_EXTRA_PLAYERCLASS];

			for(uint32_t i = 0; i < decorate_playerclass_count; i++, pc++)
				pc->menuidx = -1;

			first_class = 0xFFFF; // TODO: remove
			class_idx = 0;
			ptr = tmp;
			continue;
		}

		tmp = tp_ncompare_skip(ptr, end, kw_addplayerclass);
		if(tmp)
		{
			int32_t idx;

			// skip WS
			tp_nl_is_ws = 0;
			ptr = tp_skip_wsc(tmp, end);
			if(ptr == end)
				I_Error("[ACE] KEYCONF: invalid 'addplayerclass'");

			// get player class actor
			tmp = tp_get_keyword(ptr, end);
			if(tmp == ptr)
				I_Error("[ACE] KEYCONF: invalid 'addplayerclass'");

			// find this actor
			*tmp = 0;
			idx = decorate_get_actor(ptr);

			// add player class to the menu
			if(idx < 0 || !mobjinfo[idx].extra || *((uint16_t*)mobjinfo[idx].extra) != DECORATE_EXTRA_PLAYERCLASS)
				I_Error("[ACE] KEYCONF: actor '%s' is not PlayerPawn", tmp);

			// TODO: remove
			if(first_class == 0xFFFF)
				first_class = idx;

			*tmp = '\n';
			class_idx++;
			ptr = tmp;
			tp_nl_is_ws = 1;
			continue;
		}

		tmp = tp_get_keyword(ptr, end);
		if(!tmp)
			break;

		*tmp = 0;
		I_Error("[ACE] KEYCONF: unknown keyword %s", ptr);
	}

	// check
	if(!class_idx)
		I_Error("[ACE] KEYCONF: no player classes defined");

	// TODO: remove when player class menu is in
	// since there is no menu yet, pick first player class
	player_class = first_class;
}

//
// hooks
static fixed_t move_speed[] = {FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000, 0, 47000, FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000};

__attribute((regparm(2),no_caller_saved_registers))
uint32_t enemy_chase_move(mobj_t *mo)
{
	fixed_t x, y;
	x = mo->x + FixedMul(mo->info->speed, move_speed[mo->movedir]);
	y = mo->y + FixedMul(mo->info->speed, move_speed[mo->movedir + 6]);
	return P_TryMove(mo, x, y);
}

__attribute((regparm(2),no_caller_saved_registers))
fixed_t vile_chase_move(mobj_t *mo)
{
	*viletryx = mo->x + FixedMul(mo->info->speed, move_speed[mo->movedir]);
	return mo->y + FixedMul(mo->info->speed, move_speed[mo->movedir + 6]);
}

