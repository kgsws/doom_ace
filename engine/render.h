// kgsws' ACE Engine
////

#define RENDER_TABLE_PROGRESS	(3 * 128)

#define FONT_COLOR_WHITE	4
#define ICE_CMAP_IDX	5

#define MAX_SECTOR_COLORS	32

#define FONT_TRANSLATION_COUNT	25
#define FONT_COLOR_COUNT	32 // techically its one less, first color is transparent

enum
{
	TRANSLATION_DEHPLR2,
	TRANSLATION_DEHPLR3,
	TRANSLATION_DEHPLR4,
	TRANSLATION_PLAYER0,
	TRANSLATION_PLAYER1,
	TRANSLATION_PLAYER2,
	TRANSLATION_PLAYER3,
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

typedef union
{
	uint32_t w;
	struct
	{
		uint8_t r, g, b, l;
	};
} pal_col_t;

typedef struct
{
	uint16_t color;
	uint16_t fade;
	uint8_t *cmap;
	uint8_t *fmap;
} sector_light_t;

typedef struct
{
	uint8_t cmap[256*4]; // gold, red, green, blue; used for extra colormap and translation
	uint8_t trn0[256*256];
	uint8_t trn1[256*256];
	uint8_t addt[256*256];
	uint8_t fmap[FONT_TRANSLATION_COUNT * FONT_COLOR_COUNT]; // ZDoom font colors
} render_tables_t;

//

extern pal_col_t r_palette[256];

// basic colors
extern uint8_t r_color_duplicate;
extern uint8_t r_color_black;

// render tables
extern int32_t render_tables_lump;
extern render_tables_t *render_tables;
extern uint8_t *render_trn0;
extern uint8_t *render_trn1;
extern uint8_t *render_add;
extern uint8_t *render_translation;
extern uint8_t *blood_translation;

extern uint64_t *translation_alias;
extern uint32_t translation_count;
extern uint32_t blood_color_count;

extern visplane_t *ptr_visplanes;
extern drawseg_t *ptr_drawsegs;

// sector light / fog

extern uint32_t sector_light_count;
extern sector_light_t sector_light[MAX_SECTOR_COLORS];

//

void init_render();
void render_preinit(uint8_t*);
void render_generate_blood();
uint32_t render_setup_light_color(uint32_t);

uint8_t r_find_color(uint8_t, uint8_t, uint8_t);
uint8_t r_find_color_4(uint16_t color);

void r_draw_plane(visplane_t *pl);

uint8_t *r_translation_by_name(const uint8_t *name);
int32_t r_translation_find(const uint8_t *name);
uint32_t r_add_blood_color(uint32_t color);
uint8_t *r_get_blood_color(uint32_t idx);

void r_fixed_palette(uint16_t fade);

void *r_generate_player_color(uint32_t idx);

void render_player_view(player_t *pl);

