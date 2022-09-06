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

#define CMF_AIMOFFSET	1
#define CMF_AIMDIRECTION	2
#define CMF_TRACKOWNER	4
#define CMF_CHECKTARGETDEAD	8
#define CMF_ABSOLUTEPITCH	16
#define CMF_OFFSETPITCH	32
#define CMF_SAVEPITCH	64
#define CMF_ABSOLUTEANGLE	128

typedef struct
{
	uint16_t missiletype;
	uint8_t ptr;
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

void A_Lower(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Raise(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_WeaponReady(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_ReFire(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Light0(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Light1(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));
void A_Light2(mobj_t *mo, state_t *st, stfunc_t stfunc) __attribute((regparm(2),no_caller_saved_registers));

//

uint8_t *action_parser(uint8_t *name);

