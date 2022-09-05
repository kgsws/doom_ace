// kgsws' ACE Engine
////

//

extern uint32_t *playeringame;
extern player_t *players;

extern uint32_t *consoleplayer;
extern uint32_t *displayplayer;

extern thinker_t *thinkercap;

//

#define mobj_set_animation(mo,anim)	mobj_set_state((mo), STATE_SET_ANIMATION((anim), 0))

// internaction
void mobj_damage(mobj_t *target, mobj_t *cause, mobj_t *source, uint32_t damage, uint32_t extra);
void explode_missile(mobj_t *mo) __attribute((regparm(2),no_caller_saved_registers));

// inventory
uint32_t mobj_give_inventory(mobj_t *mo, uint16_t type, uint16_t count);

// helpers
void mobj_for_each(uint32_t (*cb)(mobj_t*));

