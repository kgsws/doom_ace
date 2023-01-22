
extern uint8_t *dr_tinttab;
extern uint8_t ds_maskcolor;

extern uint8_t *draw_patch_color;

extern uint8_t loop_dc_start[0x11];
extern uint8_t r_dc_unroll[];
extern void *r_dc_jump[SCREENWIDTH];

extern uint8_t loop_ds_start[0x17];
extern uint8_t r_ds_unroll[];
extern void *r_ds_jump[SCREENWIDTH];

extern num32_t r_dc_mask;

//

void init_draw();
void draw_patch_to_memory(patch_t*, int32_t, int32_t, void*, uint32_t, uint32_t);

void V_DrawPatchDirect(int32_t, int32_t, patch_t*) __attribute((regparm(3),no_caller_saved_registers)); // three!
void V_DrawPatchTranslated(int32_t, int32_t, patch_t*) __attribute((regparm(3),no_caller_saved_registers)); // three!
void V_DrawPatchTint0(int32_t, int32_t, patch_t*) __attribute((regparm(3),no_caller_saved_registers)); // three!
void V_DrawPatchTint1(int32_t, int32_t, patch_t*) __attribute((regparm(3),no_caller_saved_registers)); // three!

void R_DrawColumn() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawFuzzColumn() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawShadowColumn() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawColumnTint0() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawColumnTint1() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawTranslatedColumn() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawTranslatedColumnTint0() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawTranslatedColumnTint1() __attribute((regparm(2),no_caller_saved_registers));

void R_DrawSpan() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawUnknownSpan() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawSpanTint0() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawSpanTint1() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawMaskedSpan() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawMaskedSpanTint0() __attribute((regparm(2),no_caller_saved_registers));
void R_DrawMaskedSpanTint1() __attribute((regparm(2),no_caller_saved_registers));

