// kgsws' ACE Engine
////
// Subset of DECORATE. Yeah!
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "dehacked.h"
#include "decorate.h"
#include "ldr_sprite.h"
#include "inventory.h"
#include "filebuf.h"
#include "action.h"
#include "weapon.h"
#include "sound.h"
#include "mobj.h"
#include "map.h"
#include "demo.h"
#include "render.h"
#include "config.h"
#include "textpars.h"

#include "decodoom.h"

#define NUM_NEW_TYPES	(NEW_NUMMOBJTYPES - NUMMOBJTYPES)
#define NUM_NEW_STATES	(NEW_NUMSTATES - NUMSTATES)

#define NUM_STATE_HOOKS	1

// these are calculated to fit into 12288 bytes
#define CUSTOM_STATE_STORAGE	((custom_state_t*)ptr_drawsegs)
#define MAX_CUSTOM_STATES	(12288 / sizeof(custom_state_t))

enum
{
	DT_U8,
	DT_U16,
	DT_S32,
	DT_FIXED,
	DT_ALIAS64,
	DT_SOUND,
	DT_STRING,
	DT_ICON,
	DT_SKIP1,
	DT_PAINCHANCE,
	DT_ARGS,
	DT_DAMAGE_TYPE,
	DT_DAMAGE_FACTOR,
	DT_RENDER_STYLE,
	DT_RENDER_ALPHA,
	DT_TRANSLATION,
	DT_BLOODLATION,
	DT_POWERUP_TYPE,
	DT_POWERUP_MODE,
	DT_POWERUP_COLOR,
	DT_MONSTER,
	DT_PROJECTILE,
	DT_MOBJTYPE,
	DT_DAMAGE,
	DT_SOUND_CLASS,
	DT_DROPITEM,
	DT_PP_WPN_SLOT,
	DT_PP_INV_SLOT,
	DT_COLOR_RANGE,
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
	const mobjinfo_t *def;
	const dec_attr_t *attr[2];
	const dec_flag_t *flag[2];
} dec_inherit_t;

typedef struct
{
	const uint8_t *name;
	int32_t duration;
	uint32_t flags;
	uint8_t mode;
	uint8_t strength;
	uint8_t colorstuff;
} dec_powerup_t;

typedef struct
{
	const uint8_t *name;
	uint8_t colorstuff;
} dec_power_color_t;

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
	uint8_t colorstuff;
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

uint32_t sprite_table[MAX_SPRITE_NAMES];

uint32_t num_mobj_types = NEW_NUMMOBJTYPES;
mobjinfo_t *mobjinfo;

uint32_t num_states = NEW_NUMSTATES;
state_t *states;

uint32_t num_player_classes = 1;
uint16_t player_class[MAX_PLAYER_CLASSES];

static hook_t hook_states[NUM_STATE_HOOKS];

void *dec_es_ptr;
static uint32_t num_custom_states;

uint8_t *parse_actor_name;

static mobjinfo_t *parse_mobj_info;
static uint32_t *parse_next_state;
static uint16_t *parse_next_extra;
static uint32_t parse_last_anim;
static uint16_t *parse_anim_slot;

static void *extra_stuff_cur;
static void *extra_stuff_next;

static uint32_t extra_storage_size;

uint32_t dec_mod_csum;

//
static uint32_t parse_args();
static uint32_t parse_damage();
static uint32_t parse_player_sounds();
static uint32_t parse_dropitem();
static uint32_t parse_wpn_slot();
static uint32_t parse_inv_slot();
static uint32_t parse_states();

// 'DoomPlayer' stuff
static const ei_player_t ei_player =
{
	.view_height = 41 * FRACUNIT,
	.attack_offs = 8 * FRACUNIT,
	.view_bob = FRACUNIT,
	.name = "Marine",
	.sound =
	{
		.death = 57,
		.xdeath = 58,
		.gibbed = 31,
		.pain = {25, 25, 25, 25},
		.land = 34,
		.usefail = 81,
	}
};

// default mobj
static const mobjinfo_t default_mobj =
{
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.camera_height = -1,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.blood_type = 38,
	.telefog[0] = 39,
	.telefog[1] = 39,
	.gravity = FRACUNIT,
	.bounce_factor = 0xB333,
	.scale = FRACUNIT,
	.range_melee = 44 * FRACUNIT,
	.render_alpha = 255,
};

// default 'PlayerPawn'
static const mobjinfo_t default_player =
{
	.spawnhealth = 100,
	.radius = 16 << FRACBITS,
	.height = 56 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.camera_height = -1,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.blood_type = 38,
	.telefog[0] = 39,
	.telefog[1] = 39,
	.gravity = FRACUNIT,
	.bounce_factor = 0xB333,
	.scale = FRACUNIT,
	.range_melee = 44 * FRACUNIT,
	.render_alpha = 255,
	.painchance[DAMAGE_NORMAL] = 255,
	.speed = 1 << FRACBITS,
	.flags = MF_SOLID | MF_SHOOTABLE | MF_DROPOFF | MF_PICKUP | MF_NOTDMATCH | MF_SLIDE,
	.flags1 = MF1_TELESTOMP,
	.player.view_height = 41 << FRACBITS,
	.player.attack_offs = 8 << FRACBITS,
	.player.jump_z = 8 << FRACBITS,
	.player.view_bob = FRACUNIT,
};

// default 'PlayerChunk'
static const mobjinfo_t default_plrchunk =
{
	.spawnhealth = 100,
	.radius = 16 << FRACBITS,
	.height = 56 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.camera_height = -1,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.blood_type = 38,
	.telefog[0] = 39,
	.telefog[1] = 39,
	.gravity = FRACUNIT,
	.bounce_factor = 0xB333,
	.scale = FRACUNIT,
	.range_melee = 44 * FRACUNIT,
	.render_alpha = 255,
	.painchance[DAMAGE_NORMAL] = 255,
	.speed = 1 << FRACBITS,
	.flags = MF_DROPOFF,
	.player.view_height = 41 << FRACBITS,
	.player.attack_offs = 8 << FRACBITS,
	.player.jump_z = 8 << FRACBITS,
	.player.view_bob = FRACUNIT,
};

// default 'Health'
static const mobjinfo_t default_health =
{
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.step_height = 24 * FRACUNIT,
	.camera_height = -1,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.blood_type = 38,
	.telefog[0] = 39,
	.telefog[1] = 39,
	.gravity = FRACUNIT,
	.bounce_factor = 0xB333,
	.scale = FRACUNIT,
	.range_melee = 44 * FRACUNIT,
	.render_alpha = 255,
	.flags = MF_SPECIAL,
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
	.camera_height = -1,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.blood_type = 38,
	.telefog[0] = 39,
	.telefog[1] = 39,
	.gravity = FRACUNIT,
	.bounce_factor = 0xB333,
	.scale = FRACUNIT,
	.range_melee = 44 * FRACUNIT,
	.render_alpha = 255,
	.flags = MF_SPECIAL,
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
	.camera_height = -1,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.blood_type = 38,
	.telefog[0] = 39,
	.telefog[1] = 39,
	.gravity = FRACUNIT,
	.bounce_factor = 0xB333,
	.scale = FRACUNIT,
	.range_melee = 44 * FRACUNIT,
	.render_alpha = 255,
	.flags = MF_SPECIAL,
	.weapon.inventory.count = 1,
	.weapon.inventory.max_count = 1,
	.weapon.inventory.hub_count = INV_MAX_COUNT,
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
	.camera_height = -1,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.blood_type = 38,
	.telefog[0] = 39,
	.telefog[1] = 39,
	.gravity = FRACUNIT,
	.bounce_factor = 0xB333,
	.scale = FRACUNIT,
	.range_melee = 44 * FRACUNIT,
	.render_alpha = 255,
	.flags = MF_SPECIAL,
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
	.camera_height = -1,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.blood_type = 38,
	.telefog[0] = 39,
	.telefog[1] = 39,
	.gravity = FRACUNIT,
	.bounce_factor = 0xB333,
	.scale = FRACUNIT,
	.range_melee = 44 * FRACUNIT,
	.render_alpha = 255,
	.flags = MF_SPECIAL | MF_NOTDMATCH,
	.flags1 = MF1_DONTGIB,
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
	.camera_height = -1,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.blood_type = 38,
	.telefog[0] = 39,
	.telefog[1] = 39,
	.gravity = FRACUNIT,
	.bounce_factor = 0xB333,
	.scale = FRACUNIT,
	.range_melee = 44 * FRACUNIT,
	.render_alpha = 255,
	.flags = MF_SPECIAL,
	.eflags = MFE_INVENTORY_AUTOACTIVATE,
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
	.camera_height = -1,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.blood_type = 38,
	.telefog[0] = 39,
	.telefog[1] = 39,
	.gravity = FRACUNIT,
	.bounce_factor = 0xB333,
	.scale = FRACUNIT,
	.range_melee = 44 * FRACUNIT,
	.render_alpha = 255,
	.flags = MF_SPECIAL,
	.eflags = MFE_INVENTORY_AUTOACTIVATE | MFE_INVENTORY_ALWAYSPICKUP,
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
	.camera_height = -1,
	.dropoff = 24 * FRACUNIT,
	.mass = 100,
	.blood_type = 38,
	.telefog[0] = 39,
	.telefog[1] = 39,
	.gravity = FRACUNIT,
	.bounce_factor = 0xB333,
	.scale = FRACUNIT,
	.range_melee = 44 * FRACUNIT,
	.render_alpha = 255,
	.flags = MF_SPECIAL,
	.eflags = MFE_INVENTORY_ALWAYSPICKUP,
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
	{"heal", ANIM_HEAL, ETYPE_NONE, offsetof(mobjinfo_t, state_heal)},
	{"crush", ANIM_CRUSH, ETYPE_NONE, offsetof(mobjinfo_t, state_crush)},
	{"crash", ANIM_CRASH, ETYPE_NONE, offsetof(mobjinfo_t, state_crash)},
	{"crash.extreme", ANIM_XCRASH, ETYPE_NONE, offsetof(mobjinfo_t, state_xcrash)},
	// switchable decoration
	{"active", ANIM_S_ACTIVE, ETYPE_SWITCHABLE, offsetof(mobjinfo_t, st_switchable.active)},
	{"inactive", ANIM_S_INACTIVE, ETYPE_SWITCHABLE, offsetof(mobjinfo_t, st_switchable.inactive)},
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
	{"painchance", DT_PAINCHANCE},
	{"damage", DT_DAMAGE, offsetof(mobjinfo_t, damage)},
	{"damagetype", DT_DAMAGE_TYPE},
	{"damagefactor", DT_DAMAGE_FACTOR},
	{"speed", DT_FIXED, offsetof(mobjinfo_t, speed)},
	//
	{"radius", DT_FIXED, offsetof(mobjinfo_t, radius)},
	{"height", DT_FIXED, offsetof(mobjinfo_t, height)},
	{"mass", DT_S32, offsetof(mobjinfo_t, mass)},
	{"gravity", DT_FIXED, offsetof(mobjinfo_t, gravity)},
	//
	{"cameraheight", DT_FIXED, offsetof(mobjinfo_t, camera_height)},
	//
	{"fastspeed", DT_FIXED, offsetof(mobjinfo_t, fast_speed)},
	{"vspeed", DT_FIXED, offsetof(mobjinfo_t, vspeed)},
	//
	{"bouncecount", DT_U16, offsetof(mobjinfo_t, bounce_count)},
	{"bouncefactor", DT_FIXED, offsetof(mobjinfo_t, bounce_factor)},
	//
	{"species", DT_ALIAS64, offsetof(mobjinfo_t, species)},
	//
	{"telefogsourcetype", DT_MOBJTYPE, offsetof(mobjinfo_t, telefog[0])},
	{"telefogdesttype", DT_MOBJTYPE, offsetof(mobjinfo_t, telefog[1])},
	//
	{"bloodtype", DT_MOBJTYPE, offsetof(mobjinfo_t, blood_type)},
	{"bloodcolor", DT_BLOODLATION},
	//
	{"dropitem", DT_DROPITEM},
	//
	{"activesound", DT_SOUND, offsetof(mobjinfo_t, activesound)},
	{"attacksound", DT_SOUND, offsetof(mobjinfo_t, attacksound)},
	{"deathsound", DT_SOUND, offsetof(mobjinfo_t, deathsound)},
	{"painsound", DT_SOUND, offsetof(mobjinfo_t, painsound)},
	{"seesound", DT_SOUND, offsetof(mobjinfo_t, seesound)},
	{"bouncesound", DT_SOUND, offsetof(mobjinfo_t, bouncesound)},
	//
	{"maxstepheight", DT_FIXED, offsetof(mobjinfo_t, step_height)},
	{"maxdropoffheight", DT_FIXED, offsetof(mobjinfo_t, dropoff)},
	{"meleerange", DT_FIXED, offsetof(mobjinfo_t, range_melee)},
	//
	{"scale", DT_FIXED, offsetof(mobjinfo_t, scale)},
	//
	{"args", DT_ARGS},
	//
	{"renderstyle", DT_RENDER_STYLE},
	{"alpha", DT_RENDER_ALPHA},
	{"translation", DT_TRANSLATION},
	//
	{"monster", DT_MONSTER},
	{"projectile", DT_PROJECTILE},
	{"states", DT_STATES},
	//
	{"obituary", DT_SKIP1},
	{"hitobituary", DT_SKIP1},
	{"decal", DT_SKIP1},
	// terminator
	{NULL}
};

// mobj flags
const dec_flag_t mobj_flags0[] =
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
const dec_flag_t mobj_flags1[] =
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
	{"dontgib", MF1_DONTGIB},
	{"invulnerable", MF1_INVULNERABLE},
	{"buddha", MF1_BUDDHA},
	{"nodamage", MF1_NODAMAGE},
	{"reflective", MF1_REFLECTIVE},
	{"boss", MF1_BOSS}, // TODO: implement in A_Look (see sound)
	{"noradiusdmg", MF1_NORADIUSDMG},
	{"extremedeath", MF1_EXTREMEDEATH},
	{"noextremedeath", MF1_NOEXTREMEDEATH},
	{"skyexplode", MF1_SKYEXPLODE},
	{"ripper", MF1_RIPPER},
	{"dontrip", MF1_DONTRIP},
	{"pushable", MF1_PUSHABLE},
	{"cannotpush", MF1_CANNOTPUSH},
	{"puffonactors", MF1_PUFFONACTORS},
	{"spectral", MF1_SPECTRAL},
	{"ghost", MF1_GHOST},
	{"thrughost", MF1_THRUGHOST},
	{"dormant", MF1_DORMANT},
	{"painless", MF1_PAINLESS},
	{"iceshatter", MF1_ICESHATTER},
	{"dontfall", MF1_DONTFALL},
	// terminator
	{NULL}
};
const dec_flag_t mobj_flags2[] =
{
	{"noicedeath", MF2_NOICEDEATH},
	{"icecorpse", MF2_ICECORPSE},
	{"spawnsoundsource", MF2_SPAWNSOUNDSOURCE},
	{"donttranslate", MF2_DONTTRANSLATE},
	{"notautoaimed", MF2_NOTAUTOAIMED},
	{"puffgetsowner", MF2_PUFFGETSOWNER},
	{"hittarget", MF2_HITTARGET},
	{"hitmaster", MF2_HITMASTER},
	{"hittracer", MF2_HITTRACER},
	{"movewithsector", MF2_MOVEWITHSECTOR},
	{"fullvoldeath", MF2_FULLVOLDEATH},
	{"oldradiusdmg", MF2_OLDRADIUSDMG},
	{"bloodlessimpact", MF2_BLOODLESSIMPACT},
	{"synchronized", MF2_SYNCHRONIZED},
	{"dontmorph", MF2_DONTMORPH},
	{"dontsplash", MF2_DONTSPLASH},
	{"thruactors", MF2_THRUACTORS},
	{"bounceonfloors", MF2_BOUNCEONFLOORS},
	{"bounceonceilings", MF2_BOUNCEONCEILINGS},
	{"noblockmonst", MF2_NOBLOCKMONST},
	{"dontcorpse", MF2_DONTCORPSE},
	{"stealth", MF2_STEALTH},
	{"explodeonwater", MF2_EXPLODEONWATER},
	{"canbouncewater", MF2_CANBOUNCEWATER},
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
	{"weapon.dontbob", MFE_WEAPON_DONTBOB},
	// dummy
	{"weapon.wimpy_weapon", 0},
	{"weapon.meleeweapon", 0},
	// terminator
	{NULL}
};

// 'PlayerPawn' attributes
static const dec_attr_t attr_player[] =
{
	{"player.viewheight", DT_FIXED, offsetof(mobjinfo_t, player.view_height)},
	{"player.attackzoffset", DT_FIXED, offsetof(mobjinfo_t, player.attack_offs)},
	{"player.jumpz", DT_FIXED, offsetof(mobjinfo_t, player.jump_z)},
	{"player.viewbob", DT_FIXED, offsetof(mobjinfo_t, player.view_bob)},
	{"player.displayname", DT_STRING, offsetof(mobjinfo_t, player.name)},
	{"player.soundclass", DT_SOUND_CLASS},
	//
	{"player.weaponslot", DT_PP_WPN_SLOT},
	{"player.startitem", DT_PP_INV_SLOT},
	//
	{"player.colorrange", DT_COLOR_RANGE},
	//
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
	{"powerup.color", DT_POWERUP_COLOR},
	{"powerup.strength", DT_U8, offsetof(mobjinfo_t, powerup.strength)},
	// terminator
	{NULL}
};

// player sound slots
static const uint8_t *player_sound_slot[NUM_PLAYER_SOUNDS] =
{
	[PLR_SND_DEATH] = "*death",
	[PLR_SND_XDEATH] = "*xdeath",
	[PLR_SND_GIBBED] = "*gibbed",
	[PLR_SND_PAIN25] = "*pain25",
	[PLR_SND_PAIN50] = "*pain50",
	[PLR_SND_PAIN75] = "*pain75",
	[PLR_SND_PAIN100] = "*pain100",
	[PLR_SND_LAND] = "*land",
	[PLR_SND_USEFAIL] = "*usefail",
	[PLR_SND_JUMP] = "*jump",
};

// damage types
dec_damage_type damage_type_config[NUM_DAMAGE_TYPES] =
{
	[DAMAGE_NORMAL] = {.name = "normal"},
	[DAMAGE_ICE] = {.name = "ice"},
	[DAMAGE_FIRE] = {.name = "fire"},
	[DAMAGE_SLIME] = {.name = "slime"},
	[DAMAGE_CRUSH] = {.name = "crush"},
	[DAMAGE_FALLING] = {.name = "falling"},
	[DAMAGE_INSTANT] = {.name = "instantdeath"},
	[DAMAGE_TELEFRAG] = {.name = "telefrag"},
	[DAMAGE_DROWN] = {.name = "drowning"},
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
	[ETYPE_PLAYERCHUNK] =
	{
		.name = "PlayerChunk",
		.def = &default_plrchunk,
		.attr[0] = attr_player
	},
	[ETYPE_SWITCHABLE] =
	{
		.name = "SwitchableDecoration",
		.def = &default_mobj
	},
	[ETYPE_RANDOMSPAWN] =
	{
		.name = "RandomSpawner",
		.def = &default_mobj
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

// this only exists because original animations are all over the place in 'mobjinfo_t'
const uint16_t base_anim_offs[LAST_MOBJ_STATE_HACK] =
{
	[ANIM_SPAWN] = offsetof(mobjinfo_t, state_spawn),
	[ANIM_SEE] = offsetof(mobjinfo_t, state_see),
	[ANIM_PAIN] = offsetof(mobjinfo_t, state_pain),
	[ANIM_MELEE] = offsetof(mobjinfo_t, state_melee),
	[ANIM_MISSILE] = offsetof(mobjinfo_t, state_missile),
	[ANIM_DEATH] = offsetof(mobjinfo_t, state_death),
	[ANIM_XDEATH] = offsetof(mobjinfo_t, state_xdeath),
	[ANIM_RAISE] = offsetof(mobjinfo_t, state_raise),
};

// internal states
static const state_t internal_states[] =
{
	[STATE_UNKNOWN_ITEM - NUMSTATES] =
	{
		.sprite = 28, // TODO: custom sprite
		.frame = 0,
		.tics = 0xFFFF,
		.nextstate = 0, // this state is used for delayed deletion so 'next' has to be zero
	},
	[STATE_PISTOL - NUMSTATES] =
	{
		.sprite = SPR_PIST,
		.frame = 0,
		.tics = 0xFFFF,
		.nextstate = 0,
	},
	[STATE_ICE_DEATH_0 - NUMSTATES] =
	{
		.sprite = 0xFFFF,
		.frame = 0,
		.tics = 0xFFFF,
		.nextstate = STATE_ICE_DEATH_1,
		.acp = A_GenericFreezeDeath,
	},
	[STATE_ICE_DEATH_1 - NUMSTATES] =
	{
		.sprite = 0xFFFF,
		.frame = 0,
		.tics = 0xFFFF,
		.nextstate = STATE_ICE_DEATH_1,
		.acp = A_FreezeDeathChunks,
	},
	[STATE_ICE_CHUNK_0 - NUMSTATES] =
	{
		.sprite = SPR_ICEC,
		.frame = 0,
		.tics = 1,
		.nextstate = STATE_ICE_CHUNK_1,
	},
	[STATE_ICE_CHUNK_1 - NUMSTATES] =
	{
		.sprite = SPR_ICEC,
		.frame = 0,
		.tics = 1,
		.nextstate = STATE_ICE_CHUNK_2,
		.acp = A_IceSetTics,
	},
	[STATE_ICE_CHUNK_2 - NUMSTATES] =
	{
		.sprite = SPR_ICEC,
		.frame = 1,
		.tics = 1,
		.nextstate = STATE_ICE_CHUNK_3,
		.acp = A_IceSetTics,
	},
	[STATE_ICE_CHUNK_3 - NUMSTATES] =
	{
		.sprite = SPR_ICEC,
		.frame = 2,
		.tics = 1,
		.nextstate = STATE_ICE_CHUNK_4,
		.acp = A_IceSetTics,
	},
	[STATE_ICE_CHUNK_4 - NUMSTATES] =
	{
		.sprite = SPR_ICEC,
		.frame = 3,
		.tics = 1,
		.nextstate = 0,
		.acp = A_IceSetTics,
	},
	[STATE_ICE_CHUNK_PLR - NUMSTATES] =
	{
		.sprite = SPR_ICEC,
		.frame = 0,
		.tics = 10,
		.nextstate = STATE_ICE_CHUNK_PLR,
		.acp = A_CheckPlayerDone,
	},
	[STATE_SPECIAL_HIDE - NUMSTATES] =
	{
		.sprite = SPR_ICEC,
		.frame = 0,
		.tics = 10,
		.nextstate = STATE_SPECIAL_RESTORE,
		.acp = A_SpecialHide,
	},
	[STATE_SPECIAL_RESTORE - NUMSTATES] =
	{
		.sprite = SPR_ICEC,
		.frame = 0,
		.tics = 10,
		.nextstate = 0,
		.acp = A_SpecialRestore,
	},
};

// internal types
static const mobjinfo_t internal_mobj_info[NUM_NEW_TYPES] =
{
	[MOBJ_IDX_UNKNOWN - NUMMOBJTYPES] =
	{
		.alias = 0xFFFFFFFFFFFFFFFF, // for save game
		.spawnhealth = 1000,
		.mass = 100,
		.blood_type = 38,
		.telefog[0] = 39,
		.telefog[1] = 39,
		.gravity = FRACUNIT,
		.bounce_factor = 0xB333,
		.scale = FRACUNIT,
		.range_melee = 44 * FRACUNIT,
		.flags = MF_NOGRAVITY,
		.state_spawn = STATE_UNKNOWN_ITEM,
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
		.blood_type = 38,
		.telefog[0] = 39,
		.telefog[1] = 39,
		.gravity = FRACUNIT,
		.bounce_factor = 0xB333,
		.scale = FRACUNIT,
		.range_melee = 44 * FRACUNIT,
		.flags = MF_SPECIAL,
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
		.doomednum = 5010,
		.spawnhealth = 1000,
		.reactiontime = 8,
		.radius = 20 << FRACBITS,
		.height = 16 << FRACBITS,
		.mass = 100,
		.blood_type = 38,
		.telefog[0] = 39,
		.telefog[1] = 39,
		.gravity = FRACUNIT,
		.bounce_factor = 0xB333,
		.scale = FRACUNIT,
		.range_melee = 44 * FRACUNIT,
		.flags = MF_SPECIAL,
		.state_spawn = STATE_PISTOL,
		.extra_type = ETYPE_WEAPON,
		.weapon.inventory.count = 1,
		.weapon.inventory.max_count = 1,
		.weapon.inventory.hub_count = 1,
		.weapon.inventory.sound_pickup = 33,
		.weapon.kickback = 100,
	},
	[MOBJ_IDX_ICE_CHUNK - NUMMOBJTYPES] =
	{
		.alias = 0x0000AEED680E58C9,
		.spawnhealth = 1000,
		.radius = 3 << FRACBITS,
		.height = 4 << FRACBITS,
		.mass = 5,
		.blood_type = 38,
		.telefog[0] = 39,
		.telefog[1] = 39,
		.gravity = (FRACUNIT * 128) / 1000,
		.bounce_factor = 0xB333,
		.scale = FRACUNIT,
		.range_melee = 44 * FRACUNIT,
		.flags = MF_DROPOFF | MF_NOBLOCKMAP,
		.flags1 = MF1_NOTELEPORT | MF1_CANNOTPUSH,
		.flags2 = MF2_MOVEWITHSECTOR,
		.state_spawn = STATE_ICE_CHUNK_0,
	},
	[MOBJ_IDX_ICE_CHUNK_HEAD - NUMMOBJTYPES] =
	{
		.alias = 0x1948AEED680E585B,
		.spawnhealth = 1000,
		.radius = 3 << FRACBITS,
		.height = 4 << FRACBITS,
		.mass = 5,
		.blood_type = 38,
		.telefog[0] = 39,
		.telefog[1] = 39,
		.gravity = (FRACUNIT * 128) / 1000,
		.bounce_factor = 0xB333,
		.scale = FRACUNIT,
		.range_melee = 44 * FRACUNIT,
		.flags = MF_DROPOFF,
		.flags1 = MF1_CANNOTPUSH,
		.state_spawn = STATE_ICE_CHUNK_PLR,
		.extra_type = ETYPE_PLAYERCHUNK,
		.player.view_height = 2 << FRACBITS,
		.player.attack_offs = 2 << FRACBITS,
	},
};

// powerup types
static const dec_powerup_t powerup_type[NUMPOWERS] =
{
	[pw_invulnerability] = {"invulnerable", -30},
	[pw_strength] = {"strength", 1, MFE_INVENTORY_HUBPOWER},
	[pw_invisibility] = {"invisibility", -60, 0, 0, 52},
	[pw_ironfeet] = {"ironfeet", -60},
	[pw_allmap] = {""}, // this is not a powerup
	[pw_infrared] = {"lightamp", -120, 0, 0, 0, 0x01},
	[pw_buddha] = {"buddha", -60},
	[pw_attack_speed] = {"doublefiringspeed", -45},
	[pw_flight] = {"flight", -20},
	[pw_reserved0] = {""},
	[pw_reserved1] = {""},
	[pw_reserved2] = {""},
};

// powerup colors
static const dec_power_color_t powerup_color[] =
{
	{"inversemap", 32},
	{"goldmap", 33},
	{"redmap", 34},
	{"greenmap", 35},
	{"bluemap", 36},
	{"ff 00 00", 2 | 128},
	{"00 ff 00", 13 | 128},
	{"ff ff 00", 10 | 128},
	{"00 00 ff", 1 | 128}, // this assumes that PLAYPAL has been changed; this is also used for ice death
	{"ff 00 ff", 9 | 128}, // this assumes that PLAYPAL has been changed; only one choice is valid
	{"00 ff ff", 9 | 128}, // this assumes that PLAYPAL has been changed; only one choice is valid
	{"ff ff ff", 9 | 128}, // this assumes that PLAYPAL has been changed; only one choice is valid
	// terminator
	{NULL}
};

// render styles
static const char *render_style[NUM_RENDER_STYLES] =
{
	[RS_NORMAL] = "normal",
	[RS_FUZZ] = "fuzzy",
	[RS_SHADOW] = "shadow",
	[RS_TRANSLUCENT] = "translucent",
	[RS_ADDITIVE] = "add",
	[RS_INVISIBLE] = "none",
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
	{56, pw_invulnerability, 0x20, (uint8_t*)0x00022EF0}, // InvulnerabilitySphere
	{58, pw_invisibility, 0x00, (uint8_t*)0x00022F10}, // BlurSphere
	{59, pw_ironfeet, 0x8D, (uint8_t*)0x00022F28}, // RadSuit
	{61, pw_infrared, 0x01, (uint8_t*)0x00022F58}, // Infrared
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
	{(void*)0x000276C0, A_Look, NULL},
	{(void*)0x0002D490, A_Lower, &def_LowerRaise},
	{(void*)0x0002D4D0, A_Raise, &def_LowerRaise},
	{(void*)0x0002D2F0, A_WeaponReady, NULL},
	{(void*)0x0002D3F0, A_ReFire, NULL},
	{(void*)0x0002DB40, A_Light0, NULL},
	{(void*)0x0002DB50, A_Light1, NULL},
	{(void*)0x0002DB60, A_Light2, NULL},
	{(void*)0x000288D0, A_Explode, &def_Explode},
	{(void*)0x0002D550, wpn_codeptr, (void*)0x0002D550}, // A_Punch
	{(void*)0x0002D600, wpn_codeptr, (void*)0x0002D600}, // A_Saw
	{(void*)0x0002D890, A_OldBullets, (void*)1}, // A_FirePistol
	{(void*)0x0002D910, A_OldBullets, (void*)2}, // A_FireShotgun
	{(void*)0x0002D9B0, A_OldBullets, (void*)4}, // A_FireShotgun2
	{(void*)0x0002DA70, A_OldBullets, (void*)86}, // A_FireCGun
	{(void*)0x0002D700, A_OldProjectile, (void*)33}, // A_FireMissile
	{(void*)0x0002D730, A_OldProjectile, (void*)35}, // A_FireBFG
	{(void*)0x0002D760, A_OldProjectile, (void*)34}, // A_FirePlasma
	{(void*)0x0002D510, A_DoomGunFlash, NULL},
	{(void*)0x0002D460, A_CheckReload, NULL},
	{(void*)0x00028A80, wpn_sound, (void*)5}, // A_OpenShotgun2
	{(void*)0x00028A90, wpn_sound, (void*)7}, // A_LoadShotgun2
	{(void*)0x00028AA0, wpn_sound, NULL}, // A_CloseShotgun2
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
	void *ptr = dec_es_ptr;

	dec_es_ptr += size;
	if(dec_es_ptr > EXTRA_STORAGE_END)
		engine_error("LOADER", "Extra storage allocation failed!");

	extra_storage_size += size;

	return ptr;
}

//
// animation search

const dec_anim_t *dec_find_animation(const uint8_t *name)
{
	const dec_anim_t *anim = mobj_anim;

	while(anim->name)
	{
		if(	(anim->type == ETYPE_NONE || parse_mobj_info->extra_type == anim->type) &&
			!strcmp(name, anim->name)
		)
			return anim;
		anim++;
	}

	return NULL;
}

//
// decorate states

uint32_t dec_resolve_animation(mobjinfo_t *info, uint32_t offset, uint16_t anim, uint32_t limit)
{
	uint32_t state;

	if(anim < LAST_MOBJ_STATE_HACK)
		state = *((uint16_t*)((void*)info + base_anim_offs[anim]));
	else
	if(anim < NUM_MOBJ_ANIMS)
		state = info->new_states[anim - LAST_MOBJ_STATE_HACK];
	else
		state = info->extra_states[anim - NUM_MOBJ_ANIMS];

	state += offset;
	if(state >= limit)
		engine_error("MOBJ", "State jump '+%u' in '%s' is invalid!", offset, parse_actor_name);

	return state;
}

int32_t dec_find_custom_state(uint32_t alias)
{
	for(uint32_t i = 0; i < num_custom_states; i++)
	{
		if(CUSTOM_STATE_STORAGE[i].alias == alias)
			return i;
	}

	return -1;
}

uint16_t *dec_make_custom_state(uint32_t alias, uint32_t state)
{
	int32_t idx;

	idx = dec_find_custom_state(alias);
	if(idx < 0)
	{
		if(num_custom_states >= MAX_CUSTOM_STATES)
			engine_error("DECORATE", "Too many custom states!");
		idx = num_custom_states++;
		CUSTOM_STATE_STORAGE[idx].alias = alias;
	}

	CUSTOM_STATE_STORAGE[idx].state = state;

	return &CUSTOM_STATE_STORAGE[idx].state;
}

uint32_t dec_mobj_custom_state(mobjinfo_t *info, uint32_t alias)
{
	if(!info->custom_states)
		return 0;

	for(custom_state_t *cst = info->custom_states; cst < info->custom_st_end; cst++)
	{
		if(cst->alias == alias)
			return cst->state;
	}

	return 0;
}

uint32_t dec_reslove_state(mobjinfo_t *info, uint32_t current, uint32_t next, uint32_t extra)
{
	// this is a limited version for weapons and custom inventory
	switch(extra & STATE_CHECK_MASK)
	{
		case STATE_SET_OFFSET:
			next += current;
			if(next >= num_states)
				next = 0;
		break;
		case STATE_SET_CUSTOM:
			next = dec_mobj_custom_state(info, next);
			next += extra & STATE_EXTRA_MASK;
			if(next >= num_states)
				next = 0;
		break;
		case STATE_SET_ANIMATION:
			if(next == 0xFFFFFFFF)
				next = dec_resolve_animation(info, 0, extra & STATE_EXTRA_MASK, num_states);
		break;
	}

	return next;
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
		info->weapon.ammo_give[0] = clipammo[wpn->ammo] * 2;
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

	tmp = clipammo[idx];

	info = mobjinfo + ammo->clp;
	info->extra_type = ETYPE_AMMO;
	info->eflags = MFE_INVENTORY_KEEPDEPLETED; // hack for original status bar
	info->ammo = default_ammo.ammo;
	info->ammo.inventory.count = tmp;
	info->ammo.inventory.max_count = maxammo[idx];
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
	info->powerup.colorstuff = pw->colorstuff;
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

int32_t check_internal_type(uint8_t *name)
{
	// find this class
	for(uint32_t etp = 1; etp < NUM_EXTRA_TYPES; etp++)
	{
		if(inheritance[etp].name && !strcmp(name, inheritance[etp].name))
			return etp;
	}

	return -1;
}

void *dec_reloc_es(void *target, void *ptr)
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

	for(i = 0; i < numsprites; i++)
	{
		if(sprite_table[i] == name)
			return i;
	}

	if(i == MAX_SPRITE_NAMES)
		engine_error("DECORATE", "Too many sprite names!");

	sprite_table[i] = name;

	numsprites++;

	return i;
}

uint32_t dec_get_custom_damage(const uint8_t *name, const uint8_t *mmod)
{
	for(uint32_t i = 0; i < NUM_DAMAGE_TYPES; i++)
	{
		if(damage_type_config[i].name && !strcmp(damage_type_config[i].name, name))
			return i;
	}

	if(!mmod)
		engine_error("DECORATE", "Unknown damage type '%s' in '%s'!", name, parse_actor_name);
	else
		engine_error(mmod, "Unknown damage type '%s'!", name);
}

int32_t dec_get_powerup_type(const uint8_t *name)
{
	for(uint32_t i = 0; i < NUMPOWERS; i++)
	{
		if(!strcmp(powerup_type[i].name, name))
			return i;
	}
	return -1;
}

static uint32_t parse_attr(uint32_t type, void *dest)
{
	num32_t num;
	uint8_t *kw;

	switch(type)
	{
		case DT_U8:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(doom_sscanf(kw, "%u", &num.u32) != 1 || num.u32 > 255)
				return 1;
			*((uint8_t*)dest) = num.u32;
		break;
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
		case DT_ALIAS64:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			*((uint64_t*)dest) = tp_hash64(kw);
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
			patch_t patch;

			kw = tp_get_keyword();
			if(!kw)
				return 1;

			num.s32 = wad_check_lump(kw);
			if(num.s32 < 0)
				num.s32 = 0;
			else
				ldr_get_patch_header(num.s32, &patch);

			*((int32_t*)dest) = num.s32;
		}
		break;
		case DT_SKIP1:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
		break;
		case DT_PAINCHANCE:
		{
			uint8_t *wk;
			uint32_t idx;

			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;

			wk = tp_get_keyword();
			if(!wk)
				return 1;

			if(wk[0] == ',')
			{
				idx = dec_get_custom_damage(kw, NULL);
				wk = tp_get_keyword();
				if(!wk)
					return 1;
			} else
			{
				idx = 0;
				tp_push_keyword(wk);
				wk = kw;
			}

			if(doom_sscanf(wk, "%u", &num.u32) != 1 || num.u32 > 256)
				return 1;

			parse_mobj_info->painchance[idx] = num.u32 | 0x8000;
		}
		break;
		case DT_ARGS:
			return parse_args();
		break;
		case DT_DAMAGE_TYPE:
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			parse_mobj_info->damage_type = dec_get_custom_damage(kw, NULL);
		break;
		case DT_DAMAGE_FACTOR:
		{
			uint32_t type;
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			type = dec_get_custom_damage(kw, NULL);
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			if(kw[0] != ',')
				return 1;
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			if(tp_parse_fixed(kw, &num.s32))
				return 1;
			if(num.s32 < 0)
				num.s32 = FRACUNIT;
			else
			if(num.s32 >= 8192 * FRACUNIT)
				return 1;
			parse_mobj_info->damage_factor[type] = num.s32 >> 13;
		}
		break;
		case DT_RENDER_STYLE:
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			for(num.u32 = 0; num.u32 < NUM_RENDER_STYLES; num.u32++)
			{
				if(!strcmp(render_style[num.u32], kw))
					break;
			}
			if(num.u32 >= NUM_RENDER_STYLES)
				engine_error("DECORATE", "Unknown render style '%s' in '%s'!", kw, parse_actor_name);
			parse_mobj_info->render_style = num.u32;
		break;
		case DT_RENDER_ALPHA:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(tp_parse_fixed(kw, &num.s32))
				return 1;
			if(num.s32 < 0)
				num.s32 = 0;
			num.s32 >>= 8;
			if(num.s32 > 255)
				num.s32 = 255;
			parse_mobj_info->render_alpha = num.s32;
		break;
		case DT_TRANSLATION:
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			parse_mobj_info->translation = r_translation_by_name(kw);
		break;
		case DT_BLOODLATION:
		{
			uint32_t r, g, b;
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			if(doom_sscanf(kw, "%x %x %x", &r, &g, &b) != 3 || (r|g|b) > 255)
				return 1;
			*((uintptr_t*)&parse_mobj_info->blood_trns) = r_add_blood_color(r | (g << 8) | (b << 16));
		}
		break;
		case DT_POWERUP_TYPE:
			if(parse_mobj_info->powerup.type < NUMPOWERS)
				engine_error("DECORATE", "Powerup type specified multiple times in '%s'!", parse_actor_name);
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			num.u32 = dec_get_powerup_type(kw);
			if(num.u32 < 0)
				engine_error("DECORATE", "Unknown powerup type '%s' in '%s'!", kw, parse_actor_name);
			// set type
			parse_mobj_info->powerup.type = num.u32;
			// modify stuff
			parse_mobj_info->eflags |= powerup_type[num.u32].flags;
			parse_mobj_info->powerup.mode = powerup_type[num.u32].mode;
			parse_mobj_info->powerup.strength = powerup_type[num.u32].strength;
			parse_mobj_info->powerup.colorstuff = powerup_type[num.u32].colorstuff;
		break;
		case DT_POWERUP_MODE:
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			switch(parse_mobj_info->powerup.type)
			{
				case pw_invulnerability:
					// 'None' is default so it's not listed
					if(!strcmp("reflective", kw))
						parse_mobj_info->powerup.mode = 1;
					else
						return 1;
				break;
				case pw_invisibility:
					// 'Fuzzy' is default so it's not listed
					if(!strcmp("translucent", kw))
						parse_mobj_info->powerup.mode = 1;
					else
					if(!strcmp("cumulative", kw))
						parse_mobj_info->powerup.mode = 2;
					else
						return 1;
				break;
				default:
					return 1;
			}
		break;
		case DT_POWERUP_COLOR:
		{
			const dec_power_color_t *col = powerup_color;

			if(parse_mobj_info->powerup.type >= NUMPOWERS)
				return 1;

			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;

			while(col->name)
			{
				if(!strcmp(col->name, kw))
					break;
				col++;
			}

			if(!col)
				return 1;

			parse_mobj_info->powerup.colorstuff = col->colorstuff;

			// optional part; for ZDoom compatibility
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(kw[0] == ',')
			{
				// dummy read (alpha value)
				kw = tp_get_keyword();
				if(!kw)
					return 1;
			} else
				tp_push_keyword(kw);
		}
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
				engine_error("DECORATE", "Unable to find ammo type '%s' in '%s'!", kw, parse_actor_name);
			*((uint16_t*)dest) = num.s32;
		break;
		case DT_DAMAGE:
			num.u32 = parse_damage();
			tp_enable_math = 0;
			return num.u32;
		break;
		case DT_SOUND_CLASS:
			return parse_player_sounds();
		break;
		case DT_DROPITEM:
			if(parse_mobj_info->extra_type == ETYPE_PLAYERPAWN)
				engine_error("DECORATE", "Attempt to add DropItem to player class '%s'!", parse_actor_name);
			return parse_dropitem();
		break;
		case DT_PP_WPN_SLOT:
			return parse_wpn_slot();
		break;
		case DT_PP_INV_SLOT:
			return parse_inv_slot();
		break;
		case DT_COLOR_RANGE:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(doom_sscanf(kw, "%u", &num.u32) != 1 || num.u32 > 255)
				return 1;
			parse_mobj_info->player.color_first = num.u32;
			if(!tp_must_get(","))
				return 1;
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(doom_sscanf(kw, "%u", &num.u32) != 1 || num.u32 > 255)
				return 1;
			parse_mobj_info->player.color_last = num.u32;
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

	if(sprname.u32 == 0x2D2D2D2D)
		return 0xFFFF;

	return spr_add_name(sprname.u32);
}

static uint32_t add_states(uint32_t sprite, uint8_t *frames, int32_t tics, uint16_t flags, int32_t ox, int32_t oy)
{
	uint32_t tmp = num_states;

	num_states += strlen(frames);
	states = ldr_realloc(states, num_states * sizeof(state_t));

	for(uint32_t i = tmp; i < num_states; i++)
	{
		state_t *state = states + i;
		uint8_t frm;

		if(*frames < 'A' || *frames > ']')
			engine_error("DECORATE", "Invalid frame '%c' in '%s'!", *frames, parse_actor_name);

		if(parse_next_state)
		{
			*parse_next_state = state - states;
			*parse_next_extra = 0;
		}

		state->sprite = sprite;
		state->frame = (*frames - 'A') | flags;
		state->arg = parse_action_arg;
		state->tics = tics;
		state->action = parse_action_func;
		state->nextstate = 0;
		state->next_extra = 0;
		state->misc1 = ox;
		state->misc2 = oy;

		parse_next_state = &state->nextstate;
		parse_next_extra = &state->next_extra;

		frames++;
		state++;
	}
}

//
// custom parsers

static uint32_t parse_args()
{
	uint32_t value;
	uint8_t *kw;

	parse_mobj_info->args[5] = 0;

	for(uint32_t i = 0; ; i++)
	{
		kw = tp_get_keyword();
		if(!kw)
			return 1;

		if(doom_sscanf(kw, "%d", &value) != 1 || value > 255)
			engine_error("DECORATE", "Unable to parse number '%s' in '%s'!", kw, parse_actor_name);

		parse_mobj_info->args[0] = value;
		parse_mobj_info->args[5] = i + 1;

		if(i == 4)
			return 0;

		kw = tp_get_keyword();
		if(!kw)
			return 1;

		if(kw[0] != ',')
		{
			tp_push_keyword(kw);
			return 0;
		}
	}
}

static uint32_t parse_damage()
{
	uint32_t lo, hi, mul, add;
	uint8_t *kw;

	tp_enable_math = 1;

	kw = tp_get_keyword();
	if(!kw)
		return 1;

	if(kw[0] != '(')
	{
		// just original RNG value
		if(doom_sscanf(kw, "%u", &add) != 1 || add > 0x7FFFFFFF)
			return 1;
		parse_mobj_info->damage = add;

		return 0;
	}

	lo = 0;
	hi = 0;
	mul = 0;
	add = 0;

	kw = tp_get_keyword_lc();
	if(!kw)
		return 1;

	if(!strcmp(kw, "random"))
	{
		// function entry
		kw = tp_get_keyword();
		if(!kw)
			return 1;
		if(kw[0] != '(')
			return 1;

		// low
		kw = tp_get_keyword();
		if(!kw)
			return 1;

		if(doom_sscanf(kw, "%u", &lo) != 1 || lo > 511)
			return 1;

		// comma
		kw = tp_get_keyword();
		if(!kw)
			return 1;
		if(kw[0] != ',')
			return 1;

		// high
		kw = tp_get_keyword();
		if(!kw)
			return 1;

		if(doom_sscanf(kw, "%u", &hi) != 1 || hi > 511)
			return 1;

		// function end
		kw = tp_get_keyword();
		if(!kw)
			return 1;
		if(kw[0] != ')')
			return 1;

		// check range
		if(lo > hi)
			return 1;
		if(hi - lo > 255)
			return 1;

		// next
		kw = tp_get_keyword();
		if(!kw)
			return 1;

		if(kw[0] == '*')
		{
			// multiply
			kw = tp_get_keyword();
			if(!kw)
				return 1;

			if(doom_sscanf(kw, "%u", &mul) != 1 || !mul || mul > 16)
				return 1;

			mul--;

			// next
			kw = tp_get_keyword();
			if(!kw)
				return 1;
		}

		if(kw[0] == '+')
		{
			// add
			kw = tp_get_keyword();
			if(!kw)
				return 1;

			if(doom_sscanf(kw, "%u", &add) != 1 || add > 511)
				return 1;

			// next
			kw = tp_get_keyword();
			if(!kw)
				return 1;
		}
	} else
	{
		// direct value
		if(doom_sscanf(kw, "%u", &add) != 1 || add > 8687)
			return 1;

		if(add > 511)
		{
			// split
			lo = (add - 496) / 16;
			hi = lo;
			add -= lo * 16;
			mul = 15;
		}

		// next
		kw = tp_get_keyword();
		if(!kw)
			return 1;
	}

	// terminator
	if(kw[0] != ')')
		return 1;

	parse_mobj_info->damage = DAMAGE_CUSTOM(lo, hi, mul, add);
}

static uint32_t parse_player_sounds()
{
	uint8_t text[PLAYER_SOUND_CLASS_LEN + PLAYER_SOUND_SLOT_LEN];
	uint8_t *kw;

	kw = tp_get_keyword_lc();
	if(!kw)
		return 1;

	if(strlen(kw) >= PLAYER_SOUND_CLASS_LEN)
		return 1;

	for(uint32_t i = 0; i < NUM_PLAYER_SOUNDS; i++)
	{
		doom_sprintf(text, "%s#%s", kw, player_sound_slot[i]);
		parse_mobj_info->player.sound_slot[i] = sfx_by_name(text);
	}

	return 0;
}

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
			engine_error("DECORATE", "Drop item list is not contiguous in '%s'!", parse_actor_name);
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
		engine_error("DECORATE", "Unknown drop item '%s' in '%s'!", kw, parse_actor_name);
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

	drop->amount = tmp;

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
			engine_error("DECORATE", "Unable to find '%s' in '%s' weapon slot %u!", kw, parse_actor_name, slot);

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
			engine_error("DECORATE", "Player start item list is not contiguous in '%s'!", parse_actor_name);
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
		engine_error("DECORATE", "Unable to find '%s' start item for '%s'!", kw, parse_actor_name);

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
	int32_t ox, oy;
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

	// simple checksum
	dec_mod_csum ^= parse_mobj_info->alias;
	dec_mod_csum += num_states;

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
			*parse_next_extra = 0;
			*parse_next_state = parse_last_anim;
			parse_next_state = NULL;
			continue;
		} else
		if(!strcmp(kw, "stop"))
		{
			if(parse_next_state)
			{
				*parse_next_extra = 0;
				*parse_next_state = 0;
				parse_next_state = NULL;
			} else
			{
				if(parse_anim_slot)
				{
					*parse_anim_slot = 0;
					unfinished = 0;
				} else
					goto error_no_states;
			}
			parse_anim_slot = NULL;
			continue;
		} else
		if(!strcmp(kw, "wait"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_extra = 0;
			*parse_next_state = num_states - 1;
			parse_next_state = NULL;
			continue;
		} else
		if(parse_mobj_info->extra_type == ETYPE_INVENTORY_CUSTOM && !strcmp(kw, "fail"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_extra = 0;
			*parse_next_state = 1;
			parse_next_state = NULL;
			continue;
		} else
		if(!strcmp(kw, "goto"))
		{
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
			anim = dec_find_animation(kw);
			if(!anim)
			{
				if(parse_mobj_info->extra_type == ETYPE_WEAPON && !strcmp(kw, "lightdone"))
				{
					// special state for weapons
					*parse_next_extra = 0;
					*parse_next_state = 1;
					// next keyword
					kw = tp_get_keyword_lc();
					if(!kw)
						return 1;
					goto skip_math;
				}
				// custom state
				*parse_next_extra = STATE_SET_CUSTOM;
				*parse_next_state = tp_hash32(kw);
			} else
			{
				// actual animation
				*parse_next_extra = STATE_SET_ANIMATION;
				*parse_next_state = anim->idx;
			}

			// check for '+'
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			if(kw[0] == '+')
			{
				// get offset
				kw = tp_get_keyword();
				if(!kw)
					return 1;

				if(doom_sscanf(kw, "%u", &tics) != 1 || tics < 0 || tics > STATE_EXTRA_MASK)
					engine_error("DECORATE", "Unable to parse number '%s' in '%s'!", kw, parse_actor_name);

				// next keyword
				kw = tp_get_keyword_lc();
				if(!kw)
					return 1;

				// add offset to 'extra'
				*parse_next_extra = *parse_next_extra + tics;
			}

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
			if(anim->name)
			{
				// actual animation
				parse_anim_slot = (uint16_t*)((void*)parse_mobj_info + anim->offset);
				*parse_anim_slot = num_states;
			} else
			{
				// custom state
				parse_anim_slot = dec_make_custom_state(tp_hash32(kw), num_states);
			}

			parse_last_anim = num_states;
			unfinished = 1;

			continue;
		}

		unfinished = 0;

		// it's a sprite
		sprite = check_add_sprite(kw);
		if(sprite < 0)
			engine_error("DECORATE", "Sprite name '%s' has wrong length in '%s'!", kw, parse_actor_name);

		// switch to line-based parsing
		tp_enable_newline = 1;
		// reset frame stuff
		flags = 0;
		parse_action_func = NULL;
		parse_action_arg = NULL;
		ox = 0;
		oy = 0;

		// get ticks
		kw = tp_get_keyword();
		if(!kw || kw[0] == '\n')
			return 1;

		if(doom_sscanf(kw, "%d", &tics) != 1 || tics > 65000)
			engine_error("DECORATE", "Unable to parse number '%s' in '%s'!", kw, parse_actor_name);

		if(tics < 0)
			tics = 0xFFFF;

		while(1)
		{
			// optional keywords or action
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;

			if(!strcmp(kw, "offset"))
			{
				//
				kw = tp_get_keyword();
				if(!kw)
					return 1;
				if(kw[0] != '(')
					engine_error("DECORATE", "Expected '%c' found '%s'!", '(', kw);
				// X
				kw = tp_get_keyword();
				if(!kw)
					return 1;
				if(doom_sscanf(kw, "%d", &ox) != 1 || ox < -32768 || ox > 32767)
					engine_error("DECORATE", "Unable to parse number '%s' in '%s'!", kw, parse_actor_name);
				// comma
				kw = tp_get_keyword();
				if(!kw)
					return 1;
				if(kw[0] != ',')
					engine_error("DECORATE", "Expected '%c' found '%s'!", ',', kw);
				// Y
				kw = tp_get_keyword();
				if(!kw)
					return 1;
				if(doom_sscanf(kw, "%d", &oy) != 1 || oy < -32768 || oy > 32767)
					engine_error("DECORATE", "Unable to parse number '%s' in '%s'!", kw, parse_actor_name);
				//
				kw = tp_get_keyword();
				if(!kw)
					return 1;
				if(kw[0] != ')')
					engine_error("DECORATE", "Expected '%c' found '%s'!", ')', kw);
			} else
			if(!strcmp(kw, "bright"))
				flags |= FF_FULLBRIGHT;
			else
			if(!strcmp(kw, "fast"))
				flags |= FF_FAST;
			else
			if(!strcmp(kw, "nodelay"))
				flags |= FF_NODELAY;
			else
			if(!strcmp(kw, "canraise"))
				flags |= FF_CANRAISE;
			else
				break;
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
		add_states(sprite, wk, tics, flags, ox, oy);

		if(kw[0] != '\n')
			engine_error("DECORATE", "Expected newline, found '%s' in '%s'!", kw, parse_actor_name);

		// next
		tp_enable_newline = 0;
	}

	// sanity check
	if(unfinished)
		engine_error("DECORATE", "Unfinised animation in '%s'!", parse_actor_name);

	// state limit check
	if(num_states >= 0x10000)
		engine_error("DECORATE", "So. Many. States.");

	// create a loopback
	// this is how ZDoom behaves
	if(parse_next_state)
		*parse_next_state = first_state;

	// simple checksum
	dec_mod_csum += num_states;

	return 0;

error_no_states:
	engine_error("DECORATE", "Missing states for '%s' in '%s'!", kw, parse_actor_name);
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
			engine_error("DECORATE", "Expected 'actor', got '%s'!", kw);

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
		if(mobj_check_type(alias) >= 0 || check_internal_type(parse_actor_name) >= 0)
			engine_error("DECORATE", "Multiple definitions of '%s'!", parse_actor_name);

		// add new type
		idx = num_mobj_types;
		num_mobj_types++;
		mobjinfo = ldr_realloc(mobjinfo, num_mobj_types * sizeof(mobjinfo_t));
		mobjinfo[idx].alias = alias;
	}

error_end:
	engine_error("DECORATE", "Incomplete definition!");
}

static void cb_parse_actors(lumpinfo_t *li)
{
	int32_t idx, etp;
	uint8_t *kw;
	mobjinfo_t *info;
	uint64_t alias;
	uint32_t first_state;

	tp_load_lump(li);

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return;

		// must get 'actor'
		if(strcmp(kw, "actor"))
			engine_error("DECORATE", "Expected 'actor', got '%s'!", kw);

		// actor name, case sensitive
		kw = tp_get_keyword();
		if(!kw)
			goto error_end;
		parse_actor_name = kw;

		// find mobj
		alias = tp_hash64(kw);
		idx = mobj_check_type(alias);
		if(idx < 0)
			engine_error("DECORATE", "Loading mismatch for '%s'!", kw);
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

			// check other actors
			etp = mobj_check_type(tp_hash64(kw));
			if(etp >= 0)
			{
				mobjinfo_t *other = mobjinfo + etp;

				// check
				if(!(other->flags & MF_MOBJ_IS_DEFINED))
					engine_error("DECORATE", "Invalid inheritance '%s' for '%s'!", kw, parse_actor_name);

				// copy info
				memcpy(info, other, sizeof(mobjinfo_t));
				info->replacement = 0;

				// ammo check
				if(other->extra_type == ETYPE_AMMO)
				{
					info->extra_type = ETYPE_AMMO_LINK;
					info->inventory.special = etp;
				}

				// copy custom states
				num_custom_states = 0;
				for(custom_state_t *cst = other->custom_states; cst < other->custom_st_end; cst++)
					dec_make_custom_state(cst->alias, cst->state);

				// mark
				etp = -1;
			} else
			{
				// find this class
				etp = check_internal_type(kw);
				if(etp < 0)
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
				if(etp < 0)
					engine_error("DECORATE", "Invalid inheritance '%s' for '%s'!", kw, parse_actor_name);
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
				engine_error("DECORATE", "Unable to replace '%s'!", kw);

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
			engine_error("DECORATE", "Expected '%c' found '%s'!", '{', kw);

		// initialize mobj
		if(etp >= 0)
		{
			// default info
			memcpy(info, inheritance[etp].def, sizeof(mobjinfo_t));
			info->extra_type = etp;
			// default species
			info->species = alias;
			// default damage factors
			memset(info->damage_factor, 0xFF, NUM_DAMAGE_TYPES * sizeof(uint16_t));
			// reset custom states
			num_custom_states = 0;
		} else
			etp = info->extra_type;
		info->doomednum = idx;
		info->alias = alias;
		info->spawnid = -1;
		info->flags |= MF_MOBJ_IS_DEFINED;

		// reset stuff
		extra_stuff_cur = NULL;
		extra_stuff_next = NULL;
		first_state = num_states;

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
					change_flag(kw, mobj_flags2, offsetof(mobjinfo_t, flags2)) &&
					(!inheritance[etp].flag[0] || change_flag(kw, inheritance[etp].flag[0], offsetof(mobjinfo_t, eflags)) ) &&
					(!inheritance[etp].flag[1] || change_flag(kw, inheritance[etp].flag[1], offsetof(mobjinfo_t, eflags)) )
				)
					engine_error("DECORATE", "Unknown flag '%s' in '%s'!", kw + 1, parse_actor_name);
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
					engine_error("DECORATE", "Unknown attribute '%s' in '%s'!", kw, parse_actor_name);

				// parse it
				if(parse_attr(attr->type, (void*)info + attr->offset))
					engine_error("DECORATE", "Unable to parse value of '%s' in '%s'!", kw, parse_actor_name);
			}
		}

		// save custom states
		if(num_custom_states)
		{
			uint32_t size = num_custom_states * sizeof(custom_state_t);
			info->custom_states = dec_es_alloc(size);
			info->custom_st_end = info->custom_states + num_custom_states;
			memcpy(info->custom_states, CUSTOM_STATE_STORAGE, size);
		}

		// save dropitem / weapon list
		if(extra_stuff_cur)
		{
			info->extra_stuff[0] = extra_stuff_cur;
			info->extra_stuff[1] = extra_stuff_next;
		}

		// resolve animation states
		for(uint32_t i = first_state; i < num_states; i++)
		{
			state_t *st = states + i;
			if((st->next_extra & STATE_CHECK_MASK) == STATE_SET_ANIMATION)
			{
				// resolve animation jumps now, this emulates ZDoom behavior of inherited actors
				uint32_t next;
				next = dec_resolve_animation(info, st->next_extra & STATE_EXTRA_MASK, st->nextstate, num_states);
				st->next_extra &= STATE_CHECK_MASK; // keep type, remove offset
				st->next_extra |= st->nextstate; // add animation ID
				st->nextstate = next; // set explicit state number
			}
		}
	}

error_end:
	engine_error("DECORATE", "Incomplete definition!");
error_numeric:
	engine_error("DECORATE", "Unable to parse number '%s' in '%s'!", kw, parse_actor_name);
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

	tp_load_lump(lumpinfo + lump);

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
				engine_error("KEYCONF", "Too many player classes!");

			kw = tp_get_keyword();
			if(!kw)
				engine_error("KEYCONF", "Incomplete definition!");

			lump = mobj_check_type(tp_hash64(kw));
			if(lump < 0 || mobjinfo[lump].extra_type != ETYPE_PLAYERPAWN)
				engine_error("KEYCONF", "Invalid player class '%s'!", kw);

			player_class[num_player_classes] = lump;

			num_player_classes++;
		} else
			engine_error("KEYCONF", "Unknown keyword '%s'.", kw);
	}
}

//
// API

int32_t mobj_by_spawnid(uint32_t id)
{
	for(uint32_t i = num_mobj_types-1; i; i--)
	{
		if(mobjinfo[i].spawnid == id)
			return i;
	}

	return -1;
}

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
	void *es_ptr;
	uint32_t es_size;

	dec_es_ptr = EXTRA_STORAGE_PTR;

	// To avoid unnecessary memory fragmentation, this function does multiple passes.
	doom_printf("[ACE] init DECORATE\n");
	ldr_alloc_message = "Decorate";

	// init damage types
	for(uint32_t i = 1; i < NUM_DAMAGE_TYPES; i++)
	{
		if(damage_type_config[i].name)
		{
			doom_sprintf(file_buffer, "pain.%s", damage_type_config[i].name);
			damage_type_config[i].pain = tp_hash32(file_buffer);
			doom_sprintf(file_buffer, "death.%s", damage_type_config[i].name);
			damage_type_config[i].death = tp_hash32(file_buffer);
			doom_sprintf(file_buffer, "xdeath.%s", damage_type_config[i].name);
			damage_type_config[i].xdeath = tp_hash32(file_buffer);
			doom_sprintf(file_buffer, "crash.%s", damage_type_config[i].name);
			damage_type_config[i].crash = tp_hash32(file_buffer);
			doom_sprintf(file_buffer, "crash.extreme.%s", damage_type_config[i].name);
			damage_type_config[i].xcrash = tp_hash32(file_buffer);
		}
	}

	// init sprite names
	numsprites = NUMSPRITES;
	for(uint32_t i = 0; i < NUMSPRITES; i++)
		sprite_table[i] = *spr_names[i];
	sprite_tnt1 = spr_add_name(0x31544E54); // 'TNT1'
	spr_add_name(0x54534950); // 'PIST'
	spr_add_name(0x43454349); // 'ICEC'

	// mobjinfo
	mobjinfo = ldr_malloc((NUMMOBJTYPES + NUM_NEW_TYPES) * sizeof(mobjinfo_t));

	// copy original stuff
	for(uint32_t i = 0; i < NUMMOBJTYPES; i++)
	{
		mobjinfo[i] = default_mobj;

		mobjinfo[i].doomednum = deh_mobjinfo[i].doomednum;
		mobjinfo[i].state_spawn = deh_mobjinfo[i].spawnstate;
		mobjinfo[i].spawnhealth = deh_mobjinfo[i].spawnhealth;
		mobjinfo[i].state_see = deh_mobjinfo[i].seestate;
		mobjinfo[i].seesound = deh_mobjinfo[i].seesound;
		mobjinfo[i].reactiontime = deh_mobjinfo[i].reactiontime;
		mobjinfo[i].attacksound = deh_mobjinfo[i].attacksound;
		mobjinfo[i].state_pain = deh_mobjinfo[i].painstate;
		mobjinfo[i].painsound = deh_mobjinfo[i].painsound;
		mobjinfo[i].state_melee = deh_mobjinfo[i].meleestate;
		mobjinfo[i].state_missile = deh_mobjinfo[i].missilestate;
		mobjinfo[i].state_death = deh_mobjinfo[i].deathstate;
		mobjinfo[i].state_xdeath = deh_mobjinfo[i].xdeathstate;
		mobjinfo[i].deathsound = deh_mobjinfo[i].deathsound;
		mobjinfo[i].speed = deh_mobjinfo[i].speed;
		mobjinfo[i].radius = deh_mobjinfo[i].radius;
		mobjinfo[i].height = deh_mobjinfo[i].height;
		mobjinfo[i].mass = deh_mobjinfo[i].mass;
		mobjinfo[i].damage = deh_mobjinfo[i].damage;
		mobjinfo[i].activesound = deh_mobjinfo[i].activesound;
		mobjinfo[i].flags = (deh_mobjinfo[i].flags & 0x7FFFFFFF) | MF_MOBJ_IS_DEFINED;
		mobjinfo[i].state_raise = deh_mobjinfo[i].raisestate;
		mobjinfo[i].painchance[DAMAGE_NORMAL] = deh_mobjinfo[i].painchance;

		mobjinfo[i].alias = doom_actor_name[i];
		mobjinfo[i].spawnid = doom_spawn_id[i];

		mobjinfo[i].species = mobjinfo[i].alias;

		// check for original random sounds
		sfx_rng_fix(&mobjinfo[i].seesound, 98);
		sfx_rng_fix(&mobjinfo[i].deathsound, 70);

		// basically everything has vile heal state
		mobjinfo[i].state_heal = 266;

		// basically everything is randomized
		// basically everything can be seeker missile
		mobjinfo[i].flags1 = MF1_RANDOMIZE | MF1_SEEKERMISSILE;

		// mark enemies
		if(mobjinfo[i].flags & MF_COUNTKILL)
			mobjinfo[i].flags1 |= MF1_ISMONSTER;

		// modify projectiles
		if(mobjinfo[i].flags & MF_MISSILE)
			mobjinfo[i].flags1 |= MF1_NOTELEPORT;

		// modify render style
		if(mobjinfo[i].flags & MF_SHADOW)
			mobjinfo[i].render_style = RS_FUZZ;

		// default damage factors
		memset(mobjinfo[i].damage_factor, 0xFF, NUM_DAMAGE_TYPES * sizeof(uint16_t));

		// set translation
		if(mobjinfo[i].flags & MF_TRANSLATION)
			mobjinfo[i].translation = render_translation + ((mobjinfo[i].flags & MF_TRANSLATION) >> (MF_TRANSSHIFT - 8)) - 256;
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
	if(!mobjinfo[0].speed)
		mobjinfo[0].speed = FRACUNIT;

	// lost soul stuff
	mobjinfo[18].flags1 |= MF1_ISMONSTER | MF1_DONTFALL;
	mobjinfo[18].flags2 |= MF2_NOICEDEATH;

	// archvile stuff
	mobjinfo[3].flags1 |= MF1_NOTARGET | MF1_QUICKTORETALIATE;

	// bullet puff
	mobjinfo[37].vspeed = FRACUNIT;
	mobjinfo[37].state_melee = 95;

	// species
	mobjinfo[17].species = mobjinfo[15].species;

	// boss stuff
	mobjinfo[19].flags1 |= MF1_BOSS | MF1_NORADIUSDMG;
	mobjinfo[21].flags1 |= MF1_BOSS | MF1_NORADIUSDMG;

	// fast stuff
	mobjinfo[16].fast_speed = 20 * FRACUNIT;
	mobjinfo[31].fast_speed = 20 * FRACUNIT;
	mobjinfo[32].fast_speed = 20 * FRACUNIT;

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
	states = ldr_malloc(NEW_NUMSTATES * sizeof(state_t));

	// copy internal stuff
	memcpy(states + NUMSTATES, internal_states, sizeof(internal_states));

	// copy original stuff
	for(uint32_t i = 0; i < NUMSTATES; i++)
	{
		states[i].sprite = deh_states[i].sprite;
		states[i].frame = deh_states[i].frame & 0x80FF;
		states[i].arg = NULL;
		if(deh_states[i].tics > 65000)
			states[i].tics = 65000;
		else
		if(deh_states[i].tics < 0)
			states[i].tics = 0xFFFF;
		else
			states[i].tics = deh_states[i].tics;
		states[i].action = deh_states[i].action;
		states[i].nextstate = deh_states[i].nextstate;
		states[i].next_extra = 0;
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

	// fast states
	for(uint32_t i = 477; i <= 489; i++)
		states[i].frame |= FF_FAST;

	// process actors
	if(mod_config.enable_decorate)
		wad_handle_lump("DECORATE", cb_parse_actors);

	//
	// PASS 3

	// copy extra storage
	es_size = dec_es_ptr - EXTRA_STORAGE_PTR;
	es_ptr = ldr_malloc(es_size);
	memcpy(es_ptr, EXTRA_STORAGE_PTR, es_size);
	doom_printf("[DECORATE] %uB / %uB extra storage\n", es_size, EXTRA_STORAGE_SIZE);

	// post-init render
	render_generate_blood();

	// relocate extra storage
	for(uint32_t i = 0; i < num_states; i++)
		states[i].arg = dec_reloc_es(es_ptr, (void*)states[i].arg);

	for(uint32_t idx = NUMMOBJTYPES + NUM_NEW_TYPES; idx < num_mobj_types; idx++)
	{
		mobjinfo_t *info = mobjinfo + idx;

		// custom states
		info->custom_states = dec_reloc_es(es_ptr, info->custom_states);
		info->custom_st_end = dec_reloc_es(es_ptr, info->custom_st_end);

		// drop item list / start inventory list
		info->extra_stuff[0] = dec_reloc_es(es_ptr, info->extra_stuff[0]);
		info->extra_stuff[1] = dec_reloc_es(es_ptr, info->extra_stuff[1]);

		// PlayerPawn
		if(info->extra_type == ETYPE_PLAYERPAWN)
		{
			// custom damage animations
			info->player.name = dec_reloc_es(es_ptr, info->player.name);

			// weapon slots
			for(uint32_t i = 0; i < NUM_WPN_SLOTS; i++)
				info->player.wpn_slot[i] = dec_reloc_es(es_ptr, info->player.wpn_slot[i]);
		}

		// RandomSpawner
		if(info->extra_type == ETYPE_RANDOMSPAWN)
		{
			// count randomization
			uint32_t weight = 0;
			for(mobj_dropitem_t *drop = info->dropitem.start; drop < (mobj_dropitem_t*)info->dropitem.end; drop++)
			{
				if(drop->amount)
					weight += drop->amount;
				else
					weight += 1;
			}
			info->random_weight = weight;
			if(!weight)
				engine_error("DECORATE", "Empty RandomSpawner!");
		}

		// Inventory stuff
		if(inventory_is_valid(info) || info->extra_type == ETYPE_HEALTH || info->extra_type == ETYPE_INV_SPECIAL)
			info->inventory.message = dec_reloc_es(es_ptr, info->inventory.message);
	}

	// extra stuff
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
							engine_error("DECORATE", "Non-weapon in weapon slot!");
					}
				}
			break;
			case ETYPE_WEAPON:
				if(	(info->weapon.ammo_type[0] && mobjinfo[info->weapon.ammo_type[0]].extra_type != ETYPE_AMMO && mobjinfo[info->weapon.ammo_type[0]].extra_type != ETYPE_AMMO_LINK) ||
					(info->weapon.ammo_type[1] && mobjinfo[info->weapon.ammo_type[1]].extra_type != ETYPE_AMMO && mobjinfo[info->weapon.ammo_type[1]].extra_type != ETYPE_AMMO_LINK)
				)
					engine_error("DECORATE", "Invalid weapon ammo!");
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
					engine_error("DECORATE", "Invalid inheritted ammo!");

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

		// update damage type stuff
		if(info->damage_factor[DAMAGE_NORMAL] == 0xFFFF)
			info->damage_factor[DAMAGE_NORMAL] = 8;
		info->painchance[DAMAGE_NORMAL] &= 0x01FF;
		for(uint32_t i = 1; i < NUM_DAMAGE_TYPES; i++)
		{
			if(info->painchance[i] & 0x8000)
				info->painchance[i] &= 0x01FF;
			else
				info->painchance[i] = info->painchance[DAMAGE_NORMAL];

			if(info->damage_factor[i] == 0xFFFF)
				info->damage_factor[i] = info->damage_factor[DAMAGE_NORMAL];
		}

		// resolve replacements
		{
			uint32_t tmp = 64;
			uint32_t type = idx;

			while(mobjinfo[type].replacement)
			{
				if(!tmp)
					engine_error("DECORATE", "Too many replacements!");
				type = mobjinfo[type].replacement;
				tmp--;
			}

			if(idx != type)
				mobjinfo[idx].replacement = type;
		}

		// blood translations
		if(info->blood_trns)
			info->blood_trns = r_get_blood_color((uintptr_t)info->blood_trns);
	}

	//
	// DONE

	// patch CODE - 'states'
	for(uint32_t i = 0; i < NUM_STATE_HOOKS; i++)
		hook_states[i].value += (uint32_t)states;
	utils_install_hooks(hook_states, NUM_STATE_HOOKS);

	//
	// KEYCONF

	if(mod_config.enable_decorate)
		parse_keyconf();

	if(!num_player_classes)
		engine_error("KEYCONF", "No player classes defined!");
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
static uint32_t check_step_height(fixed_t floorz, mobj_t *mo)
{
	if(mo->flags & MF_MISSILE)
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
		floorz - tmdropoffz > mo->info->dropoff
	)
		return 1;

	return 0;
}

__attribute((regparm(2),no_caller_saved_registers))
static void sound_noway(mobj_t *mo)
{
	S_StartSound(mo, mo->info->player.sound.usefail);
}

__attribute((regparm(2),no_caller_saved_registers))
static void sound_oof(mobj_t *mo)
{
	if(mo->health <= 0)
		return;
	S_StartSound(mo, mo->info->player.sound.land);
}

//
// hooks

static hook_t hook_states[NUM_STATE_HOOKS] =
{
	{0x000315D9, CODE_HOOK | HOOK_UINT32, 0}, // P_SpawnMobj
};

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// hook step height check in 'P_TryMove'
	{0x0002B27C, CODE_HOOK | HOOK_UINT16, 0xF289},
	{0x0002B27E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)check_step_height},
	{0x0002B283, CODE_HOOK | HOOK_UINT16, 0xC085},
	{0x0002B285, CODE_HOOK | HOOK_UINT32, 0x1FEB2B74},
	// disable teleport sounds
	{0x00020BA2, CODE_HOOK | HOOK_UINT16, 0x0AEB}, // G_CheckSpot
	{0x000313D7, CODE_HOOK | HOOK_UINT16, 0x08EB}, // P_NightmareRespawn
	{0x000313FC, CODE_HOOK | HOOK_UINT16, 0x08EB}, // P_NightmareRespawn
	// use 'MF1_ISMONSTER' in 'P_MobjThinker'
	{0x000314FC, CODE_HOOK | HOOK_UINT16, offsetof(mobj_t, flags1) | (MF1_ISMONSTER << 8)},
	// use 'MF1_TELESTOMP' in 'PIT_StompThing'
	{0x0002ABC7, CODE_HOOK | HOOK_UINT16, 0x43F6},
	{0x0002ABC9, CODE_HOOK | HOOK_UINT16, offsetof(mobj_t, flags1) | (MF1_TELESTOMP << 8)},
	{0x0002ABCB, CODE_HOOK | HOOK_SET_NOPS, 3},
	// change damage in 'PIT_StompThing'
	{0x0002ABDE, CODE_HOOK | HOOK_UINT32, 1000000},
	// disable 'th->momz' in 'P_SpawnPuff'
	{0x00031B27, CODE_HOOK | HOOK_UINT16, 0x05EB},
	// change 'sfx_noway' in 'PTR_UseTraverse'
	{0x0002BCCA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)sound_noway},
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

