
void intro_init();
void intro_drawer() __attribute((regparm(2),no_caller_saved_registers));
void intro_ticker() __attribute((regparm(2),no_caller_saved_registers));
int intro_input(event_t *ev) __attribute((regparm(2),no_caller_saved_registers));

void intro_draw_text(uint32_t idx);

extern void *playermo;

extern uint32_t intro_start;
extern uint32_t intro_end;

