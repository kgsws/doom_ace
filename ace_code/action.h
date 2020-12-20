// kgsws' Doom ACE
//

typedef struct
{
	uint8_t count;
	uint32_t typecombo[];
} __attribute__((packed)) arg_droplist_t;

//
// A few functions need special patch after states are defined.
// Such functions are returning special "marker" and it's code pointer is set later.

//
// A_NoBlocking
#define ACTFIX_NoBlocking	((void*)1)
void *arg_NoBlocking(void*, uint8_t*, uint8_t*);
void A_NoBlocking(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

//
// A_SpawnProjectile
void *arg_SpawnProjectile(void*, uint8_t*, uint8_t*);
void A_SpawnProjectile(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

//
// A_JumpIfCloser
#define ACTFIX_JumpIfCloser	((void*)2)
typedef struct
{
	fixed_t distance;
	uint32_t state;
	uint32_t noz;
} __attribute__((packed)) arg_jump_distance_t;
void *arg_JumpIfCloser(void*, uint8_t*, uint8_t*);
void A_JumpIfCloser(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

//
// A_Jump
#define ACTFIX_Jump	((void*)3)
typedef struct
{
	uint16_t count;
	uint16_t chance;
	uint32_t state[5];
} __attribute__((packed)) arg_jump_random_t;
void *arg_Jump(void*, uint8_t*, uint8_t*);
void A_Jump(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

//
// A_Raise (weapon)
void A_Raise(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

//
// A_Lower (weapon)
void A_Lower(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

//
// A_WeaponReady (weapon)
void A_WeaponReady(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

//
// A_ReFire (weapon)
void A_ReFire(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

//
// A_GunFlash (weapon)
void A_GunFlash(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

//
// A_LightX (weapon)
void A_Light0(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void A_Light1(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void A_Light2(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));


