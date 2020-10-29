// kgsws' Doom ACE
//

extern uint32_t *setsizeneeded;
extern void **ds_colormap;
extern void **dc_colormap;
extern void **dc_translation;
extern void **fixedcolormap;
extern uint32_t **spritelights;
extern seg_t **curline;
extern int *extralight;
extern void **colormaps;

extern void **colfunc;
extern void **basecolfunc;
extern void **fuzzcolfunc;

// API
void render_init();
void *render_get_translation(int num) __attribute((regparm(2)));

// hooks
void render_SetViewSize() __attribute((regparm(2),no_caller_saved_registers));
void render_planeColormap(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
int render_planeLight(int) __attribute((regparm(2),no_caller_saved_registers));
void render_segColormap(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
int render_segLight(int32_t,vertex_t*) __attribute((regparm(2),no_caller_saved_registers));
void render_maskedColormap() __attribute((regparm(2),no_caller_saved_registers));
int render_maskedLight() __attribute((regparm(2),no_caller_saved_registers));
void render_spriteColormap(mobj_t*,vissprite_t*) __attribute((regparm(2),no_caller_saved_registers));
int render_spriteLight() __attribute((regparm(2),no_caller_saved_registers));
vissprite_t *render_pspColormap(vissprite_t*,player_t*) __attribute((regparm(2),no_caller_saved_registers));
void render_spriteColfunc(void*) __attribute((regparm(2),no_caller_saved_registers));
void *render_skyColormap() __attribute((regparm(2),no_caller_saved_registers));

