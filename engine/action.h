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
	uint32_t bits;
	uint16_t offset;
} act_moflag_t;

//

typedef struct
{
	fixed_t value;
} args_singleFixed_t;

typedef struct
{
	uint16_t value;
} args_singleU16_t;

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
	uint16_t flags;
} args_singleFlags_t;

typedef struct
{
	uint32_t damage;
} args_singleDamage_t;

typedef struct
{
	state_jump_t state;
} args_singleState_t;

typedef struct
{
	uint16_t special;
	arg_special_t arg[5];
} args_lineSpecial_t;

//

#define GFF_NOEXTCHANGE	1

typedef struct
{
	state_jump_t state;
	uint16_t flags;
} args_GunFlash_t;

//

#define CHAN_BODY	0
#define CHAN_WEAPON	1
#define CHAN_VOICE	2

#define ATTN_NORM	0
#define ATTN_NONE	1

typedef struct
{
	uint16_t sound;
	uint16_t slot;
	uint16_t attn;
} args_StartSound_t;

//

#define VAF_DMGTYPEAPPLYTODIRECT	1

typedef struct
{
	uint16_t sound;
	uint16_t flags;
	uint32_t damage_direct;
	uint32_t damage_blast;
	fixed_t blast_radius;
	fixed_t thrust_factor;
	uint8_t damage_type;
} args_VileAttack_t;

//

#define WRF_NOBOB	1
#define WRF_NOFIRE	(WRF_NOPRIMARY | WRF_NOSECONDARY)
#define WRF_NOSWITCH	2
#define WRF_DISABLESWITCH	4
#define WRF_NOPRIMARY	8
#define WRF_NOSECONDARY	16

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
	fixed_t pitch;
	uint16_t flags;
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
	fixed_t range;
	uint16_t flags;
} args_BulletAttack_t;

//

typedef struct
{
	uint32_t damage;
	uint16_t sound_hit;
	uint16_t sound_miss;
	uint8_t damage_type;
	uint8_t sacrifice;
} args_MeleeAttack_t;

//

#define CPF_USEAMMO	1
#define CPF_PULLIN	2
#define CPF_NORANDOMPUFFZ	4
#define CPF_NOTURN	8

typedef struct
{
	uint16_t pufftype;
	uint16_t flags;
	uint32_t damage;
	fixed_t range;
	uint8_t norandom;
} args_PunchAttack_t;

//

typedef struct
{
	uint16_t spraytype;
	uint16_t numrays;
	uint16_t damagecnt;
	uint32_t damage;
	angle_t angle;
	fixed_t range;
} args_BFGSpray_t;

//

#define SMF_LOOK	1
#define SMF_PRECISE	2

typedef struct
{
	angle_t threshold;
	angle_t maxturn;
	uint16_t flags;
	uint16_t chance;
	uint16_t range;
} args_SeekerMissile_t;

//

#define SXF_TRANSFERTRANSLATION	0x0001
#define SXF_ABSOLUTEPOSITION	0x0002
#define SXF_ABSOLUTEANGLE	0x0004
#define SXF_ABSOLUTEVELOCITY	0x0008
#define SXF_SETMASTER	0x0010
#define SXF_NOCHECKPOSITION	0x0020
#define SXF_TELEFRAG	0x0040
#define SXF_TRANSFERAMBUSHFLAG	0x0080
#define SXF_TRANSFERPITCH	0x0100
#define SXF_TRANSFERPOINTERS	0x0200
#define SXF_USEBLOODCOLOR	0x0400
#define SXF_CLEARCALLERTID	0x0800
#define SXF_SETTARGET	0x1000
#define SXF_SETTRACER	0x2000
#define SXF_NOPOINTERS	0x4000
#define SXF_ORIGINATOR	0x8000
#define SXF_ISTARGET	0x10000
#define SXF_ISMASTER	0x20000
#define SXF_ISTRACER	0x40000

typedef struct
{
	uint16_t type;
	uint16_t tid;
	fixed_t ox, oy, oz;
	fixed_t vx, vy, vz;
	angle_t angle;
	uint32_t flags;
	uint16_t fail;
} args_SpawnItemEx_t;

//

typedef struct
{
	uint16_t type;
	uint16_t chance;
} args_DropItem_t;

//

typedef struct
{
	uint16_t type;
	uint16_t amount;
	uint8_t ptr;
	uint8_t sacrifice;
} args_GiveInventory_t;

//

typedef struct
{
	fixed_t alpha;
	uint16_t flags;
} args_SetRenderStyle_t;

//

typedef struct
{
	angle_t angle;
	uint8_t ptr;
	uint8_t sacrifice;
} args_SetAngle_t;

//

#define CVF_RELATIVE	1
#define CVF_REPLACE	2

typedef struct
{
	fixed_t x, y, z;
	uint16_t flags;
	uint8_t ptr;
} args_ChangeVelocity_t;

//

typedef struct
{
	fixed_t scale;
	uint8_t ptr;
} args_ScaleVelocity_t;

//

typedef struct
{
	uint8_t target;
	uint8_t master;
	uint8_t tracer;
} args_RearrangePointers_t;

//

#define WARPF_ABSOLUTEOFFSET	0x0001
#define WARPF_ABSOLUTEANGLE	0x0002
#define WARPF_ABSOLUTEPOSITION	0x0004
#define WARPF_USECALLERANGLE	0x0008
#define WARPF_NOCHECKPOSITION	0x0010
#define WARPF_STOP	0x0020
#define WARPF_MOVEPTR	0x0040
#define WARPF_COPYVELOCITY	0x0080
#define WARPF_COPYPITCH	0x0100

typedef struct
{
	fixed_t x, y, z;
	angle_t angle;
	uint16_t flags;
	uint8_t ptr;
} args_Warp_t;

//

#define XF_HURTSOURCE	1
#define XF_NOTMISSILE	2
#define XF_THRUSTZ	4
#define XF_NOSPLASH	8

typedef struct
{
	uint32_t damage;
	fixed_t distance;
	fixed_t fistance;
	uint16_t flags;
	uint8_t alert;
} args_Explode_t;

//

typedef struct
{
	act_moflag_t moflag;
	uint8_t set;
} args_ChangeFlag_t;

//

typedef struct
{
	uint16_t chance;
	uint16_t count;
	union
	{
		struct
		{
			state_jump_t state0;
			state_jump_t state1;
			state_jump_t state2;
			state_jump_t state3;
			state_jump_t state4;
			state_jump_t state5;
			state_jump_t state6;
			state_jump_t state7;
			state_jump_t state8;
			state_jump_t state9;
		};
		state_jump_t states[10];
	};
} args_Jump_t;

typedef struct
{
	uint16_t type;
	uint16_t amount;
	state_jump_t state;
	uint8_t ptr;
} args_JumpIfInventory_t;

typedef struct
{
	state_jump_t state;
	uint16_t amount;
	uint8_t ptr;
} args_JumpIfHealthLower_t;

typedef struct
{
	fixed_t range;
	state_jump_t state;
	uint8_t no_z;
} args_JumpIfCloser_t;

typedef struct
{
	act_moflag_t moflag;
	state_jump_t state;
	uint8_t ptr;
} args_CheckFlag_t;

typedef struct
{
	state_jump_t state;
	uint16_t chance;
} args_MonsterRefire_t;

//

extern const args_singleU16_t def_LowerRaise;
extern const args_Explode_t def_Explode;

extern uint32_t act_cc_tick;

//

angle_t slope_to_angle(fixed_t slope);
fixed_t projectile_speed(mobjinfo_t *info);
void missile_stuff(mobj_t *mo, mobj_t *source, mobj_t *target, fixed_t speed, angle_t angle, angle_t pitch, fixed_t slope);

//

void A_Look(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Lower(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Raise(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_DoomGunFlash(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_CheckReload(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_WeaponReady(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_ReFire(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Light0(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Light1(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Light2(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Explode(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));

void A_OldProjectile(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_OldBullets(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));

void A_CheckPlayerDone(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));

void A_GenericFreezeDeath(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_FreezeDeathChunks(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_IceSetTics(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));

//

uint8_t *action_parser(uint8_t *name);

