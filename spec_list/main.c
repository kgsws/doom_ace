#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

typedef struct
{
	uint32_t special;
	const uint8_t *name;
} linespec_t;

//

static const linespec_t special_action[] =
{
	{2, "polyobj_rotateleft"},
	{3, "polyobj_rotateright"},
	{4, "polyobj_move"},
	{7, "polyobj_doorswing"},
	{8, "polyobj_doorslide"},
	{10, "door_close"},
	{12, "door_raise"},
	{13, "door_lockedraise"},
	{19, "thing_stop"},
	{20, "floor_lowerbyvalue"},
	{23, "floor_raisebyvalue"},
	{26, "stairs_builddown"},
	{27, "stairs_buildup"},
	{28, "floor_raiseandcrush"},
	{31, "stairs_builddownsync"},
	{32, "stairs_buildupsync"},
	{35, "floor_raisebyvaluetimes8"},
	{36, "floor_lowerbyvaluetimes8"},
	{40, "ceiling_lowerbyvalue"},
	{41, "ceiling_raisebyvalue"},
	{44, "ceiling_crushstop"},
	{62, "plat_downwaitupstay"},
	{64, "plat_upwaitdownstay"},
	{70, "teleport"},
	{71, "teleport_nofog"},
	{72, "thrustthing"},
	{73, "damagething"},
	{74, "teleport_newmap"},
	{76, "teleportother"},
	{97, "ceiling_lowerandcrushdist"},
	{99, "floor_raiseandcrushdoom"},
	{110, "light_raisebyvalue"},
	{111, "light_lowerbyvalue"},
	{112, "light_changetovalue"},
	{113, "light_fade"},
	{114, "light_glow"},
	{116, "light_strobe"},
	{117, "light_stop"},
	{119, "thing_damage"},
	{127, "thing_setspecial"},
	{128, "thrustthingz"},
	{130, "thing_activate"},
	{131, "thing_deactivate"},
	{132, "thing_remove"},
	{133, "thing_destroy"},
	{134, "thing_projectile"},
	{135, "thing_spawn"},
	{136, "thing_projectilegravity"},
	{137, "thing_spawnnofog"},
	{139, "thing_spawnfacing"},
	{172, "plat_upnearestwaitdownstay"},
	{173, "noisealert"},
	{176, "thing_changetid"},
	{179, "changeskill"},
	{191, "setplayerproperty"},
	{195, "ceiling_crushraiseandstaya"},
	{196, "ceiling_crushandraisea"},
	{198, "ceiling_raisebyvaluetimes8"},
	{199, "ceiling_lowerbyvaluetimes8"},
	{200, "generic_floor"},
	{201, "generic_ceiling"},
	{206, "plat_downwaitupstaylip"},
	{212, "sector_setcolor"},
	{213, "sector_setfade"},
	{239, "floor_raisebyvaluetxty"},
	{243, "exit_normal"},
	{244, "exit_secret"},
	// terminator
	{0}
};


//
//

uint64_t tp_hash64(const uint8_t *name)
{
	// Converts string name into 64bit alias (pseudo-hash).
	// CRC64 could work too, but this does not require big table.
	uint64_t ret = 0;
	uint32_t sub = 0;

	while(*name)
	{
		uint8_t in = *name++;

		in &= 0x3F;

		ret ^= (uint64_t)in << sub;
		if(sub > 56)
			ret ^= (uint64_t)in >> (64 - sub);

		sub += 6;
		if(sub >= 64)
			sub -= 64;
	}

	return ret;
}

//
//

int main(int argc, void **argv)
{
	const linespec_t *spec = special_action;

	printf("static const dec_linespec_t special_action[] =\n{\n");

	while(spec->special)
	{
		printf("\t{%u, 0x%016lX}, // %s\n", spec->special, tp_hash64(spec->name), spec->name);
		spec++;
	}

	printf("\t// terminator\n\t{0}\n};\n");

	return 0;
}

