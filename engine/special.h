// kgsws' ACE Engine
////

#define SPEC_ACT_USE	0
#define SPEC_ACT_CROSS	1
#define SPEC_ACT_SHOOT	2
#define SPEC_ACT_BUMP	3
#define SPEC_ACT_TYPE_MASK	0x000000FF
#define SPEC_ACT_BACK_SIDE	0x80000000

//

extern uint32_t spec_special;
extern int32_t spec_arg[5];
extern uint32_t spec_success;
extern line_t *spec_line;
extern mobj_t *spec_activator;

extern uint_fast8_t spec_autosave;

//

void spec_activate(line_t*,mobj_t*,uint32_t);

// hooks
void spec_line_cross(uint32_t lidx, uint32_t side) __attribute((regparm(2),no_caller_saved_registers));
uint32_t spec_line_use(mobj_t *mo, line_t *ln) __attribute((regparm(2),no_caller_saved_registers));

