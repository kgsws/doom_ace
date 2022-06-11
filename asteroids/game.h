
typedef struct
{
	int16_t x, y;
} point_t;

void game_init();
void game_drawer() __attribute((regparm(2),no_caller_saved_registers));
void game_ticker() __attribute((regparm(2),no_caller_saved_registers));
int game_input(event_t *ev) __attribute((regparm(2),no_caller_saved_registers));

