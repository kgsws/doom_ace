// kgsws' Doom ACE
// Snake! game example.

void snake_init();
void snake_drawer() __attribute((no_caller_saved_registers));
int snake_input(event_t *ev) __attribute((regparm(1),no_caller_saved_registers));
void snake_start() __attribute((regparm(1),no_caller_saved_registers));
