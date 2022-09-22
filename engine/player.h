// kgsws' ACE Engine
////

#define PLAYER_INVBAR_TICS	25
#define PLAYER_LOOK_TOP	0x20000000
#define PLAYER_LOOK_BOT	-0x20000000
#define PLAYER_LOOK_DEAD	0x08000000
#define PLAYER_LOOK_STEP	0x00800000

enum
{
	plf_auto_switch,
	plf_auto_aim,
	plf_mouse_look,
};

#define PLF_AUTO_SWITCH	(1 << plf_auto_switch)
#define PLF_AUTO_AIM	(1 << plf_auto_aim)
#define PLF_MOUSE_LOOK	(1 << plf_mouse_look)

typedef struct
{
	uint32_t flags;
} player_info_t;

typedef struct
{
	uint8_t auto_switch;
	uint8_t auto_aim;
	uint8_t mouse_look;
} player_config_t;

//

extern player_info_t player_info[MAXPLAYERS];

extern player_config_t plcfg;

extern uint32_t *playeringame;
extern player_t *players;
extern uint32_t *consoleplayer;
extern uint32_t *displayplayer;

extern uint_fast8_t player_flags_changed;

//

void powerup_give(player_t *pl, mobjinfo_t *info);

void player_viewheight(fixed_t wh);

