
#define TRN_FLAG_LIQUID	1
#define TRN_FLAG_PROTECT	2

#define TRN_SPLASH_NOALERT	1

#define TERRAIN_LOW_MASS	10

typedef struct
{
	uint16_t smallclass;
	uint16_t smallsound;
	uint16_t baseclass;
	uint16_t chunkclass;
	uint16_t sound;
	fixed_t bz;
	uint8_t sx, sy, sz;
	uint8_t smallclip; // must be zero to spawn, but default is not zero
	uint8_t flags;
} terrain_splash_t;

typedef struct
{
	uint8_t splash;
	uint8_t flags;
	uint8_t damagetype;
	uint8_t damagetimemask;
	uint16_t damageamount;
	fixed_t friction;
} terrain_terrain_t;

//

extern uint32_t num_terrain;
extern terrain_terrain_t *terrain;

extern uint8_t *flatterrain;

//

void init_terrain();
uint32_t terrain_hit_splash(mobj_t *mo, fixed_t x, fixed_t y, fixed_t z, int32_t flat);
void terrain_explosion_splash(mobj_t *mo, fixed_t dist);
void terrain_sound();

