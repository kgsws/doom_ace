// kgsws' ACE Engine
////
// Subset of DECORATE. Yeah!
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "dehacked.h"
#include "decorate.h"
#include "inventory.h"
#include "action.h"
#include "weapon.h"
#include "sound.h"
#include "map.h"
#include "demo.h"
#include "config.h"
#include "textpars.h"

#include "decodoom.h"

#define NUM_NEW_TYPES	3

#define NUM_STATE_HOOKS	1

enum
{
	DT_U16,
	DT_S32,
	DT_FIXED,
	DT_SOUND,
	DT_STRING,
	DT_ICON,
	DT_SKIP1,
	DT_POWERUP_TYPE,
	DT_POWERUP_MODE,
	DT_MONSTER,
	DT_PROJECTILE,
	DT_MOBJTYPE,
	DT_DROPITEM,
	DT_PP_WPN_SLOT,
	DT_PP_INV_SLOT,
	DT_STATES,
};

typedef struct
{
	const uint8_t *name;
	uint16_t type;
	uint16_t offset;
} dec_attr_t;

typedef struct
{
	const uint8_t *name;
	uint32_t bits;
} dec_flag_t;

typedef struct
{
	const uint8_t *name;
	uint8_t idx;
	uint8_t type;
	uint16_t offset;
} dec_anim_t;

typedef struct
{
	const uint8_t *name;
	const mobjinfo_t *def;
	const dec_attr_t *attr[2];
	const dec_flag_t *flag[2];
} dec_inherit_t;

typedef struct
{
	const uint8_t *name;
	int32_t duration;
	uint32_t flags;
	uint16_t mode;
	uint16_t strength;
} dec_powerup_t;

//

typedef struct
{
	uint8_t type;
	uint8_t selection;
	uint16_t use;
	uint8_t *message;
} doom_weapon_t;

typedef struct
{
	uint8_t clp;
	uint8_t box;
	uint8_t *msg_clp;
	uint8_t *msg_box;
} doom_ammo_t;

typedef struct
{
	uint8_t type;
	uint8_t *icon;
	uint8_t *message;
} doom_key_t;

typedef struct
{
	uint8_t type;
	uint8_t count;
	uint8_t max_count;
	uint8_t bonus;
	uint8_t *message;
} doom_health_t;

typedef struct
{
	uint8_t type;
	uint8_t count;
	uint8_t percent;
	uint8_t bonus;
	uint8_t *message;
} doom_armor_t;

typedef struct
{
	uint8_t type;
	uint8_t power;
	uint8_t *message;
} doom_powerup_t;

typedef struct
{
	uint8_t type;
	uint8_t special;
	uint8_t sound;
	uint8_t *message;
} doom_invspec_t;

typedef struct
{
	void *codeptr;
	void *func;
	const void *arg;
} doom_codeptr_t;

//

uint32_t num_spr_names = NUMSPRITES;
uint32_t sprite_table[MAX_SPRITE_NAMES];

static uint32_t **spr_names;
static uint32_t *numsprites;

uint32_t num_mobj_types = NUMMOBJTYPES + NUM_NEW_TYPES;
mobjinfo_t *mobjinfo;

uint32_t num_states = NUMSTATES;
state_t *states;

uint32_t num_player_classes = 1;
uint16_t player_class[MAX_PLAYER_CLASSES];

static hook_t hook_states[NUM_STATE_HOOKS];

static void *es_ptr;

uint8_t *parse_actor_name;

static mobjinfo_t *parse_mobj_info;
static uint32_t *parse_next_state;
static uint32_t parse_last_anim;

static void *extra_stuff_cur;
static void *extra_stuff_next;

static uint32_t parse_states();
static uint32_t parse_dropitem();
static uint32_t parse_wpn_slot();
static uint32_t parse_inv_slot();

// 'DoomPlayer' stuff
static const ei_player_t ei_player =
{
	.view_height = 41 * FRACUNIT,
	.attack_offs = 8 * FRACUNIT,
};

// default mobj
static const mobjinfo_t default_mobj =
{
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.state_crush = 895,
};

// default 'PlayerPawn'
static const mobjinfo_t default_player =
{
	.spawnhealth = 100,
	.radius = 16 << FRACBITS,
	.height = 56 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.painchance = 255,
	.speed = 1 << FRACBITS,
	.flags = MF_SOLID | MF_SHOOTABLE | MF_DROPOFF | MF_PICKUP | MF_NOTDMATCH | MF_SLIDE,
	.flags1 = MF1_TELESTOMP,
	.player.view_height = 41 << FRACBITS,
	.player.attack_offs = 8 << FRACBITS,
	.state_crush = 895,
};

// default 'Health'
static const mobjinfo_t default_health =
{
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.flags = MF_SPECIAL,
	.state_crush = 895,
	.inventory.count = 1,
	.inventory.max_count = 0,
	.inventory.sound_pickup = 32,
};

// default 'Inventory'
static mobjinfo_t default_inventory =
{
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.flags = MF_SPECIAL,
	.state_crush = 895,
	.inventory.count = 1,
	.inventory.max_count = 1,
	.inventory.hub_count = 1,
	.inventory.sound_pickup = 32,
};

// default 'DoomWeapon'
static const mobjinfo_t default_weapon =
{
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.flags = MF_SPECIAL,
	.state_crush = 895,
	.weapon.inventory.count = 1,
	.weapon.inventory.max_count = 1,
	.weapon.inventory.hub_count = 1,
	.weapon.inventory.sound_pickup = 33,
	.weapon.kickback = 100,
};

// default 'Ammo'
static mobjinfo_t default_ammo =
{
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.flags = MF_SPECIAL,
	.state_crush = 895,
	.ammo.inventory.count = 1,
	.ammo.inventory.max_count = 1,
	.ammo.inventory.hub_count = INV_MAX_COUNT,
	.ammo.inventory.sound_pickup = 32,
};

// default 'DoomKey'
static const mobjinfo_t default_key =
{
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.flags = MF_SPECIAL | MF_NOTDMATCH,
	.flags1 = MF1_DONTGIB,
	.state_crush = 895,
	.inventory.count = 1,
	.inventory.max_count = 1,
	.inventory.sound_pickup = 32,
};

// default 'BasicArmorPickup'
static mobjinfo_t default_armor =
{
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.flags = MF_SPECIAL,
	.eflags = MFE_INVENTORY_AUTOACTIVATE,
	.state_crush = 895,
	.armor.inventory.count = 1,
	.armor.inventory.max_count = 0,
	.armor.inventory.hub_count = INV_MAX_COUNT,
	.armor.inventory.sound_pickup = 32,
};

// default 'BasicArmorBonus'
static mobjinfo_t default_armor_bonus =
{
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.flags = MF_SPECIAL,
	.eflags = MFE_INVENTORY_AUTOACTIVATE | MFE_INVENTORY_ALWAYSPICKUP,
	.state_crush = 895,
	.armor.inventory.count = 1,
	.armor.inventory.max_count = 0,
	.armor.inventory.hub_count = INV_MAX_COUNT,
	.armor.inventory.sound_pickup = 32,
	.armor.percent = 33,
};

// default 'PowerupGiver'
static mobjinfo_t default_powerup =
{
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.flags = MF_SPECIAL,
	.eflags = MFE_INVENTORY_ALWAYSPICKUP,
	.state_crush = 895,
	.powerup.inventory.count = 1,
	.powerup.inventory.max_count = 25,
	.powerup.inventory.hub_count = INV_MAX_COUNT,
	.powerup.inventory.sound_pickup = 93,
	.powerup.type = -1,
};

// mobj animations
static const dec_anim_t mobj_anim[] =
{
	{"spawn", ANIM_SPAWN, ETYPE_NONE, offsetof(mobjinfo_t, state_spawn)},
	{"see", ANIM_SEE, ETYPE_NONE, offsetof(mobjinfo_t, state_see)},
	{"pain", ANIM_PAIN, ETYPE_NONE, offsetof(mobjinfo_t, state_pain)},
	{"melee", ANIM_MELEE, ETYPE_NONE, offsetof(mobjinfo_t, state_melee)},
	{"missile", ANIM_MISSILE, ETYPE_NONE, offsetof(mobjinfo_t, state_missile)},
	{"death", ANIM_DEATH, ETYPE_NONE, offsetof(mobjinfo_t, state_death)},
	{"xdeath", ANIM_XDEATH, ETYPE_NONE, offsetof(mobjinfo_t, state_xdeath)},
	{"raise", ANIM_RAISE, ETYPE_NONE, offsetof(mobjinfo_t, state_raise)},
	{"crush", ANIM_CRUSH, ETYPE_NONE, offsetof(mobjinfo_t, state_crush)},
	{"heal", ANIM_HEAL, ETYPE_NONE, offsetof(mobjinfo_t, state_heal)},
	// weapon
	{"ready", ANIM_W_READY, ETYPE_WEAPON, offsetof(mobjinfo_t, st_weapon.ready)},
	{"deselect", ANIM_W_LOWER, ETYPE_WEAPON, offsetof(mobjinfo_t, st_weapon.lower)},
	{"select", ANIM_W_RAISE, ETYPE_WEAPON, offsetof(mobjinfo_t, st_weapon.raise)},
	{"deadlowered", ANIM_W_DEADLOW, ETYPE_WEAPON, offsetof(mobjinfo_t, st_weapon.deadlow)},
	{"fire", ANIM_W_FIRE, ETYPE_WEAPON, offsetof(mobjinfo_t, st_weapon.fire)},
	{"altfire", ANIM_W_FIRE_ALT, ETYPE_WEAPON, offsetof(mobjinfo_t, st_weapon.fire_alt)},
	{"hold", ANIM_W_HOLD, ETYPE_WEAPON, offsetof(mobjinfo_t, st_weapon.hold)},
	{"althold", ANIM_W_HOLD_ALT, ETYPE_WEAPON, offsetof(mobjinfo_t, st_weapon.hold_alt)},
	{"flash", ANIM_W_FLASH, ETYPE_WEAPON, offsetof(mobjinfo_t, st_weapon.flash)},
	{"altflash", ANIM_W_FLASH_ALT, ETYPE_WEAPON, offsetof(mobjinfo_t, st_weapon.flash_alt)},
	// custom inventory
	{"pickup", ANIM_I_PICKUP, ETYPE_INVENTORY_CUSTOM, offsetof(mobjinfo_t, st_custinv.pickup)},
	{"use", ANIM_I_USE, ETYPE_INVENTORY_CUSTOM, offsetof(mobjinfo_t, st_custinv.use)},
	// terminator
	{NULL}
};

// mobj attributes
static const dec_attr_t attr_mobj[] =
{
	{"spawnid", DT_U16, offsetof(mobjinfo_t, spawnid)},
	//
	{"health", DT_S32, offsetof(mobjinfo_t, spawnhealth)},
	{"reactiontime", DT_S32, offsetof(mobjinfo_t, reactiontime)},
	{"painchance", DT_S32, offsetof(mobjinfo_t, painchance)},
	{"damage", DT_S32, offsetof(mobjinfo_t, damage)},
	{"speed", DT_FIXED, offsetof(mobjinfo_t, speed)},
	//
	{"radius", DT_FIXED, offsetof(mobjinfo_t, radius)},
	{"height", DT_FIXED, offsetof(mobjinfo_t, height)},
	{"mass", DT_S32, offsetof(mobjinfo_t, mass)},
	//
	{"dropitem", DT_DROPITEM},
	//
	{"activesound", DT_SOUND, offsetof(mobjinfo_t, activesound)},
	{"attacksound", DT_SOUND, offsetof(mobjinfo_t, attacksound)},
	{"deathsound", DT_SOUND, offsetof(mobjinfo_t, deathsound)},
	{"painsound", DT_SOUND, offsetof(mobjinfo_t, painsound)},
	{"seesound", DT_SOUND, offsetof(mobjinfo_t, seesound)},
	//
	{"maxstepheight", DT_FIXED, offsetof(mobjinfo_t, step_height)},
	{"maxdropoffheight", DT_FIXED, offsetof(mobjinfo_t, dropoff)},
	//
	{"monster", DT_MONSTER},
	{"projectile", DT_PROJECTILE},
	{"states", DT_STATES},
	//
	{"obituary", DT_SKIP1},
	{"hitobituary", DT_SKIP1},
	// terminator
	{NULL}
};

// mobj flags
static const dec_flag_t mobj_flags0[] =
{
	// ignored flags
	{"floorclip", 0},
	{"noskin", 0},
	// used flags
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
	// terminator
	{NULL}
};
static const dec_flag_t mobj_flags1[] =
{
	{"ismonster", MF1_ISMONSTER},
	{"noteleport", MF1_NOTELEPORT},
	{"randomize", MF1_RANDOMIZE},
	{"telestomp", MF1_TELESTOMP},
	{"notarget", MF1_NOTARGET},
	{"quicktoretaliate", MF1_QUICKTORETALIATE},
	{"seekermissile", MF1_SEEKERMISSILE},
	{"noforwardfall", MF1_NOFORWARDFALL},
	{"dontthrust", MF1_DONTTHRUST},
	{"nodamagethrust", MF1_NODAMAGETHRUST},
	{"dontgib", MF1_DONTGIB}, // TODO: implement in crusher
	{"invulnerable", MF1_INVULNERABLE},
	{"buddha", MF1_BUDDHA},
	{"nodamage", MF1_NODAMAGE},
	{"reflective", MF1_REFLECTIVE}, // TODO: implement
	{"boss", MF1_BOSS}, // TODO: implement in A_Look (see sound)
	{"noradiusdmg", MF1_NORADIUSDMG}, // TODO: implement in explosion
	{"extremedeath", MF1_EXTREMEDEATH},
	{"noextremedeath", MF1_NOEXTREMEDEATH},
	{"skyexplode", MF1_SKYEXPLODE},
	// terminator
	{NULL}
};
static const dec_flag_t inventory_flags[] =
{
	{"inventory.quiet", MFE_INVENTORY_QUIET},
	{"inventory.ignoreskill", MFE_INVENTORY_IGNORESKILL},
	{"inventory.autoactivate", MFE_INVENTORY_AUTOACTIVATE},
	{"inventory.alwayspickup", MFE_INVENTORY_ALWAYSPICKUP},
	{"inventory.invbar", MFE_INVENTORY_INVBAR},
//	{"inventory.hubpower", MFE_INVENTORY_HUBPOWER}, // not available, not planned
//	{"inventory.persistentpower", MFE_INVENTORY_PERSISTENTPOWER},
	{"inventory.bigpowerup", MFE_INVENTORY_BIGPOWERUP}, // not implemented
//	{"inventory.neverrespawn", MFE_INVENTORY_NEVERRESPAWN},
//	{"inventory.keepdepleted", MFE_INVENTORY_KEEPDEPLETED},
	{"inventory.additivetime", MFE_INVENTORY_ADDITIVETIME},
//	{"inventory.restrictabsolutely", MFE_INVENTORY_RESTRICTABSOLUTELY},
	{"inventory.noscreenflash", MFE_INVENTORY_NOSCREENFLASH},
//	{"inventory.transfer", MFE_INVENTORY_TRANSFER},
//	{"inventory.noteleportfreeze", MFE_INVENTORY_NOTELEPORTFREEZE},
//	{"inventory.noscreenblink", MFE_INVENTORY_NOSCREENBLINK},
	{"inventory.untossable", MFE_INVENTORY_UNTOSSABLE}, // for ZDoom compatibility, might be used later
	// terminator
	{NULL}
};
static const dec_flag_t weapon_flags[] =
{
	{"weapon.noautofire", MFE_WEAPON_NOAUTOFIRE},
	{"weapon.noalert", MFE_WEAPON_NOALERT},
	{"weapon.ammo_optional", MFE_WEAPON_AMMO_OPTIONAL},
	{"weapon.alt_ammo_optional", MFE_WEAPON_ALT_AMMO_OPTIONAL},
	{"weapon.ammo_checkboth", MFE_WEAPON_AMMO_CHECKBOTH},
	{"weapon.primary_uses_both", MFE_WEAPON_PRIMARY_USES_BOTH},
	{"weapon.alt_uses_both", MFE_WEAPON_ALT_USES_BOTH},
	{"weapon.noautoaim", MFE_WEAPON_NOAUTOAIM},
	// terminator
	{NULL}
};

// 'PlayerPawn' attributes
static const dec_attr_t attr_player[] =
{
	{"player.viewheight", DT_FIXED, offsetof(mobjinfo_t, player.view_height)},
	{"player.attackzoffset", DT_FIXED, offsetof(mobjinfo_t, player.attack_offs)},
	//
	{"player.weaponslot", DT_PP_WPN_SLOT},
	{"player.startitem", DT_PP_INV_SLOT},
	//
	{"player.displayname", DT_SKIP1},
	{"player.crouchsprite", DT_SKIP1},
	// terminator
	{NULL}
};

// 'Inventory' attributes
static const dec_attr_t attr_inventory[] =
{
	{"inventory.amount", DT_U16, offsetof(mobjinfo_t, inventory.count)},
	{"inventory.maxamount", DT_U16, offsetof(mobjinfo_t, inventory.max_count)},
	{"inventory.interhubamount", DT_U16, offsetof(mobjinfo_t, inventory.hub_count)},
	{"inventory.icon", DT_ICON, offsetof(mobjinfo_t, inventory.icon)},
	{"inventory.pickupflash", DT_MOBJTYPE, offsetof(mobjinfo_t, inventory.flash_type)},
	{"inventory.respawntics", DT_MOBJTYPE, offsetof(mobjinfo_t, inventory.respawn_tics)},
	{"inventory.usesound", DT_SOUND, offsetof(mobjinfo_t, inventory.sound_use)},
	{"inventory.pickupsound", DT_SOUND, offsetof(mobjinfo_t, inventory.sound_pickup)},
	{"inventory.pickupmessage", DT_STRING, offsetof(mobjinfo_t, inventory.message)},
	{"inventory.althudicon", DT_SKIP1, 0},
	// terminator
	{NULL}
};

// 'DoomWeapon' attributes
static const dec_attr_t attr_weapon[] =
{
	{"weapon.selectionorder", DT_U16, offsetof(mobjinfo_t, weapon.selection_order)},
	{"weapon.kickback", DT_U16, offsetof(mobjinfo_t, weapon.kickback)},
	{"weapon.ammouse", DT_U16, offsetof(mobjinfo_t, weapon.ammo_use[0])},
	{"weapon.ammouse1", DT_U16, offsetof(mobjinfo_t, weapon.ammo_use[0])},
	{"weapon.ammouse2", DT_U16, offsetof(mobjinfo_t, weapon.ammo_use[1])},
	{"weapon.ammogive", DT_U16, offsetof(mobjinfo_t, weapon.ammo_give[0])},
	{"weapon.ammogive1", DT_U16, offsetof(mobjinfo_t, weapon.ammo_give[0])},
	{"weapon.ammogive2", DT_U16, offsetof(mobjinfo_t, weapon.ammo_give[1])},
	{"weapon.ammotype", DT_MOBJTYPE, offsetof(mobjinfo_t, weapon.ammo_type[0])},
	{"weapon.ammotype1", DT_MOBJTYPE, offsetof(mobjinfo_t, weapon.ammo_type[0])},
	{"weapon.ammotype2", DT_MOBJTYPE, offsetof(mobjinfo_t, weapon.ammo_type[1])},
	{"weapon.readysound", DT_SOUND, offsetof(mobjinfo_t, weapon.sound_ready)},
	{"weapon.upsound", DT_SOUND, offsetof(mobjinfo_t, weapon.sound_up)},
	// terminator
	{NULL}
};

// 'Ammo' attributes
static const dec_attr_t attr_ammo[] =
{
	{"ammo.backpackamount", DT_U16, offsetof(mobjinfo_t, ammo.count)},
	{"ammo.backpackmaxamount", DT_U16, offsetof(mobjinfo_t, ammo.max_count)},
	// terminator
	{NULL}
};

// 'BasicArmorPickup' attributes
static const dec_attr_t attr_armor[] =
{
	{"armor.saveamount", DT_U16, offsetof(mobjinfo_t, armor.count)},
	{"armor.maxsaveamount", DT_U16, offsetof(mobjinfo_t, armor.max_count)}, // only for bonus, but MEH
	{"armor.savepercent", DT_U16, offsetof(mobjinfo_t, armor.percent)},
	// terminator
	{NULL}
};

// 'BasicArmorPickup' attributes
static const dec_attr_t attr_powerup[] =
{
	{"powerup.duration", DT_S32, offsetof(mobjinfo_t, powerup.duration)},
	{"powerup.type", DT_POWERUP_TYPE},
	{"powerup.mode", DT_POWERUP_MODE},
	{"powerup.strength", DT_U16, offsetof(mobjinfo_t, powerup.strength)},
	// terminator
	{NULL}
};

// actor inheritance
const dec_inherit_t inheritance[NUM_EXTRA_TYPES] =
{
	[ETYPE_NONE] =
	{
		.name = NULL,
		.def = &default_mobj
	},
	[ETYPE_PLAYERPAWN] =
	{
		.name = "PlayerPawn",
		.def = &default_player,
		.attr[0] = attr_player
	},
	[ETYPE_HEALTH] =
	{
		.name = "Health",
		.def = &default_health,
		.attr[0] = attr_inventory,
		.flag[0] = inventory_flags,
	},
	[ETYPE_INV_SPECIAL] =
	{
		// special inheritance
		.def = &default_inventory,
		.attr[0] = attr_inventory,
		.flag[0] = inventory_flags,
	},
	[ETYPE_INVENTORY] =
	{
		.name = "Inventory",
		.def = &default_inventory,
		.attr[0] = attr_inventory,
		.flag[0] = inventory_flags,
	},
	[ETYPE_INVENTORY_CUSTOM] =
	{
		.name = "CustomInventory",
		.def = &default_inventory,
		.attr[0] = attr_inventory,
		.flag[0] = inventory_flags,
	},
	[ETYPE_WEAPON] =
	{
		.name = "DoomWeapon",
		.def = &default_weapon,
		.attr[0] = attr_inventory,
		.attr[1] = attr_weapon,
		.flag[0] = inventory_flags,
		.flag[1] = weapon_flags,
	},
	[ETYPE_AMMO] =
	{
		.name = "Ammo",
		.def = &default_ammo,
		.attr[0] = attr_inventory,
		.attr[1] = attr_ammo,
		.flag[0] = inventory_flags,
	},
	[ETYPE_AMMO_LINK] =
	{
		// fake inheritance
		.def = &default_ammo,
		.attr[0] = attr_inventory,
		.flag[0] = inventory_flags,
	},
	[ETYPE_KEY] =
	{
		.name = "DoomKey",
		.def = &default_key,
		.attr[0] = attr_inventory,
		.flag[0] = inventory_flags,
	},
	[ETYPE_ARMOR] =
	{
		.name = "BasicArmorPickup",
		.def = &default_armor,
		.attr[0] = attr_inventory,
		.attr[1] = attr_armor,
		.flag[0] = inventory_flags,
	},
	[ETYPE_ARMOR_BONUS] =
	{
		.name = "BasicArmorBonus",
		.def = &default_armor_bonus,
		.attr[0] = attr_inventory,
		.attr[1] = attr_armor,
		.flag[0] = inventory_flags,
	},
	[ETYPE_POWERUP] =
	{
		.name = "PowerupGiver",
		.def = &default_powerup,
		.attr[0] = attr_inventory,
		.attr[1] = attr_powerup,
		.flag[0] = inventory_flags,
	},
	[ETYPE_HEALTH_PICKUP] =
	{
		.name = "HealthPickup",
		.def = &default_inventory,
		.attr[0] = attr_inventory,
		.flag[0] = inventory_flags,
	},
};

// internal types
static const mobjinfo_t internal_mobj_info[NUM_NEW_TYPES] =
{
	[MOBJ_IDX_UNKNOWN - NUMMOBJTYPES] =
	{
		.spawnhealth = 1000,
		.mass = 100,
		.flags = MF_NOGRAVITY,
		.state_spawn = 149, // TODO: custom sprite
		.state_idx_limit = NUMSTATES,
	},
	[MOBJ_IDX_FIST - NUMMOBJTYPES] =
	{
		// Fist
		.alias = 0x0000000000D33A46,
		.spawnhealth = 1000,
		.reactiontime = 8,
		.radius = 20 << FRACBITS,
		.height = 16 << FRACBITS,
		.mass = 100,
		.flags = MF_SPECIAL,
		.state_crush = 895,
		.state_idx_limit = NUMSTATES,
		.extra_type = ETYPE_WEAPON,
		.weapon.inventory.count = 1,
		.weapon.inventory.max_count = 1,
		.weapon.inventory.hub_count = 1,
		.weapon.inventory.sound_pickup = 33,
		.weapon.kickback = 100,
	},
	[MOBJ_IDX_PISTOL - NUMMOBJTYPES] =
	{
		// Pistol
		.alias = 0x0000000B2FD33A50,
		.spawnhealth = 1000,
		.reactiontime = 8,
		.radius = 20 << FRACBITS,
		.height = 16 << FRACBITS,
		.mass = 100,
		.flags = MF_SPECIAL,
		.state_crush = 895,
		.state_idx_limit = NUMSTATES,
		.extra_type = ETYPE_WEAPON,
		.weapon.inventory.count = 1,
		.weapon.inventory.max_count = 1,
		.weapon.inventory.hub_count = 1,
		.weapon.inventory.sound_pickup = 33,
		.weapon.kickback = 100,
	}
};

// powerup types
static const dec_powerup_t powerup_type[NUMPOWERS] =
{
	[pw_invulnerability] = {"invulnerable", -30},
	[pw_strength] = {"strength", 1, MFE_INVENTORY_HUBPOWER},
	[pw_invisibility] = {"invisibility", -60, 0, 0, 80},
	[pw_ironfeet] = {"ironfeet", -60},
	[pw_allmap] = {""}, // this is not a powerup
	[pw_infrared] = {"lightamp", -120},
};

// special inventory
static const uint8_t *powerup_special[] =
{
	"BackpackItem",
	"MapRevealer",
	// terminator
	NULL
};

// doom weapons
static doom_weapon_t doom_weapon[NUMWEAPONS] =
{
	{MOBJ_IDX_FIST, 37, 0, NULL}, // Fist (new)
	{MOBJ_IDX_PISTOL, 19, 1, "Pistol!"}, // Pistol (new)
	{77, 13, 1, (uint8_t*)0x00023104}, // Shotgun
	{73, 7, 1, (uint8_t*)0x00023094}, // Chaingun
	{75, 25, 1, (uint8_t*)0x000230CC}, // RocketLauncher
	{76, 1, 1, (uint8_t*)0x000230EC}, // PlasmaRifle
	{72, 28, 40, (uint8_t*)0x00023074}, // BFG9000
	{74, 22, 0, (uint8_t*)0x000230AC}, // Chainsaw
	{78, 4, 2, (uint8_t*)0x0002311C}, // SuperShotgun
};

// doom ammo
static const doom_ammo_t doom_ammo[NUMAMMO] =
{
	{63, 64, (uint8_t*)0x00022F74, (uint8_t*)0x00022F88}, // Clip, ClipBox
	{69, 70, (uint8_t*)0x00023010, (uint8_t*)0x0002302C}, // Shell, ShellBox
	{67, 68, (uint8_t*)0x00022FD4, (uint8_t*)0x00022FF0}, // Cell, CellPack
	{65, 66, (uint8_t*)0x00022FA4, (uint8_t*)0x00022FB8}, // RocketAmmo, RocketBox
};

// doom keys
static const doom_key_t doom_key[NUMCARDS] =
{
	{47, "STKEYS0", (uint8_t*)0x00022DE8}, // BlueCard
	{49, "STKEYS1", (uint8_t*)0x00022E04}, // YellowCard
	{48, "STKEYS2", (uint8_t*)0x00022E20}, // RedCard
	{52, "STKEYS3", (uint8_t*)0x00022E3C}, // BlueSkull
	{50, "STKEYS4", (uint8_t*)0x00022E58}, // YellowSkull
	{51, "STKEYS5", (uint8_t*)0x00022E78}, // RedSkull
};

// doom health
static const doom_health_t doom_health[] =
{
	{53, 10, 0, 0, (uint8_t*)0x00022E94}, // Stimpack
	{54, 25, 0, 0, (uint8_t*)0x00022ED8}, // Medikit
	{45, 1, 200, 1, (uint8_t*)0x00022D94}, // HealthBonus
	{55, 100, 200, 2, (uint8_t*)0x00022DCC}, // Soulsphere
};
#define NUM_HEALTH_ITEMS	(sizeof(doom_health) / sizeof(doom_health_t))

// doom armor
static const doom_armor_t doom_armor[] =
{
	{43, 100, 33, 0, (uint8_t*)0x00022D60}, // GreenArmor
	{44, 200, 50, 0, (uint8_t*)0x00022D78}, // BlueArmor
	{46, 1, 33, 1, (uint8_t*)0x00022DB0}, // ArmorBonus
};
#define NUM_ARMOR_ITEMS	(sizeof(doom_armor) / sizeof(doom_armor_t))

// doom powerups
static const doom_powerup_t doom_powerup[] =
{
	{56, pw_invulnerability, (uint8_t*)0x00022EF0}, // InvulnerabilitySphere
	{58, pw_invisibility, (uint8_t*)0x00022F10}, // BlurSphere
	{59, pw_ironfeet, (uint8_t*)0x00022F28}, // RadSuit
	{61, pw_infrared, (uint8_t*)0x00022F58}, // Infrared
};
#define NUM_POWERUP_ITEMS	(sizeof(doom_powerup) / sizeof(doom_powerup_t))

// doop special powerups
static const doom_invspec_t doom_invspec[] =
{
	{71, 0, 32, (uint8_t*)0x00023050}, // Backpack
	{60, 1, 93, (uint8_t*)0x00022F44}, // Allmap
	{62, 2, 93, (uint8_t*)0x00022DDC}, // Megasphere
	{57, 3, 93, (uint8_t*)0x00022F04}, // Berserk
};
#define NUM_INVSPEC_ITEMS	(sizeof(doom_invspec) / sizeof(doom_invspec_t))

// doom code pointers
static const doom_codeptr_t doom_codeptr[] =
{
	{(void*)0x0002D490, A_Lower, &def_LowerRaise},
	{(void*)0x0002D4D0, A_Raise, &def_LowerRaise},
	{(void*)0x0002D2F0, A_WeaponReady, NULL},
	{(void*)0x0002D3F0, A_ReFire, NULL},
	{(void*)0x0002DB40, A_Light0, NULL},
	{(void*)0x0002DB50, A_Light1, NULL},
	{(void*)0x0002DB60, A_Light2, NULL},
	{(void*)0x0002D550, wpn_codeptr, (void*)0x0002D550}, // A_Punch
	{(void*)0x0002D600, wpn_codeptr, (void*)0x0002D600}, // A_Saw
	{(void*)0x0002D890, A_OldBullets, (void*)1}, // A_FirePistol
	{(void*)0x0002D910, A_OldBullets, (void*)2}, // A_FireShotgun
	{(void*)0x0002D9B0, A_OldBullets, (void*)4}, // A_FireShotgun2
	{(void*)0x0002DA70, A_OldBullets, (void*)86}, // A_FireCGun
	{(void*)0x0002D700, A_OldProjectile, (void*)33}, // A_FireMissile
	{(void*)0x0002D730, A_OldProjectile, (void*)35}, // A_FireBFG
	{(void*)0x0002D760, A_OldProjectile, (void*)34}, // A_FirePlasma
	{(void*)0x0002D510, A_GunFlash, NULL},
	{(void*)0x0002D460, A_CheckReload, NULL},
	{(void*)0x00028A80, wpn_sound, (void*)5}, // A_OpenShotgun2
	{(void*)0x00028A90, wpn_sound, (void*)7}, // A_LoadShotgun2
	{(void*)0x00028AA0, wpn_sound, (void*)6}, // A_CloseShotgun2
	{(void*)0x0002DC20, wpn_sound, (void*)9}, // A_BFGSound
};
#define NUM_CODEPTR_MODS	(sizeof(doom_codeptr) / sizeof(doom_codeptr_t))

// 'DoomPlayer' weapon slots
static const uint16_t doom_slot_1[] = {MOBJ_IDX_FIST, 74, 0};
static const uint16_t doom_slot_2[] = {MOBJ_IDX_PISTOL, 0};
static const uint16_t doom_slot_3[] = {77, 78, 0};
static const uint16_t doom_slot_4[] = {73, 0};
static const uint16_t doom_slot_5[] = {75, 0};
static const uint16_t doom_slot_6[] = {76, 0};
static const uint16_t doom_slot_7[] = {72, 0};
static const uint16_t doom_slot_8[] = {74, 0};
static const uint16_t *doom_weapon_slots[NUM_WPN_SLOTS] =
{
	NULL,
	doom_slot_1,
	doom_slot_2,
	doom_slot_3,
	doom_slot_4,
	doom_slot_5,
	doom_slot_6,
	doom_slot_7,
	doom_slot_8,
	NULL,
};

// 'DoomPlayer' default inventory
static const plrp_start_item_t doom_start_items[] =
{
	{.type = MOBJ_IDX_PISTOL, .count = 1}, // Pistol
	{.type = MOBJ_IDX_FIST, .count = 1}, // Fist
	{.type = 63, .count = 50}, // Clip
};

//
// extra storage

void *dec_es_alloc(uint32_t size)
{
	void *ptr = es_ptr;

	es_ptr += size;
	if(es_ptr > EXTRA_STORAGE_END)
		I_Error("[DECORATE] Extra storage allocation failed!");

	return ptr;
}

//
// funcs

static void make_doom_weapon(uint32_t idx)
{
	const doom_weapon_t *def = doom_weapon + idx;
	mobjinfo_t *info = mobjinfo + def->type;
	weaponinfo_t *wpn = deh_weaponinfo + idx;

	info->extra_type = ETYPE_WEAPON;
	info->weapon = default_weapon.weapon;

	if(def->type < NUMMOBJTYPES)
		info->weapon.inventory.message = def->message + doom_data_segment;

	if(def->type == 74)
	{
		info->weapon.kickback = 0;
		info->weapon.sound_ready = 11;
		info->weapon.sound_up = 10;
	}

	if(wpn->ammo < NUMAMMO)
	{
		info->weapon.ammo_type[0] = doom_ammo[wpn->ammo].clp;
		info->weapon.ammo_give[0] = ((uint32_t*)(0x00012D80 + doom_data_segment))[wpn->ammo] * 2; // clipammo
	}
	info->weapon.selection_order = (uint16_t)def->selection * 100;
	info->weapon.ammo_use[0] = def->use;

	info->st_weapon.raise = wpn->upstate;
	info->st_weapon.lower = wpn->downstate;
	info->st_weapon.ready = wpn->readystate;
	info->st_weapon.fire = wpn->atkstate;
	info->st_weapon.flash = wpn->flashstate;
}

static void make_doom_ammo(uint32_t idx)
{
	const doom_ammo_t *ammo = doom_ammo + idx;
	mobjinfo_t *info;
	uint16_t tmp;

	tmp = ((uint32_t*)(0x00012D80 + doom_data_segment))[idx]; // clipammo

	info = mobjinfo + ammo->clp;
	info->extra_type = ETYPE_AMMO;
	info->eflags = MFE_INVENTORY_KEEPDEPLETED; // hack for original status bar
	info->ammo = default_ammo.ammo;
	info->ammo.inventory.count = tmp;
	info->ammo.inventory.max_count = ((uint32_t*)(0x00012D70 + doom_data_segment))[idx]; // maxammo
	info->ammo.count = info->ammo.inventory.count;
	info->ammo.max_count = info->ammo.inventory.max_count * 2;
	info->ammo.inventory.message = ammo->msg_clp + doom_data_segment;
	// TODO: icon

	info = mobjinfo + ammo->box;
	info->extra_type = ETYPE_AMMO_LINK;
	info->ammo = default_ammo.ammo;
	info->inventory.count = tmp * 5;
	info->inventory.special = ammo->clp;
	info->inventory.message = ammo->msg_box + doom_data_segment;
	// TODO: icon
}

static void make_doom_key(uint32_t idx)
{
	const doom_key_t *key = doom_key + idx;
	mobjinfo_t *info = mobjinfo + key->type;
	int32_t lump;

	lump = wad_check_lump(key->icon);
	if(lump < 0)
		lump = 0;

	info->extra_type = ETYPE_KEY;
	info->inventory = default_key.inventory;
	info->inventory.icon = lump;
	info->inventory.message = key->message + doom_data_segment;
}

static void make_doom_health(uint32_t idx)
{
	const doom_health_t *hp = doom_health + idx;
	mobjinfo_t *info = mobjinfo + hp->type;

	info->extra_type = ETYPE_HEALTH;
	info->inventory = default_health.inventory;
	info->inventory.count = hp->count;
	info->inventory.max_count = hp->max_count;
	info->inventory.message = hp->message + doom_data_segment;
	if(hp->bonus)
		info->eflags |= MFE_INVENTORY_ALWAYSPICKUP;
	if(hp->bonus == 2)
		info->inventory.sound_pickup = 93;
}

static void make_doom_armor(uint32_t idx)
{
	const doom_armor_t *ar = doom_armor + idx;
	mobjinfo_t *info = mobjinfo + ar->type;

	if(ar->bonus)
	{
		info->extra_type = ETYPE_ARMOR_BONUS;
		info->eflags = default_armor_bonus.eflags;
		info->armor = default_armor_bonus.armor;
		info->armor.max_count = 200;
	} else
	{
		info->extra_type = ETYPE_ARMOR;
		info->eflags = default_armor.eflags;
		info->armor = default_armor.armor;
	}

	info->armor.count = ar->count;
	info->armor.percent = ar->percent;
	info->armor.inventory.message = ar->message + doom_data_segment;
}

static void make_doom_powerup(uint32_t idx)
{
	const doom_powerup_t *pw = doom_powerup + idx;
	mobjinfo_t *info = mobjinfo + pw->type;

	info->extra_type = ETYPE_POWERUP;
	info->eflags = default_powerup.eflags | MFE_INVENTORY_AUTOACTIVATE;
	info->powerup = default_powerup.powerup;
	info->powerup.inventory.max_count = 0;
	info->powerup.type = pw->power;
	info->powerup.inventory.message = pw->message + doom_data_segment;
}

static void make_doom_invspec(uint32_t idx)
{
	const doom_invspec_t *pw = doom_invspec + idx;
	mobjinfo_t *info = mobjinfo + pw->type;

	info->extra_type = ETYPE_INV_SPECIAL;
	info->eflags = default_powerup.eflags;
	info->inventory = default_powerup.powerup.inventory;
	info->inventory.special = pw->special;
	info->inventory.sound_pickup = pw->sound;
	info->inventory.message = pw->message + doom_data_segment;
}

static void *relocate_estorage(void *target, void *ptr)
{
	if(ptr < EXTRA_STORAGE_PTR)
		return ptr;

	if(ptr >= EXTRA_STORAGE_END)
		return ptr;

	return target + (ptr - EXTRA_STORAGE_PTR);
}

static uint32_t spr_add_name(uint32_t name)
{
	uint32_t i = 0;

	for(i = 0; i < num_spr_names; i++)
	{
		if(sprite_table[i] == name)
			return i;
	}

	if(i == MAX_SPRITE_NAMES)
		I_Error("[DECORATE] Too many sprite names!");

	sprite_table[i] = name;

	num_spr_names++;

	return i;
}

static uint32_t parse_attr(uint32_t type, void *dest)
{
	num32_t num;
	uint8_t *kw;

	switch(type)
	{
		case DT_U16:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(doom_sscanf(kw, "%u", &num.u32) != 1 || num.u32 > 65535)
				return 1;
			*((uint16_t*)dest) = num.u32;
		break;
		case DT_S32:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(doom_sscanf(kw, "%d", &num.s32) != 1)
				return 1;
			*((int32_t*)dest) = num.s32;
		break;
		case DT_FIXED:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(tp_parse_fixed(kw, &num.s32))
				return 1;
			*((fixed_t*)dest) = num.s32;
		break;
		case DT_SOUND:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			*((uint16_t*)dest) = sfx_by_name(kw);
		break;
		case DT_STRING:
		{
			uint8_t *tmp;

			kw = tp_get_keyword();
			if(!kw)
				return 1;

			num.u32 = strlen(kw);
			num.u32 += 4; // NUL + padding
			num.u32 &= ~3; // 32bit align

			tmp = dec_es_alloc(num.u32);
			strcpy(tmp, kw);

			*((uint8_t**)dest) = tmp;
		}
		break;
		case DT_ICON:
		{
			uint8_t *tmp;

			kw = tp_get_keyword();
			if(!kw)
				return 1;

			num.s32 = wad_check_lump(kw);
			if(num.s32 < 0)
				num.s32 = 0;
			*((int32_t*)dest) = num.s32;
		}
		break;
		case DT_SKIP1:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
		break;
		case DT_POWERUP_TYPE:
			if(parse_mobj_info->powerup.type < NUMPOWERS)
				I_Error("[DECORATE] Powerup mode specified multiple times in '%s'!", parse_actor_name);
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			for(num.u32 = 0; num.u32 < NUMPOWERS; num.u32++)
			{
				if(!strcmp(powerup_type[num.u32].name, kw))
					break;
			}
			if(num.u32 >= NUMPOWERS)
				I_Error("[DECORATE] Unknown powerup type '%s' in '%s'!", kw, parse_actor_name);
			// set type
			parse_mobj_info->powerup.type = num.u32;
			// modify stuff
			parse_mobj_info->eflags |= powerup_type[num.u32].flags;
			parse_mobj_info->powerup.mode = powerup_type[num.u32].mode;
			parse_mobj_info->powerup.strength = powerup_type[num.u32].strength;
		break;
		case DT_POWERUP_MODE:
			if(parse_mobj_info->powerup.type >= NUMPOWERS)
				I_Error("[DECORATE] Powerup mode specified before type in '%s'!", parse_actor_name);
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			// TODO
		break;
		case DT_MONSTER:
			((mobjinfo_t*)dest)->flags |= MF_SHOOTABLE | MF_COUNTKILL | MF_SOLID;
			((mobjinfo_t*)dest)->flags1 |= MF1_ISMONSTER;
		break;
		case DT_PROJECTILE:
			((mobjinfo_t*)dest)->flags |= MF_NOBLOCKMAP | MF_NOGRAVITY | MF_DROPOFF | MF_MISSILE;
			((mobjinfo_t*)dest)->flags1 |= MF1_NOTELEPORT;
		break;
		case DT_MOBJTYPE:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			num.s32 = mobj_check_type(tp_hash64(kw));
			if(num.s32 < 0)
				I_Error("[DECORATE] Unable to find ammo type '%s' in '%s'!", kw, parse_actor_name);
			*((uint16_t*)dest) = num.s32;
		break;
		case DT_DROPITEM:
			if(parse_mobj_info->extra_type == ETYPE_PLAYERPAWN)
				I_Error("[DECORATE] Attempt to add DropItem to player class '%s'!", parse_actor_name);
			return parse_dropitem();
		break;
		case DT_PP_WPN_SLOT:
			return parse_wpn_slot();
		break;
		case DT_PP_INV_SLOT:
			return parse_inv_slot();
		break;
		case DT_STATES:
			num.u32 = parse_states();
			tp_enable_math = 0;
			tp_enable_newline = 0;
			return num.u32;
		break;
	}

	return 0;
}

static int32_t check_add_sprite(uint8_t *text)
{
	// make lowercase, get length
	uint32_t len = 0;
	num32_t sprname;

	while(1)
	{
		uint8_t in = *text;

		if(!in)
		{
			if(len != 4)
				return -1;
			break;
		}

		if(in >= 'a' && in <= 'z')
			sprname.u8[len] = in & ~0x20;
		else
			sprname.u8[len] = in;

		text++;
		len++;
	}

	return spr_add_name(sprname.u32);
}

static uint32_t add_states(uint32_t sprite, uint8_t *frames, int32_t tics, uint16_t flags)
{
	uint32_t tmp = num_states;

	num_states += strlen(frames);
	states = ldr_realloc(states, num_states * sizeof(state_t));

	for(uint32_t i = tmp; i < num_states; i++)
	{
		state_t *state = states + i;
		uint8_t frm;

		if(*frames < 'A' || *frames > ']')
			I_Error("[DECORATE] Invalid frame '%c' in '%s'!", *frames, parse_actor_name);

		if(parse_next_state)
			*parse_next_state = state - states;

		state->sprite = sprite;
		state->frame = (*frames - 'A') | flags;
		state->arg = parse_action_arg;
		state->tics = tics;
		state->action = parse_action_func;
		state->nextstate = 0;
		state->misc1 = 0;
		state->misc2 = 0;

		parse_next_state = &state->nextstate;

		frames++;
		state++;
	}
}

//
// dropitem parser

static uint32_t parse_dropitem()
{
	uint8_t *kw;
	int32_t tmp;
	mobj_dropitem_t *drop;

	// allocate space, setup destination
	drop = dec_es_alloc(sizeof(mobj_dropitem_t));
	if(extra_stuff_cur)
	{
		if(drop != extra_stuff_next)
			I_Error("[DECORATE] Drop item list is not contiguous in '%s'!", parse_actor_name);
	} else
		extra_stuff_cur = drop;

	extra_stuff_next = drop + 1;

	// defaults
	drop->chance = 255;
	drop->amount = 0;

	// get actor name
	kw = tp_get_keyword();
	if(!kw)
		return 1;

	tmp = mobj_check_type(tp_hash64(kw));
	if(tmp < 0)
		drop->type = MOBJ_IDX_UNKNOWN;
	else
		drop->type = tmp;

	// check for chance
	kw = tp_get_keyword();
	if(!kw)
		return 1;
	if(kw[0] != ',')
		goto finished;

	// parse chance
	kw = tp_get_keyword();
	if(!kw)
		return 1;
	if(doom_sscanf(kw, "%d", &tmp) != 1 || tmp < -32768 || tmp > 32767)
		return 1;

	drop->chance = tmp;

	// check for amount
	kw = tp_get_keyword();
	if(!kw)
		return 1;
	if(kw[0] != ',')
		goto finished;

	// parse amount
	kw = tp_get_keyword();
	if(!kw)
		return 1;
	if(doom_sscanf(kw, "%d", &tmp) != 1 || tmp < 0 || tmp > 65535)
		return 1;

	return 0;

finished:
	// this keyword has to be parsed again
	tp_push_keyword(kw);
	return 0;
}

//
// weapon slot parser

static uint32_t parse_wpn_slot()
{
	uint8_t *kw;
	uint32_t slot, count;
	uint16_t *type;

	// get slot number
	kw = tp_get_keyword();
	if(!kw)
		return 1;
	if(doom_sscanf(kw, "%u", &slot) != 1 || slot >= NUM_WPN_SLOTS)
		return 1;

	// get list of weapons
	count = 0;
	while(1)
	{
		int32_t tmp;

		// get separator
		kw = tp_get_keyword();
		if(!kw)
			return 1;
		if(kw[0] != ',')
			break;

		// get actor name
		kw = tp_get_keyword();
		if(!kw)
			return 1;

		tmp = mobj_check_type(tp_hash64(kw));
		if(tmp < 0)
			I_Error("[DECORATE] Unable to find '%s' in '%s' weapon slot %u!", kw, parse_actor_name, slot);

		type = dec_es_alloc(sizeof(uint16_t));
		*type = tmp;

		if(!count)
			parse_mobj_info->player.wpn_slot[slot] = type;

		count++;
	}

	// allocate terminator
	type = dec_es_alloc(sizeof(uint16_t));
	*type = 0;

	// add padding (for 32bit alignment)
	if(!(count & 1))
		dec_es_alloc(sizeof(uint16_t));

	// this keyword has to be parsed again
	tp_push_keyword(kw);
	return 0;
}

//
// start inventory parser

static uint32_t parse_inv_slot()
{
	uint8_t *kw;
	int32_t tmp;
	plrp_start_item_t *item;

	// allocate space, setup destination
	item = dec_es_alloc(sizeof(plrp_start_item_t));
	if(extra_stuff_cur)
	{
		if(item != extra_stuff_next)
			I_Error("[DECORATE] Player start item list is not contiguous in '%s'!", parse_actor_name);
	} else
		extra_stuff_cur = item;

	extra_stuff_next = item + 1;

	// defaults
	item->count = 1;

	// get actor name
	kw = tp_get_keyword();
	if(!kw)
		return 1;
	tmp = mobj_check_type(tp_hash64(kw));
	if(tmp < 0)
		I_Error("[DECORATE] Unable to find '%s' start item for '%s'!", kw, parse_actor_name);

	item->type = tmp;

	// check for amount
	kw = tp_get_keyword();
	if(!kw)
		return 1;
	if(kw[0] != ',')
	{
		// this keyword has to be parsed again
		tp_push_keyword(kw);
		return 0;
	}

	// parse chance
	kw = tp_get_keyword();
	if(!kw)
		return 1;
	if(doom_sscanf(kw, "%d", &tmp) != 1 || tmp < 1 || tmp > 65535)
		return 1;

	item->count = tmp;

	return 0;
}

//
// state parser

static uint32_t parse_states()
{
	uint8_t *kw;
	uint8_t *wk;
	int32_t sprite;
	int32_t tics;
	uint16_t flags;
	uint_fast8_t unfinished;
	uint32_t first_state = num_states;

	kw = tp_get_keyword();
	if(!kw || kw[0] != '{')
		return 1;

	// reset parser
	parse_last_anim = 0;
	parse_next_state = NULL;
	unfinished = 0;

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return 1;
have_keyword:
		if(kw[0] == '}')
			break;

		if(!strcmp(kw, "loop"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_state = parse_last_anim;
			parse_next_state = NULL;
			continue;
		} else
		if(!strcmp(kw, "stop"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_state = 0;
			parse_next_state = NULL;
			continue;
		} else
		if(!strcmp(kw, "wait"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_state = num_states - 1;
			parse_next_state = NULL;
			continue;
		}
		if(parse_mobj_info->extra_type == ETYPE_INVENTORY_CUSTOM && !strcmp(kw, "fail"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_state = 1;
			parse_next_state = NULL;
			continue;
		} else
		if(!strcmp(kw, "goto"))
		{
			uint32_t i;
			const dec_anim_t *anim;

			if(!parse_next_state)
				goto error_no_states;

			// enable math
			tp_enable_math = 1;

			// get state name
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;

			// find animation
			anim = mobj_anim;
			while(anim->name)
			{
				if(	(anim->type == ETYPE_NONE || parse_mobj_info->extra_type == anim->type) &&
					!strcmp(kw, anim->name)
				)
					break;
				anim++;
			}
			if(!anim->name)
			{
				if(parse_mobj_info->extra_type == ETYPE_WEAPON && !strcmp(kw, "lightdone"))
				{
					// special state for weapons
					*parse_next_state = 1;
					// next keyword
					kw = tp_get_keyword_lc();
					if(!kw)
						return 1;
					goto skip_math;
				}
				I_Error("[DECORATE] Unknown animation '%s' in '%s'!", kw, parse_actor_name);
			}

			// check for '+'
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			if(kw[0] == '+')
			{
				// get offset
				kw = tp_get_keyword_lc();
				if(!kw)
					return 1;

				if(doom_sscanf(kw, "%u", &tics) != 1 || tics < 0 || tics > 65535)
					I_Error("[DECORATE] Unable to parse number '%s' in '%s'!", kw, parse_actor_name);

				// next keyword
				kw = tp_get_keyword_lc();
				if(!kw)
					return 1;
			} else
				tics = 0;

			// use animation 'system'
			*parse_next_state = STATE_SET_ANIMATION(anim->idx, tics);
skip_math:
			// disable math
			tp_enable_math = 0;

			// done
			parse_next_state = NULL;

			// one extra keyword was already parsed
			goto have_keyword;
		}

		// sprite frames or state definition
		wk = tp_get_keyword_uc();
		if(!wk)
			return 1;

		if(wk[0] == ':')
		{
			// it's a new state
			const dec_anim_t *anim;

			anim = mobj_anim;
			while(anim->name)
			{
				if(	(anim->type == ETYPE_NONE || parse_mobj_info->extra_type == anim->type) &&
					!strcmp(kw, anim->name)
				)
					break;
				anim++;
			}
			if(!anim->name)
				I_Error("[DECORATE] Unknown animation '%s' in '%s'!", kw, parse_actor_name);

			parse_last_anim = num_states;
			*((uint16_t*)((void*)parse_mobj_info + anim->offset)) = num_states;

			unfinished = 1;
			continue;
		}

		unfinished = 0;

		// it's a sprite
		sprite = check_add_sprite(kw);
		if(sprite < 0)
			I_Error("[DECORATE] Sprite name '%s' has wrong length in '%s'!", kw, parse_actor_name);

		// switch to line-based parsing
		tp_enable_newline = 1;
		// reset frame stuff
		flags = 0;
		parse_action_func = NULL;
		parse_action_arg = NULL;

		// get ticks
		kw = tp_get_keyword();
		if(!kw || kw[0] == '\n')
			return 1;

		if(doom_sscanf(kw, "%d", &tics) != 1)
			I_Error("[DECORATE] Unable to parse number '%s' in '%s'!", kw, parse_actor_name);

		// optional 'bright' or action
		kw = tp_get_keyword_lc();
		if(!kw)
			return 1;

		if(!strcmp(kw, "bright"))
		{
			// optional action
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			// set the flag
			flags |= 0x8000;
		}

		if(kw[0] != '\n')
		{
			// action
			tp_enable_math = 1;
			kw = action_parser(kw);
			tp_enable_math = 0;
			if(!kw)
				return 1;
		}

		// create states
		add_states(sprite, wk, tics, flags);

		if(kw[0] != '\n')
			I_Error("[DECORATE] Expected newline, found '%s' in '%s'!", kw, parse_actor_name);

		// next
		tp_enable_newline = 0;
	}

	// sanity check
	if(unfinished)
		I_Error("[DECORATE] Unfinised animation in '%s'!", parse_actor_name);

	// create a loopback
	// this is how ZDoom behaves
	if(parse_next_state)
		*parse_next_state = first_state;

	// add limitation for animations
	parse_mobj_info->state_idx_first = first_state;
	parse_mobj_info->state_idx_limit = num_states;

	return 0;

error_no_states:
	I_Error("[DECORATE] Missing states for '%s' in '%s'!", kw, parse_actor_name);
}

//
// flag parsing

static uint32_t change_flag(uint8_t *kw, const dec_flag_t *flag, uint32_t offset)
{
	uint32_t *dest;

	// find flag
	while(flag->name)
	{
		if(!strcmp(flag->name, kw + 1))
			break;
		flag++;
	}

	if(!flag->name)
		return 1;

	dest = (void*)parse_mobj_info + offset;

	// change
	if(kw[0] == '+')
		*dest |= flag->bits;
	else
		*dest &= ~flag->bits;

	return 0;
}

//
// callbacks

static void cb_count_actors(lumpinfo_t *li)
{
	uint32_t idx;
	uint8_t *kw;
	uint64_t alias;

	tp_load_lump(li);

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return;

		// must get 'actor'
		if(strcmp(kw, "actor"))
			I_Error("[DECORATE] Expected 'actor', got '%s'!", kw);

		// actor name, case sensitive
		kw = tp_get_keyword();
		if(!kw)
			goto error_end;
		parse_actor_name = kw;

		// skip optional stuff
		while(1)
		{
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;

			if(kw[0] == '{')
				break;
		}

		if(tp_skip_code_block(1))
			goto error_end;

		// check for duplicate
		alias = tp_hash64(parse_actor_name);
		if(mobj_check_type(alias) >= 0)
			I_Error("[DECORATE] Multiple definitions of '%s'!", parse_actor_name);

		// add new type
		idx = num_mobj_types;
		num_mobj_types++;
		mobjinfo = ldr_realloc(mobjinfo, num_mobj_types * sizeof(mobjinfo_t));
		mobjinfo[idx].alias = alias;
	}

error_end:
	I_Error("[DECORATE] Incomplete definition!");
}

static void cb_parse_actors(lumpinfo_t *li)
{
	int32_t idx, etp;
	uint8_t *kw;
	mobjinfo_t *info;
	uint64_t alias;

	tp_load_lump(li);

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return;

		// must get 'actor'
		if(strcmp(kw, "actor"))
			I_Error("[DECORATE] Expected 'actor', got '%s'!", kw);

		// actor name, case sensitive
		kw = tp_get_keyword();
		if(!kw)
			goto error_end;
		parse_actor_name = kw;

		// find mobj
		alias = tp_hash64(kw);
		idx = mobj_check_type(alias);
		if(idx < 0)
			I_Error("[DECORATE] Loading mismatch for '%s'!", kw);
		info = mobjinfo + idx;
		parse_mobj_info = info;

		// next, a few options
		kw = tp_get_keyword_lc();
		if(!kw)
			goto error_end;

		if(kw[0] == ':')
		{
			// inheritance

			// get actor name
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;

			// find this class
			for(etp = 1; etp < NUM_EXTRA_TYPES; etp++)
			{
				if(inheritance[etp].name && !strcmp(kw, inheritance[etp].name))
					break;
			}
			if(etp >= NUM_EXTRA_TYPES)
			{
				// extra special type
				uint32_t extra = 0;
				const uint8_t **name = powerup_special;
				while(*name)
				{
					if(!strcmp(*name, kw))
						break;
					extra++;
					name++;
				}
				if(*name)
				{
					etp = ETYPE_INV_SPECIAL;
					default_inventory.inventory.special = extra;
				}
			}
			if(etp >= NUM_EXTRA_TYPES)
			{
				// must be ammo link
				idx = mobj_check_type(tp_hash64(kw));
				if(idx < 0 || mobjinfo[idx].extra_type != ETYPE_AMMO)
					I_Error("[DECORATE] Invalid inheritance '%s' for '%s'!", kw, parse_actor_name);

				// fake inheritance - only link ammo
				etp = ETYPE_AMMO_LINK;
				default_ammo.inventory.special = idx;
			}

			// next keyword
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;
		} else
			etp = ETYPE_NONE;

		if(!strcmp(kw, "replaces"))
		{
			int32_t rep;

			// get replacement
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;

			rep = mobj_check_type(tp_hash64(kw));
			if(rep <= 0)
				I_Error("[DECORATE] Unable to replace '%s'!", kw);

			mobjinfo[rep].replacement = idx;

			// next keyword
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;
		}

		if(kw[0] != '{')
		{
			// editor number
			if(doom_sscanf(kw, "%u", &idx) != 1 || (uint32_t)idx > 65535)
				goto error_numeric;

			// next keyword
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;
		} else
			idx = -1;

		if(kw[0] != '{')
			I_Error("[DECORATE] Expected '{', got '%s'!", kw);

		// initialize mobj
		memcpy(info, inheritance[etp].def, sizeof(mobjinfo_t));
		info->doomednum = idx;
		info->alias = alias;
		info->extra_type = etp;

		// reset stuff
		extra_stuff_cur = NULL;
		extra_stuff_next = NULL;

		// reset extra storage
		es_ptr = EXTRA_STORAGE_PTR;

		// parse attributes
		while(1)
		{
			kw = tp_get_keyword_lc();
			if(!kw)
				goto error_end;

			// check for end
			if(kw[0] == '}')
				break;

			// check for flags
			if(kw[0] == '-' || kw[0] == '+')
			{
				uint32_t tmp;

				if(
					change_flag(kw, mobj_flags0, offsetof(mobjinfo_t, flags)) &&
					change_flag(kw, mobj_flags1, offsetof(mobjinfo_t, flags1)) &&
					(!inheritance[etp].flag[0] || change_flag(kw, inheritance[etp].flag[0], offsetof(mobjinfo_t, eflags)) ) &&
					(!inheritance[etp].flag[1] || change_flag(kw, inheritance[etp].flag[1], offsetof(mobjinfo_t, eflags)) )
				)
					I_Error("[DECORATE] Unknown flag '%s' in '%s'!", kw + 1, parse_actor_name);
			} else
			{
				const dec_attr_t *attr = attr_mobj;

				// find attribute
				while(attr->name)
				{
					if(!strcmp(attr->name, kw))
						break;
					attr++;
				}

				if(!attr->name && inheritance[etp].attr[0])
				{
					// find attribute
					attr = inheritance[etp].attr[0];
					while(attr->name)
					{
						if(!strcmp(attr->name, kw))
							break;
						attr++;
					}
				}

				if(!attr->name && inheritance[etp].attr[1])
				{
					// find attribute
					attr = inheritance[etp].attr[1];
					while(attr->name)
					{
						if(!strcmp(attr->name, kw))
							break;
						attr++;
					}
				}

				if(!attr->name)
					I_Error("[DECORATE] Unknown attribute '%s' in '%s'!", kw, parse_actor_name);

				// parse it
				if(parse_attr(attr->type, (void*)info + attr->offset))
					I_Error("[DECORATE] Unable to parse value of '%s' in '%s'!", kw, parse_actor_name);
			}
		}

		// save drop item list
		info->extra_stuff[0] = extra_stuff_cur;
		info->extra_stuff[1] = extra_stuff_next;

		// process extra storage
		if(es_ptr != EXTRA_STORAGE_PTR)
		{
			// allocate extra space .. in states!
			uint32_t size;
			uint32_t idx = num_states;

			size = es_ptr - EXTRA_STORAGE_PTR;
			size += sizeof(state_t) - 1;
			size /= sizeof(state_t);

			num_states += size;
			states = ldr_realloc(states, num_states * sizeof(state_t));

			// copy all the stuff
			memcpy(states + idx, EXTRA_STORAGE_PTR, es_ptr - EXTRA_STORAGE_PTR);

			// relocation has to be done later
		}
	}

error_end:
	I_Error("[DECORATE] Incomplete definition!");
error_numeric:
	I_Error("[DECORATE] Unable to parse number '%s' in '%s'!", kw, parse_actor_name);
}

//
// KEYCONF

static void parse_keyconf()
{
	int32_t lump;
	uint8_t *kw;

	lump = wad_check_lump("KEYCONF");
	if(lump < 0)
		return;

	doom_printf("[ACE] load KEYCONF\n");

	tp_load_lump(*lumpinfo + lump);

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			break;

		if(!strcmp("clearplayerclasses", kw))
		{
			num_player_classes = 0;
		} else
		if(!strcmp("addplayerclass", kw))
		{
			if(num_player_classes >= MAX_PLAYER_CLASSES)
				I_Error("[KEYCONF] Too many player classes!");

			kw = tp_get_keyword();
			if(!kw)
				I_Error("[KEYCONF] Incomplete definition!");

			lump = mobj_check_type(tp_hash64(kw));
			if(lump < 0 || mobjinfo[lump].extra_type != ETYPE_PLAYERPAWN)
				I_Error("[KEYCONF] Invalid player class '%s'!", kw);

			player_class[num_player_classes] = lump;

			num_player_classes++;
		} else
			I_Error("[KEYCONF] Unknown keyword '%s'.", kw);
	}
}

//
// API

int32_t mobj_check_type(uint64_t alias)
{
	for(uint32_t i = 0; i < num_mobj_types; i++)
	{
		if(mobjinfo[i].alias == alias)
			return i;
	}

	return -1;
}

void init_decorate()
{
	// To avoid unnecessary memory fragmentation, this function does multiple passes.
	doom_printf("[ACE] init DECORATE\n");
	ldr_alloc_message = "Decorate memory allocation failed!";

	// init sprite names
	for(uint32_t i = 0; i < NUMSPRITES; i++)
		sprite_table[i] = *spr_names[i];

	// mobjinfo
	mobjinfo = ldr_malloc((NUMMOBJTYPES + NUM_NEW_TYPES) * sizeof(mobjinfo_t));

	// copy original stuff
	for(uint32_t i = 0; i < NUMMOBJTYPES; i++)
	{
		memset(mobjinfo + i, 0, sizeof(mobjinfo_t));
		memcpy(mobjinfo + i, deh_mobjinfo + i, sizeof(deh_mobjinfo_t));
		mobjinfo[i].alias = doom_actor_name[i];
		mobjinfo[i].spawnid = doom_spawn_id[i];
		mobjinfo[i].step_height = 24 * FRACUNIT;
		mobjinfo[i].dropoff = 24 * FRACUNIT;
		mobjinfo[i].state_crush = 895;
		mobjinfo[i].state_idx_limit = NUMSTATES;

		// check for original random sounds
		sfx_rng_fix(&mobjinfo[i].seesound, 98);
		sfx_rng_fix(&mobjinfo[i].deathsound, 70);

		// basically everything is randomized
		// basically everything can be seeker missile
		mobjinfo[i].flags1 = MF1_RANDOMIZE | MF1_SEEKERMISSILE;

		// mark enemies
		if(mobjinfo[i].flags & MF_COUNTKILL)
			mobjinfo[i].flags1 |= MF1_ISMONSTER;

		// modify projectiles
		if(mobjinfo[i].flags & MF_MISSILE)
			mobjinfo[i].flags1 |= MF1_NOTELEPORT;
	}

	// copy internal stuff
	memcpy(mobjinfo + NUMMOBJTYPES, internal_mobj_info, sizeof(internal_mobj_info));

	// player stuff
	mobjinfo[0].flags |= MF_SLIDE;
	mobjinfo[0].flags1 |= MF1_TELESTOMP;
	mobjinfo[0].extra_type = ETYPE_PLAYERPAWN;
	memcpy(&mobjinfo[0].player, &ei_player, sizeof(ei_player_t));
	for(uint32_t i = 0; i < NUM_WPN_SLOTS; i++)
		mobjinfo[0].player.wpn_slot[i] = (uint16_t*)doom_weapon_slots[i];
	mobjinfo[0].start_item.start = (void*)doom_start_items;
	mobjinfo[0].start_item.end = (void*)doom_start_items + sizeof(doom_start_items);

	// lost souls are enemy too
	mobjinfo[18].flags1 |= MF1_ISMONSTER;

	// archvile stuff
	mobjinfo[3].flags1 |= MF1_NOTARGET | MF1_QUICKTORETALIATE;

	// boss stuff
	mobjinfo[19].flags1 |= MF1_BOSS | MF1_NORADIUSDMG;
	mobjinfo[21].flags1 |= MF1_BOSS | MF1_NORADIUSDMG;

	// doom weapons
	doom_weapon[6].use = dehacked.bfg_cells;
	for(uint32_t i = 0; i < NUMWEAPONS; i++)
		make_doom_weapon(i);

	// doom ammo
	for(uint32_t i = 0; i < NUMAMMO; i++)
		make_doom_ammo(i);

	// doom keys
	for(uint32_t i = 0; i < NUMCARDS; i++)
		make_doom_key(i);

	// doom health
	for(uint32_t i = 0; i < NUM_HEALTH_ITEMS; i++)
		make_doom_health(i);

	// doom armor
	for(uint32_t i = 0; i < NUM_ARMOR_ITEMS; i++)
		make_doom_armor(i);

	// doom powerups
	for(uint32_t i = 0; i < NUM_POWERUP_ITEMS; i++)
		make_doom_powerup(i);

	// doom other powerups
	for(uint32_t i = 0; i < NUM_INVSPEC_ITEMS; i++)
		make_doom_invspec(i);

	// dehacked modifications
	mobjinfo[45].inventory.max_count = dehacked.max_bonus_health;
	mobjinfo[46].armor.max_count = dehacked.max_bonus_armor;
	mobjinfo[55].inventory.count = dehacked.hp_soulsphere;
	mobjinfo[55].inventory.max_count = dehacked.max_soulsphere;

	//
	// PASS 1

	// count actors, allocate actor names
	if(mod_config.enable_decorate)
		wad_handle_lump("DECORATE", cb_count_actors);

	//
	// PASS 2

	// states
	states = ldr_malloc(NUMSTATES * sizeof(state_t));

	// copy original stuff
	for(uint32_t i = 0; i < NUMSTATES; i++)
	{
		states[i].sprite = deh_states[i].sprite;
		states[i].frame = deh_states[i].frame;
		states[i].arg = NULL;
		states[i].tics = deh_states[i].tics;
		states[i].action = deh_states[i].action;
		states[i].nextstate = deh_states[i].nextstate;
		states[i].misc1 = deh_states[i].misc1;
		states[i].misc2 = deh_states[i].misc2;

		// action checks
		if(states[i].action)
		for(uint32_t j = 0; j < NUM_CODEPTR_MODS; j++)
		{
			if(doom_codeptr[j].codeptr + doom_code_segment == states[i].action)
			{
				states[i].action = doom_codeptr[j].func;
				states[i].arg = doom_codeptr[j].arg;
			}
		}
	}

	// process actors
	if(mod_config.enable_decorate)
		wad_handle_lump("DECORATE", cb_parse_actors);

	//
	// PASS 3

	// relocate extra storage
	for(uint32_t idx = NUMMOBJTYPES + NUM_NEW_TYPES; idx < num_mobj_types; idx++)
	{
		mobjinfo_t *info = mobjinfo + idx;
		void *target = states + info->state_idx_limit;

		// state arguments
		for(uint32_t i = info->state_idx_first; i < info->state_idx_limit; i++)
			states[i].arg = relocate_estorage(target, (void*)states[i].arg);

		// drop item list / start inventory list
		if(info->extra_stuff[0])
		{
			info->extra_stuff[0] = relocate_estorage(target, info->extra_stuff[0]);
			info->extra_stuff[1] = relocate_estorage(target, info->extra_stuff[1]);
		}

		// PlayerPawn
		if(info->extra_type == ETYPE_PLAYERPAWN)
		{
			// weapon slots
			for(uint32_t i = 0; i < NUM_WPN_SLOTS; i++)
				info->player.wpn_slot[i] = relocate_estorage(target, info->player.wpn_slot[i]);
		}

		// Inventory stuff
		if(inventory_is_valid(info) || info->extra_type == ETYPE_HEALTH || info->extra_type == ETYPE_INV_SPECIAL)
			info->inventory.message = relocate_estorage(target, info->inventory.message);
	}

	// extra inventory stuff
	for(uint32_t idx = 0; idx < num_mobj_types; idx++)
	{
		mobjinfo_t *info = mobjinfo + idx;

		switch(info->extra_type)
		{
			case ETYPE_PLAYERPAWN:
				// check weapon slots
				for(uint32_t i = 0; i < NUM_WPN_SLOTS; i++)
				{
					uint16_t *ptr;

					ptr = info->player.wpn_slot[i];
					if(!ptr)
						continue;

					while(*ptr)
					{
						uint16_t type = *ptr++;
						if(mobjinfo[type].extra_type != ETYPE_WEAPON)
							I_Error("[DECORATE] Non-weapon in weapon slot!");
					}
				}
			break;
			case ETYPE_WEAPON:
				if(	(info->weapon.ammo_type[0] && mobjinfo[info->weapon.ammo_type[0]].extra_type != ETYPE_AMMO && mobjinfo[info->weapon.ammo_type[0]].extra_type != ETYPE_AMMO_LINK) ||
					(info->weapon.ammo_type[1] && mobjinfo[info->weapon.ammo_type[1]].extra_type != ETYPE_AMMO && mobjinfo[info->weapon.ammo_type[1]].extra_type != ETYPE_AMMO_LINK)
				)
					I_Error("[DECORATE] Invalid weapon ammo!");
				info->eflags &= ~MFE_INVENTORY_INVBAR;
			break;
			case ETYPE_AMMO:
				info->eflags &= ~MFE_INVENTORY_INVBAR;
				// ZDoom - allow 0, but no specific number
				if(info->inventory.hub_count)
					info->inventory.hub_count = INV_MAX_COUNT;
			break;
			case ETYPE_AMMO_LINK:
			{
				mobjinfo_t *ofni = mobjinfo + info->inventory.special;

				if(ofni->extra_type != ETYPE_AMMO)
					I_Error("[DECORATE] Invalid inheritted ammo!");

				// copy only max amounts, for faster access later
				info->inventory.max_count = ofni->inventory.max_count;
				info->inventory.hub_count = ofni->inventory.hub_count;
				info->ammo.count = ofni->ammo.count;
				info->ammo.max_count = ofni->ammo.max_count;
			}
			break;
			case ETYPE_KEY:
				info->eflags &= ~MFE_INVENTORY_INVBAR;
				info->inventory.count = 1;
				info->inventory.max_count = 1;
				// ZDoom - allow 0, but no specific number
				if(info->inventory.hub_count)
					info->inventory.hub_count = 1;
			break;
			case ETYPE_POWERUP:
				// apply default duration if unspecified
				if(!info->powerup.duration && info->powerup.type < NUMPOWERS)
					info->powerup.duration = powerup_type[info->powerup.type].duration;
			break;
		}
	}

	//
	// DONE

	// patch CODE - 'states'
	for(uint32_t i = 0; i < NUM_STATE_HOOKS; i++)
		hook_states[i].value += (uint32_t)states;
	utils_install_hooks(hook_states, NUM_STATE_HOOKS);

	// done
	*numsprites = num_spr_names;

	//
	// KEYCONF

	if(mod_config.enable_decorate)
		parse_keyconf();

	if(!num_player_classes)
		I_Error("No player classes defined!");
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
static uint32_t check_step_height(fixed_t floorz, mobj_t *mo)
{
	if(mo->flags & MF_MISSILE && *demoplayback != DEMO_OLD)
	{
		// projectiles can't step-up
		if(floorz > mo->z)
			return 1;
		return 0;
	}

	// step height
	if(floorz - mo->z > mo->info->step_height)
		return 1;

	// dropoff
	if(	!(mo->flags & (MF_DROPOFF|MF_FLOAT)) &&
		floorz - *tmdropoffz > mo->info->dropoff
	)
		return 1;

	return 0;
}

//
// hooks

static hook_t hook_states[NUM_STATE_HOOKS] =
{
	{0x000315D9, CODE_HOOK | HOOK_UINT32, 0}, // P_SpawnMobj
};

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x00015800, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spr_names},
	{0x0005C8E0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsprites},
	// hook step height check in 'P_TryMove'
	{0x0002B27C, CODE_HOOK | HOOK_UINT16, 0xF289},
	{0x0002B27E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)check_step_height},
	{0x0002B283, CODE_HOOK | HOOK_UINT16, 0xC085},
	{0x0002B285, CODE_HOOK | HOOK_UINT32, 0x1FEB2B74},
	// use 'MF1_NOTELEPORT' in 'EV_Teleport'
	{0x00031E4D, CODE_HOOK | HOOK_UINT8, offsetof(mobj_t, flags1)},
	{0x00031E4E, CODE_HOOK | HOOK_UINT8, MF1_NOTELEPORT},
	// disable teleport sounds
	{0x00031F54, CODE_HOOK | HOOK_UINT16, 0x08EB}, // EV_Teleport
	{0x00031FAE, CODE_HOOK | HOOK_UINT16, 0x08EB}, // EV_Teleport
	{0x00020BA2, CODE_HOOK | HOOK_UINT16, 0x0AEB}, // G_CheckSpot
	// use 'MF1_TELESTOMP' in 'PIT_StompThing'
	{0x0002ABC7, CODE_HOOK | HOOK_UINT16, 0x43F6},
	{0x0002ABC9, CODE_HOOK | HOOK_UINT8, offsetof(mobj_t, flags1)},
	{0x0002ABCA, CODE_HOOK | HOOK_UINT8, MF1_TELESTOMP},
	{0x0002ABCB, CODE_HOOK | HOOK_SET_NOPS, 3},
	// change damage in 'PIT_StompThing'
	{0x0002ABDE, CODE_HOOK | HOOK_UINT32, 1000000},
	// fix 'R_ProjectSprite'; use new 'frame' and 'state', ignore invalid sprites
	{0x00037D45, CODE_HOOK | HOOK_UINT32, 0x2446B70F},
	{0x00037D49, CODE_HOOK | HOOK_UINT32, 0x1072E839},
	{0x00037D4D, CODE_HOOK | HOOK_JMP_DOOM, 0x00037F86},
	{0x00037D6C, CODE_HOOK | HOOK_UINT32, 0x2656B70F},
	{0x00037D70, CODE_HOOK | HOOK_UINT32, 0xE283038B},
	{0x00037D74, CODE_HOOK | HOOK_UINT32, 0x7CC2393F},
	{0x00037D78, CODE_HOOK | HOOK_UINT8, 0x1F},
	{0x00037D79, CODE_HOOK | HOOK_JMP_DOOM, 0x00037F86},
	{0x00037F59, CODE_HOOK | HOOK_UINT8, 0x27},
	// fix 'R_DrawPSprite'; use new 'frame' and 'state', ignore invalid sprites
	{0x00038023, CODE_HOOK | HOOK_UINT32, 0x3910B70F},
	{0x00038027, CODE_HOOK | HOOK_UINT32, 0x1172DA},
	{0x0003802A, CODE_HOOK | HOOK_JMP_DOOM, 0x00038203},
	{0x0003804A, CODE_HOOK | HOOK_UINT32, 0x0258B70F},
	{0x0003804E, CODE_HOOK | HOOK_UINT32, 0xE3833A8B},
	{0x00038052, CODE_HOOK | HOOK_UINT32, 0x7CFB393F},
	{0x00038056, CODE_HOOK | HOOK_UINT8, 0x21},
	{0x00038057, CODE_HOOK | HOOK_JMP_DOOM, 0x00038203},
	{0x000381DD, CODE_HOOK | HOOK_UINT8, 0x03},
};

