// kgsws' Doom ACE
//

typedef struct
{
	uint8_t count;
	uint32_t typecombo[];
} __attribute__((packed)) arg_droplist_t;

//
//

void A_NoBlocking(mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));

