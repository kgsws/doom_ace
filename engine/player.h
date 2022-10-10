// kgsws' ACE Engine
////

#define PLAYER_INVBAR_TICS	25
#define PLAYER_LOOK_TOP	0x22000000
#define PLAYER_LOOK_BOT	-0x22000000
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

//

extern player_info_t player_info[MAXPLAYERS];

extern uint_fast8_t player_flags_changed;

//

void powerup_give(player_t *pl, mobjinfo_t *info);

void player_viewheight(fixed_t wh);

void player_finish(player_t *pl);

