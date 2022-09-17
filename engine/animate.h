// kgsws' ACE Engine
////

#define MAX_BUTTONS	32
#define BUTTON_TIME	35
#define BUTTON_SNDTICK	350

//

typedef struct
{
	uint16_t *dest;
	line_t *line;
	struct aswitch_s *swtch;
	uint32_t base;
	uint16_t animate;
	uint16_t delay;
	union
	{
		uint32_t soundtick;
		degenmobj_t soundorg;
	};
} switch_t;

//

extern switch_t active_switch[MAX_BUTTONS];

//

void init_animations();

// level think
void animate_step();
void clear_buttons();

// save
uint16_t anim_switch_type(switch_t *slot);
uint64_t anim_switch_texture(switch_t *slot);

// load
switch_t *anim_switch_make(uint16_t type, line_t *line, uint64_t wame);

