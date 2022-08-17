// kgsws' ACE Engine
////

enum
{
	ANIM_TYPE_FLAT_SINGLE,
	ANIM_TYPE_FLAT_RANGE,
	ANIM_TYPE_TEXTURE_SINGLE,
	ANIM_TYPE_TEXTURE_RANGE,
	//
	ANIM_TERMINATOR = 0xFF
};

typedef struct
{
	uint16_t pic;
	uint16_t tick;
} animframe_t;

typedef struct
{
	uint16_t tick_total;
	animframe_t frame[];
} animt_single_t;

typedef struct
{
	uint8_t type;
	uint8_t count;
	uint16_t target;
} anim_header_t;

typedef struct
{
	uint16_t tics;
	uint16_t pic[];
} animt_range_t;

typedef struct
{
	anim_header_t head;
	union
	{
		animt_single_t single;
		animt_range_t range;
	};
} animation_t;

//

void init_animations();

