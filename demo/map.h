// kgsws' Doom ACE
//

#define	MF_ISMONSTER	0x10000000
#define	MF_INACTIVE	0x20000000

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
extern line_t **lines;
extern vertex_t **vertexes;
extern side_t **sides;

void map_init();

