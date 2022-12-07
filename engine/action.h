// kgsws' ACE Engine
////

extern void *parse_action_func;
extern void *parse_action_arg;

//

typedef union
{
	struct
	{
		int16_t value;
		uint8_t reserved;
		uint8_t info;
	};
	uint32_t w;
} arg_special_t;

//

typedef struct
{
	fixed_t value;
} args_singleFixed_t;

typedef struct
{
	void *value;
} args_singlePointer_t;

typedef struct
{
	uint16_t type;
} args_singleType_t;

typedef struct
{
	uint16_t special;
	arg_special_t arg[5];
} args_lineSpecial_t;

//

#define CHAN_BODY	0
#define CHAN_WEAPON	1
#define CHAN_VOICE	2

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

#define FBF_USEAMMO	1
#define FBF_NOFLASH	2
#define FBF_NORANDOM	4
#define FBF_NORANDOMPUFFZ	8

#define CBAF_AIMFACING	1
#define CBAF_NORANDOM	2
#define CBAF_NORANDOMPUFFZ	4

typedef struct
{
	uint16_t pufftype;
	uint8_t ptr; // for monsters
	int8_t blt_count;
	angle_t spread_hor;
	angle_t spread_ver;
	uint32_t damage;
	uint32_t flags;
	fixed_t range;
} args_BulletAttack_t;

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

angle_t slope_to_angle(fixed_t slope);
void missile_stuff(mobj_t *mo, mobj_t *source, mobj_t *target, angle_t angle, angle_t pitch, fixed_t slope);

//

void A_Look(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
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

void A_CheckPlayerDone(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));

void A_GenericFreezeDeath(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_FreezeDeathChunks(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_IceSetTics(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));

//

uint8_t *action_parser(uint8_t *name);

