// kgsws' Doom ACE
//

typedef struct
{
	fixed_t start;
	fixed_t stop;
	fixed_t speed;
	fixed_t crushspeed;
	uint16_t stopsound;
	uint16_t movesound;
	union
	{
		struct
		{
			int texture;
			int special;
			int light;
		};
		struct
		{
			uint16_t opensound;
			uint16_t closesound;
			int16_t delay;
			uint16_t sleeping;
			uint16_t lighttag;
			uint8_t lightmin;
			uint8_t lightmax;
		};
	};
} generic_mover_info_t;

typedef struct
{
	thinker_t thinker;
	sector_t *sector;
	generic_mover_info_t info;
} generic_mover_t;

void generic_door(sector_t *sec, generic_mover_info_t *info) __attribute((regparm(2)));
void generic_door_toggle(generic_mover_t *gm) __attribute((regparm(1)));

void generic_ceiling(sector_t *sec, generic_mover_info_t *info) __attribute((regparm(2)));
void generic_floor(sector_t *sec, generic_mover_info_t *info) __attribute((regparm(2)));

void think_door_mover(generic_mover_t *gm) __attribute((regparm(1),no_caller_saved_registers));
