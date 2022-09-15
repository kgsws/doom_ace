// kgsws' ACE Engine
////

extern void *parse_action_func;
extern void *parse_action_arg;

//

typedef struct
{
	fixed_t value;
} args_singleFixed_t;

//

#define CHAN_BODY	0
#define CHAN_WEAPON	1
#define CHAN_VOICE	2
#define CHAN_ITEM	3

typedef struct
{
	uint16_t sound;
	uint32_t slot;
} args_StartSound_t;

//

#define CMF_AIMOFFSET	1
#define CMF_AIMDIRECTION	2
#define CMF_TRACKOWNER	4
#define CMF_CHECKTARGETDEAD	8
#define CMF_ABSOLUTEPITCH	16
#define CMF_OFFSETPITCH	32
#define CMF_SAVEPITCH	64
#define CMF_ABSOLUTEANGLE	128

#define FPF_AIMATANGLE	1
#define FPF_TRANSFERTRANSLATION	2
#define FPF_NOAUTOAIM	4

typedef struct
{
	uint16_t missiletype;
	uint8_t ptr; // for monsters
	uint8_t noammo; // for players
	fixed_t spawnheight;
	fixed_t spawnofs_xy;
	angle_t angle;
	uint32_t flags;
	fixed_t pitch;
} args_SpawnProjectile_t;

//

typedef struct
{
	uint16_t type;
	uint16_t amount;
	uint8_t ptr;
} args_GiveInventory_t;

//

typedef struct
{
	angle_t angle;
	uint8_t ptr;
	uint32_t flags;
} args_SetAngle_t;

//

extern const args_singleFixed_t def_LowerRaise;

//

void missile_stuff(mobj_t *mo, mobj_t *source, mobj_t *target, angle_t angle);

//

void A_Lower(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Raise(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_GunFlash(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_CheckReload(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_WeaponReady(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_ReFire(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Light0(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Light1(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Light2(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));

void A_OldProjectile(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_OldBullets(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));

void debug_codeptr(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));

//

uint8_t *action_parser(uint8_t *name);

