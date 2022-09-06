// kgsws' ACE Engine
////

#define INV_MAX_COUNT	0xFFFF	// inventory uses uint16_t

typedef struct inventory_s
{
	struct inventory_s *next;
	struct inventory_s *prev;
	uint16_t type;
	uint16_t count;
} inventory_t;

//

uint32_t inventory_is_valid(mobjinfo_t*);
inventory_t *inventory_find(mobj_t *mo, uint16_t type);

uint32_t inventory_give(mobj_t *mo, uint16_t type, uint16_t count);
uint32_t inventory_take(mobj_t *mo, uint16_t type, uint16_t count);
void inventory_clear(mobj_t *mo);

uint32_t inventory_check(mobj_t *mo, uint16_t type);
