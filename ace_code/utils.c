// kgsws' Doom ACE
// Various utility functions.
//
#include "engine.h"
#include "utils.h"

void utils_install_hooks(const hook_t *table)
{
	while(table->addr)
	{
		uint32_t addr = table->addr;

		if(table->type & 0x80000000)
		{
			if(table->type & 0x40000000)
				addr += ace_segment;
			else
				addr += doom_data_segment;
		} else
			addr += doom_code_segment;

		switch(table->type & 0xFFFF)
		{
			// these modify Doom memory
			case HOOK_CALL_ACE:
				*((uint8_t*)addr) = 0xE8;
				addr++;
			goto reladdr_ace;
			case HOOK_JMP_ACE:
				*((uint8_t*)addr) = 0xE9;
				addr++; // fall trough
			case HOOK_RELADDR_ACE:
reladdr_ace:
				*((uint32_t*)(addr)) = (table->value + ace_segment) - (addr + 4);
			break;
			case HOOK_CALL_DOOM:
				*((uint8_t*)addr) = 0xE8;
				addr++;
			goto reladdr_doom;
			case HOOK_JMP_DOOM:
				*((uint8_t*)addr) = 0xE9;
				addr++; // fall trough
			case HOOK_RELADDR_DOOM:
reladdr_doom:
				*((uint32_t*)(addr)) = (table->value + doom_code_segment) - (addr + 4);
			break;
			case HOOK_UINT8:
				*((uint8_t*)addr) = table->value;
			break;
			case HOOK_UINT16:
				*((uint16_t*)addr) = table->value;
			break;
			case HOOK_UINT32:
				*((uint32_t*)addr) = table->value;
			break;
			case HOOK_CSTR_ACE:
				strcpy((char*)addr, (char*)(table->value + ace_segment));
			break;
			case HOOK_CSTR_DOOM:
				strcpy((char*)addr, (char*)(table->value + doom_data_segment));
			break;
			case HOOK_ABSADDR_ACE:
				*((uint32_t*)addr) = table->value + ace_segment;
			break;
			case HOOK_ABSADDR_CODE:
				*((uint32_t*)addr) = table->value + doom_code_segment;
			break;
			case HOOK_ABSADDR_DATA:
				*((uint32_t*)addr) = table->value + doom_data_segment;
			break;
			// these modify ACE memory
			case HOOK_IMPORT:
				*((uint32_t*)(table->value + ace_segment)) = addr;
			break;
			case HOOK_READ8:
				*((uint8_t*)(table->value + ace_segment)) = *((uint8_t*)addr);
			break;
			case HOOK_READ16:
				*((uint16_t*)(table->value + ace_segment)) = *((uint16_t*)addr);
			break;
			case HOOK_READ32:
				*((uint32_t*)(table->value + ace_segment)) = *((uint32_t*)addr);
			break;
		}

		table++;
	}
}

void utils_fix_parray(uint32_t *table, uint32_t count)
{
	do
	{
		*table += ace_segment;
		table++;
	} while(--count);
}

char *strcpy(char *dst, const char *src)
{
	char *ret = dst;
	do
	{
		*dst++ = *src++;
	} while(*src);
}

void *memset(void *dst, int value, size_t len)
{
	void *ret = dst;
	while(len--)
	{
		*(uint8_t*)dst = value;
		dst++;
	}
	return ret;
}

