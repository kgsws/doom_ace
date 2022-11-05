// kgsws' ACE Engine
////

uint32_t hs_slide_traverse(intercept_t *in) __attribute((regparm(2),no_caller_saved_registers));
uint32_t hs_shoot_traverse(intercept_t *in) __attribute((regparm(2),no_caller_saved_registers));
fixed_t hs_intercept_vector(divline_t *v2, divline_t *v1);

fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance) __attribute((regparm(3),no_caller_saved_registers));

