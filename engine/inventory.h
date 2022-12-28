// kgsws' ACE Engine
////

#define INV_MAX_COUNT	0xFFFF	// inventory uses uint16_t
#define INV_SLOT_PLAYER	64	// players usually have large inventories
#define INV_SLOT_MOBJ	8	// other objects usually have no inventories

typedef struct invitem_s
{
	uint16_t type;
	uint16_t count;
} invitem_t;

typedef struct inventory_s
{
	uint32_t numslots;
	invitem_t slot[];
} inventory_t;

//

uint32_t inventory_is_valid(mobjinfo_t*);

uint32_t inventory_give(mobj_t *mo, uint16_t type, uint16_t count);
uint32_t inventory_take(mobj_t *mo, uint16_t type, uint16_t count);
invitem_t *inventory_find(mobj_t *mo, uint16_t type);
void inventory_clear(mobj_t *mo);
void inventory_hubstrip(mobj_t *mo);

uint32_t inventory_check(mobj_t *mo, uint16_t type);

int32_t inv_player_next(mobj_t *mo);
int32_t inv_player_prev(mobj_t *mo);

