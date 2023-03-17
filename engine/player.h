// kgsws' ACE Engine
////

#define PLAYER_INVBAR_TICS	25
#define PLAYER_LOOK_TOP	0x22000000
#define PLAYER_LOOK_BOT	-0x22000000
#define PLAYER_LOOK_DEAD	0x08000000
#define PLAYER_LOOK_STEP	0x00800000

#define PLAYER_AIRSUPPLY	(20 * 35)

enum
{
	plf_auto_switch,
	plf_auto_aim,
	plf_mouse_look,
};

#define PLF_AUTO_SWITCH	(1 << plf_auto_switch)
#define PLF_AUTO_AIM	(1 << plf_auto_aim)
#define PLF_MOUSE_LOOK	(1 << plf_mouse_look)

//

extern player_info_t player_info[MAXPLAYERS];

extern uint_fast8_t player_info_changed;
extern int_fast16_t player_class_change;

//

void powerup_give(player_t *pl, mobjinfo_t *info);

void player_think(uint32_t idx);

void player_chat_char(uint32_t pidx);

void player_finish(player_t *pl, uint32_t strip);

void player_check_info(player_info_t *info);
