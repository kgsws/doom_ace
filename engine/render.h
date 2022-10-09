// kgsws' ACE Engine
////

extern uint32_t *viewheight;
extern uint32_t *viewwidth;

extern int32_t *centerx;
extern int32_t *centery;

extern int32_t *viewwindowx;
extern int32_t *viewwindowy;

extern uint32_t *screenblocks;

extern uint32_t *usegamma;

extern fixed_t *viewx;
extern fixed_t *viewy;
extern fixed_t *viewz;
extern angle_t *viewangle;
extern int32_t *extralight;

extern fixed_t *centeryfrac;
extern fixed_t *yslope;

extern uint32_t *dc_x;
extern uint32_t *dc_yl;
extern uint32_t *dc_yh;
extern uint8_t **dc_source;
extern fixed_t *dc_texturemid;

extern uint8_t **ds_source;

extern fixed_t *sprtopscreen;
extern fixed_t *spryscale;

extern int16_t **mfloorclip;
extern int16_t **mceilingclip;

extern void (**colfunc)();

extern uint8_t r_palette[768];

// basic colors
extern uint8_t r_color_black;

//

void init_render();

void r_init_palette(uint8_t*);
uint8_t r_find_color(uint8_t, uint8_t, uint8_t);

void r_draw_plane(visplane_t *pl);

