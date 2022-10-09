// kgsws' ACE Engine
////

typedef struct extraplane_s
{
	struct extraplane_s *next;
	line_t *line;
	sector_t *source;
	fixed_t *height;
	uint16_t *texture;
	uint16_t *pic;
	uint16_t *light;
	uint32_t validcount;
} extraplane_t;

typedef struct extra_height_s
{
	struct extra_height_s *next;
	fixed_t height;
} extra_height_t;

//

extern int16_t *e3d_floorclip;
extern int16_t *e3d_ceilingclip;

extern extra_height_t *e3d_up_height;
extern extra_height_t *e3d_dn_height;

//

void e3d_init(uint32_t count);
void e3d_create();
void e3d_reset();
void e3d_draw_height(fixed_t);

//

visplane_t *e3d_find_plane(fixed_t height, uint32_t picnum, uint32_t lightlevel);
visplane_t *e3d_check_plane(visplane_t *pl, int32_t start, int32_t stop);

//

void e3d_add_height(fixed_t height);

