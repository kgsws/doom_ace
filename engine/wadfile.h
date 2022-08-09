// kgsws' ACE Engine
////
// WAD initialization replacement.
// Fixes bug: https://doomwiki.org/wiki/PWAD_size_limit

extern uint8_t **wadfiles;
extern uint32_t *numlumps;
extern lumpinfo_t **lumpinfo;
extern void ***lumpcache;

//

void wad_init();

