// kgsws' Doom ACE
//

typedef struct
{
	uint8_t count;
	uint32_t typecombo[];
} __attribute__((packed)) arg_droplist_t;

//
//

void *arg_NoBlocking(void*, uint8_t*, uint8_t*);
void A_NoBlocking(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

void *arg_SpawnProjectile(void*, uint8_t*, uint8_t*);
void A_SpawnProjectile(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

typedef struct
{
	fixed_t distance;
	uint32_t state;
	uint32_t noz;
} __attribute__((packed)) arg_jump_distance_t;
void *arg_JumpIfCloser(void*, uint8_t*, uint8_t*);
void A_JumpIfCloser(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));


