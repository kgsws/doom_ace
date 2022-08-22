// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "map.h"
#include "decorate.h"

uint32_t *leveltime;

uint32_t *numlines;
uint32_t *numsectors;
line_t **lines;
vertex_t **vertexes;
side_t **sides;
sector_t **sectors;

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
uint32_t get_spawn_type(uint32_t ednum)
{
	// do a backward search
	uint32_t idx = num_mobj_types;

	do
	{
		idx--;
		if(mobjinfo[idx].doomednum == ednum)
			return idx;
	} while(idx);

	// TODO: custom 'unknown item'
	return 0;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace mobjinfo search in 'P_SpawnMapThing'
	{0x000319E7, CODE_HOOK | HOOK_UINT32, 0x0645B70F},
	{0x000319EB, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)get_spawn_type},
	{0x000319F0, CODE_HOOK | HOOK_UINT32, 0x32EBC189},
	// replace mobjinfo search in 'P_RespawnSpecials'
	{0x00031772, CODE_HOOK | HOOK_UINT32, 0x0645B70F},
	{0x00031776, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)get_spawn_type},
	{0x0003177B, CODE_HOOK | HOOK_UINT32, 0x10EBC189},
	// import variables
	{0x0002CF80, DATA_HOOK | HOOK_IMPORT, (uint32_t)&leveltime},
	{0x0002C134, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numlines},
	{0x0002C14C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsectors},
	{0x0002C120, DATA_HOOK | HOOK_IMPORT, (uint32_t)&lines},
	{0x0002C138, DATA_HOOK | HOOK_IMPORT, (uint32_t)&vertexes},
	{0x0002C118, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sides},
	{0x0002C148, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sectors},
};

