// kgsws' ACE Engine
////

#define RENDER_TABLE_PROGRESS	(3 * 128)

#define FONT_COLOR_WHITE	4
#define ICE_CMAP_IDX	5

enum
{
	TRANSLATION_PLAYER2,
	TRANSLATION_PLAYER3,
	TRANSLATION_PLAYER4,
	TRANSLATION_ICE,
	//
	NUM_EXTRA_TRANSLATIONS
};

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

typedef struct
{
	uint8_t r, g, b, l;
} pal_col_t;

typedef struct
{
	uint8_t cmap[256*5]; // gold, red, green, blue, white; used for extra colormap, fonts and translation
	uint8_t trn0[256*256];
	uint8_t trn1[256*256];
	uint8_t addt[256*256];
} render_tables_t;

//

extern pal_col_t r_palette[256];

// basic colors
extern uint8_t r_color_black;

// render tables
extern int32_t render_tables_lump;
extern render_tables_t *render_tables;
extern uint8_t *render_trn0;
extern uint8_t *render_trn1;
extern uint8_t *render_add;
extern uint8_t *render_translation;

extern uint64_t *translation_alias;
extern uint32_t translation_count;

//

void init_render();
void render_preinit(uint8_t*);

uint8_t r_find_color(uint8_t, uint8_t, uint8_t);

void r_draw_plane(visplane_t *pl);

uint8_t *r_translation_by_name(const uint8_t *name);

void render_player_view(player_t *pl);

