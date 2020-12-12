// kgsws' Doom ACE
//

// special sprite
#define KEEP_SPRITE_OF_FRAME	0x7FFF

// animation destination state
#define MOBJ_ANIM_STATE(anim,state)	((((anim)+1) << 24) | state)

// animation list
// care, raw numbers are used in hooks
// keep in sync with decorate.c 'actor_anim_list'
enum
{
	// original
	MOANIM_SPAWN, // this must be zero
	MOANIM_SEE,
	MOANIM_PAIN,
	MOANIM_MELEE,
	MOANIM_MISSILE,
	MOANIM_DEATH,
	MOANIM_XDEATH,
	MOANIM_RAISE,
	// extra
	MOANIM_HEAL,
	MOANIM_DEATH_FIRE,
	MOANIM_DEATH_ICE,
	MOANIM_DEATH_DISINTEGRATE,
	// inventory
	INANIM_PICKUP,
	INANIM_USE,
	// weapons
	WEANIM_READY,
	WEANIM_DESELECT,
	WEANIM_SELECT,
	WEANIM_FIRE1,
	WEANIM_FIRE2,
	WEANIM_HOLD1,
	WEANIM_HOLD2,
	WEANIM_FLASH1,
	WEANIM_FLASH2,
	WEANIM_DEADLOW,
	//
	MOBJ_ANIM_COUNT
};

extern uint16_t player_class;

//
//

uint32_t *P_GetAnimPtr(uint8_t anim, mobjinfo_t*) __attribute((regparm(2),no_caller_saved_registers));
uint32_t P_SetMobjState(mobj_t *mo, uint32_t state) __attribute((regparm(2),no_caller_saved_registers));
void P_SetMobjAnimation(mobj_t *mo, uint8_t anim) __attribute((regparm(2),no_caller_saved_registers));

void P_ExplodeMissile(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

void set_player_viewheight(fixed_t wh) __attribute((regparm(2),no_caller_saved_registers));

// hooks
void mobj_kill_animation(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void mobj_spawn_init(mobj_t*,uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void mobj_player_init(player_t*) __attribute((regparm(2),no_caller_saved_registers));
void mobj_use_fail(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));
void player_land(mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

// state changes in codeptr
// could even be new animation code
static inline void P_ChangeMobjState(mobj_t *mo, uint32_t code)
{
	mo->state = (void*)code;
	mo->tics = -2;
}

