// kgsws' ACE Engine
////

extern uint8_t r_palette[768];

// basic colors
extern uint8_t r_color_black;

//

void init_render();

void r_init_palette(uint8_t*);
uint8_t r_find_color(uint8_t, uint8_t, uint8_t);

void r_draw_plane(visplane_t *pl);

void render_player_view(player_t *pl);

