// kgsws' Doom ACE
// Snake! game example.

void snake_init();
void snake_drawer();
int __attribute((regparm(1))) snake_input(event_t *ev); // this function requires one argument, Watcom style

