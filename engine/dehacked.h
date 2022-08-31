// kgsws' ACE Engine
////
// This is a DEHACKED parser.

#define NUMMOBJTYPES	137
#define NUMSTATES	967

//

typedef struct
{
	int32_t doomednum;
	int32_t spawnstate;
	int32_t spawnhealth;
	int32_t seestate;
	int32_t seesound;
	int32_t reactiontime;
	int32_t attacksound;
	int32_t painstate;
	int32_t painchance;
	int32_t painsound;
	int32_t meleestate;
	int32_t missilestate;
	int32_t deathstate;
	int32_t xdeathstate;
	int32_t deathsound;
	int32_t speed;
	int32_t radius;
	int32_t height;
	int32_t mass;
	int32_t damage;
	int32_t activesound;
	int32_t flags;
	int32_t raisestate;
} deh_mobjinfo_t;

typedef struct deh_state_s
{
	int32_t sprite;
	int32_t frame;
	int32_t tics;
	void *action;
	int32_t nextstate;
	int32_t misc1;
	int32_t misc2;
} deh_state_t;

//

extern deh_mobjinfo_t *deh_mobjinfo;
extern deh_state_t *deh_states;
extern weaponinfo_t *deh_weaponinfo;

extern uint_fast8_t deh_no_infight;
extern uint_fast8_t deh_plr_health;
extern uint_fast8_t deh_plr_bullets;

//

void deh_init();

