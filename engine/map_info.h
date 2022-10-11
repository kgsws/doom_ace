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
	[CLUSTER_D1_EPISODE1] = {.text = dtxt_E1TEXT, .flat = dtxt_floor4_8, .idx = 1, .type = 1},
	[CLUSTER_D1_EPISODE2] = {.text = dtxt_E2TEXT, .flat = dtxt_sflr6_1, .idx = 2, .type = 1},
	[CLUSTER_D1_EPISODE3] = {.text = dtxt_E3TEXT, .flat = dtxt_mflr8_4, .idx = 3, .type = 1},
};

static const def_cluster_t d2_clusters[] =
{
	[CLUSTER_D2_1TO6] = {.text = dtxt_C1TEXT, .flat = dtxt_slime16, .idx = 5, .type = 1},
	[CLUSTER_D2_7TO11] = {.text = dtxt_C2TEXT, .flat = dtxt_rrock14, .idx = 6, .type = 1},
	[CLUSTER_D2_12TO20] = {.text = dtxt_C3TEXT, .flat = dtxt_rrock07, .idx = 7, .type = 1},
	[CLUSTER_D2_21TO30] = {.text = dtxt_C4TEXT, .flat = dtxt_rrock17, .idx = 8, .type = 1},
	[CLUSTER_D2_LVL31] = {.text = dtxt_C5TEXT, .flat = dtxt_rrock13, .idx = 9, .type = 0},
	[CLUSTER_D2_LVL32] = {.text = dtxt_C6TEXT, .flat = dtxt_rrock19, .idx = 10, .type = 0},
};

