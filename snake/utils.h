// kgsws' Doom ACE
// Various utility stuff.
//

enum
{
	// hooks that modify Doom memory
	HOOK_RELADDR_ACE,	// replace 32bit 'call' or 'jmp' destination to ACE function
	HOOK_RELADDR_DOOM,	// replace 32bit 'call' or 'jmp' destination to DOOM function
	HOOK_UINT32,	// replace 32bit value
	HOOK_UINT16,	// replace 16bit value
	HOOK_UINT8,	// replace 8bit value
	// 'hooks' that modify ACE memory
	HOOK_IMPORT,	// get address of any variable
	HOOK_READ8,	// get value of 8bit variable
	HOOK_READ16,	// get value of 16bit variable
	HOOK_READ32,	// get value of 32bit variable
	// address is referenced to
	CODE_HOOK = 0x00000000,
	DATA_HOOK = 0x80000000,
	ACE_HOOK = 0xC0000000,
};

typedef struct
{
	uint32_t addr;
	int type;
	uint32_t value;
} hook_t;

#define relocate_addr_code(x)	((uint32_t)(x)+doom_code_segment)
#define relocate_addr_data(x)	((uint32_t)(x)+doom_data_segment)
#define relocate_addr_ace(x)	((uint32_t)(x)+ace_segment)

extern uint32_t doom_code_segment;
extern uint32_t doom_data_segment;
extern uint32_t ace_segment;

void utils_install_hooks(const hook_t *table);
void utils_fix_parray(uint32_t *table, uint32_t count);
