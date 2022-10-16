// kgsws' ACE Engine
////

enum
{
	RS_NORMAL, // must be zero
	RS_FUZZ,
	RS_SHADOW,
	RS_TRANSLUCENT,
	RS_ADDITIVE,
	RS_INVISIBLE, // must be last
	NUM_RENDER_STYLES
};

//

extern uint8_t r_palette[768];

// basic colors
extern uint8_t r_color_black;

// render style tables
extern uint8_t *render_trn0;
extern uint8_t *render_trn1;
extern uint8_t *render_add;

//

void init_render();

void r_init_palette(uint8_t*);
uint8_t r_find_color(uint8_t, uint8_t, uint8_t);

void r_draw_plane(visplane_t *pl);

void render_player_view(player_t *pl);

