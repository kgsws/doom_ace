// kgsws' Doom ACE
//

// addtions to old flags
#define	MF_ISMONSTER	0x10000000

// new flags
#define	MFN_INACTIVE	0x01
#define MFN_COLORHACK	0x02 // just testing

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

extern thinker_t *thinkercap;
extern void *ptr_MobjThinker;

void map_init();

