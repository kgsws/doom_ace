// kgsws' Doom ACE
// Main code example.
#include "engine.h"
#include "utils.h"

// this is the exploit entry function
// patch everything you need and leave
void ace_init()
{
	doom_printf("Text from ACE_CODE ...\n");
	dos_exit();
}

