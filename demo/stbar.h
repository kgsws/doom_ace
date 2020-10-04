// kgsws' Doom ACE
// Full screen status bar.

extern uint32_t *screenblocks;

void stbar_init();
void stbar_draw(player_t*);
void stbar_big_number_r(int x, int y, int value);
