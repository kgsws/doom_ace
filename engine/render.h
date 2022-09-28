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

extern fixed_t *centeryfrac;
extern fixed_t *yslope;

extern uint32_t *dc_x;
extern uint32_t *dc_yl;
extern uint32_t *dc_yh;
extern uint8_t **dc_source;

extern fixed_t *sprtopscreen;
extern fixed_t *spryscale;

extern int16_t **mfloorclip;
extern int16_t **mceilingclip;

extern void (**colfunc)();

extern uint8_t r_palette[768];

//

void init_render();

void r_init_palette(uint8_t*);
uint8_t r_find_color(uint8_t, uint8_t, uint8_t);

