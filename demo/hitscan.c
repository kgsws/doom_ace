// kgsws' Doom ACE
// New hiscan features.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "render.h"

static void hitscan_HitMobj(mobj_t*,fixed_t) __attribute((regparm(2),no_caller_saved_registers));

int32_t *la_damage;

static hook_t hook_list[] =
{
	// required variables
	{0x0002B9B0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&la_damage},
	// custom mobj hit handling
	{0x0002bb30, CODE_HOOK | HOOK_UINT32, 0x24148b}, // 'mov (%esp),%edx'
	{0x0002BB33, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hitscan_HitMobj},
	{0x0002bb38, CODE_HOOK | HOOK_UINT32, 0x18EB}, // 'jmp'
	// terminator
	{0}
};

void hitscan_init()
{
	utils_install_hooks(hook_list);
}

static __attribute((regparm(2),no_caller_saved_registers))
void hitscan_HitMobj(mobj_t *mo, fixed_t x)
{
	// custom puffs / custom blood on mobj can be added here
	fixed_t y, z;
	{
		// GCC bug workaround
		register fixed_t iy asm("ebp");
		register fixed_t iz asm("ebx");
		y = iy;
		z = iz;
	}

	if(mo->flags & MF_NOBLOOD)
	{
		// original Doom puff
		P_SpawnPuff(x, y, z);
	} else
	{
		mobj_t *th;
		// original Doom blood
		// TODO: replace hardcoded values
		z += ((P_Random() - P_Random()) << 10);
		th = P_SpawnMobj(x, y, z, 38);
		th->momz = FRACUNIT * 2;
		th->tics -= P_Random() & 3;
		if(th->tics < 1)
			th->tics = 1;
		if(*la_damage < 9)
			P_SetMobjState(th, 92);
		else
		if(*la_damage < 12)
			P_SetMobjState(th, 91);

		// hardcoded blood hack for demo
		// this is not a complete solution
		// crusher spawns blood too
		// crushed things turn into blood
		switch(mo->type)
		{
			case 17:
			case 15:
				// green
				th->extra.translation = render_get_translation(2);
			break;
			case 14:
				// blue
				th->extra.translation = render_get_translation(3);
			break;
			case 18:
				// fire?
				th->extra.translation = render_get_translation(4);
			break;
			case 13:
				// desaturated
				th->extra.translation = render_get_translation(1);
			break;
		}
	}
}

