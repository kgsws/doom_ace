
extern uint8_t *framebuffer;
extern uint8_t *wipebuffer;
extern int32_t vesa_offset;

//

void vesa_check();
void vesa_init();
void vesa_copy();
void vesa_update();
