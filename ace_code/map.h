// kgsws' Doom ACE
//

// MAP flags
#define MTF_INACTIVE	0x10
#define MTF_CLASS0	0x20
#define MTF_CLASS1	0x40
#define MTF_CLASS2	0x80
#define MTF_SINGLE	0x100
#define MTF_COOPERATIVE	0x200
#define MTF_DEATHMATCH	0x400
#define MTF_SHADOW	0x800
#define MTF_ALTSHADOW	0x1000
#define MTF_FRIENDLY	0x2000
#define MTF_STANDSTILL	0x4000

extern uint32_t *numlines;
extern uint32_t *numsectors;
extern line_t **lines;
extern vertex_t **vertexes;
extern side_t **sides;
extern sector_t **sectors;

// API
void map_init();

// hooks
int32_t map_get_map_lump(char*) __attribute((regparm(2),no_caller_saved_registers));
uint32_t map_get_spawn_type(uint32_t) __attribute((regparm(2),no_caller_saved_registers));
void map_ItemPickup(mobj_t*,mobj_t*) __attribute((regparm(2),no_caller_saved_registers));

