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
int32_t wad_check_lump(uint8_t *name);
int32_t wad_get_lump(uint8_t *name);
uint32_t wad_read_lump(void *dest, int32_t idx, uint32_t limit);
void *wad_cache_lump(int32_t idx, uint32_t *size);
void *wad_cache_optional(uint8_t *name, uint32_t *size);

void wad_handle_range(uint16_t ident, void (*cb)(lumpinfo_t*));