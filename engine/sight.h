
extern struct extraplane_s *topplane;
extern struct extraplane_s *botplane;

//

uint32_t P_CheckSight(mobj_t *t1, mobj_t *t2) __attribute((regparm(2),no_caller_saved_registers));

uint32_t sight_extra_3d_cover(sector_t *sec);

