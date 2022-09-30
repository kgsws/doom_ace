// kgsws' ACE Engine
////
// Default MAPINFO stuff.

typedef struct
{
	uint8_t *text;
	uint8_t *flat;
	uint8_t idx;
	uint8_t type;
} def_cluster_t;

// original clusters
static const def_cluster_t d1_clusters[] =
{
	[CLUSTER_D1_EPISODE1] = {.text = (void*)0x000204C0, .flat = (void*)0x000212A8, .idx = 1, .type = 1},
	[CLUSTER_D1_EPISODE2] = {.text = (void*)0x0002067C, .flat = (void*)0x000212B4, .idx = 2, .type = 1},
	[CLUSTER_D1_EPISODE3] = {.text = (void*)0x00020850, .flat = (void*)0x000212BC, .idx = 3, .type = 1},
};

static const def_cluster_t d2_clusters[] =
{
	[CLUSTER_D2_1TO6] = {.text = (void*)0x00020A40, .flat = (void*)0x00021278, .idx = 5, .type = 1},
	[CLUSTER_D2_7TO11] = {.text = (void*)0x00020BD8, .flat = (void*)0x00021280, .idx = 6, .type = 1},
	[CLUSTER_D2_12TO20] = {.text = (void*)0x00020E44, .flat = (void*)0x00021288, .idx = 7, .type = 1},
	[CLUSTER_D2_21TO30] = {.text = (void*)0x00020F80, .flat = (void*)0x00021290, .idx = 8, .type = 1},
	[CLUSTER_D2_LVL31] = {.text = (void*)0x00021170, .flat = (void*)0x00021298, .idx = 9, .type = 0},
	[CLUSTER_D2_LVL32] = {.text = (void*)0x00021218, .flat = (void*)0x000212A0, .idx = 10, .type = 0},
};

