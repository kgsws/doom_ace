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

// internal LOCKDEFS
static uint8_t engine_lockdefs[] =
"l 1"
"{"
	"RedCard\n"
	"m \"You need a red card to open this door\"\n"
	"r \"You need a red card to activate this object\"\n"
	"c 255 0 0"
"}"
"l 2"
"{"
	"BlueCard\n"
	"m \"You need a blue card to open this door\"\n"
	"r \"You need a blue card to activate this object\"\n"
	"c 0 0 255"
"}"
"l 3"
"{"
	"YellowCard\n"
	"m \"You need a yellow card to open this door\"\n"
	"r \"You need a yellow card to activate this object\"\n"
	"c 255 255 0"
"}"
"l 4"
"{"
	"RedSkull\n"
	"m \"You need a red skull to open this door\"\n"
	"r \"You need a red skull to activate this object\"\n"
	"c 255 0 0"
"}"
"l 5"
"{"
	"BlueSkull\n"
	"m \"You need a blue skull to open this door\"\n"
	"r \"You need a blue skull to activate this object\"\n"
	"c 0 0 255"
"}"
"l 6"
"{"
	"YellowSkull\n"
	"m \"You need a yellow skull to open this door\"\n"
	"r \"You need a yellow skull to activate this object\"\n"
	"c 255 255 0"
"}"
"l 100"
"{"
	"m \"Any key will open this door\"\n"
	"r \"Any key will activate this object\"\n"
	"c 128 128 255"
"}"
"l 101"
"{"
	"BlueCard\n"
	"BlueSkull\n"
	"YellowCard\n"
	"YellowSkull\n"
	"RedCard\n"
	"RedSkull\n"
	"m \"You need all six keys to open this door\"\n"
	"r \"You need all six keys to activate this object\""
"}"
"l 129"
"{"
	"a{RedCard RedSkull}"
	"g 22CE0\n"
	"h 22C34\n"
	"c 255 0 0"
"}"
"l 130"
"{"
	"a{BlueCard BlueSkull}"
	"g 22C90\n"
	"h 22C08\n"
	"c 0 0 255"
"}"
"l 131"
"{"
	"a{YellowCard YellowSkull}"
	"g 22CB8\n"
	"h 22C60\n"
	"c 255 255 0"
"}"
"l 132"
"{"
	"a{RedCard RedSkull}"
	"g 22CE0\n"
	"h 22C34\n"
	"c 255 0 0"
"}"
"l 133"
"{"
	"a{BlueCard BlueSkull}"
	"g 22C90\n"
	"h 22C08\n"
	"c 0 0 255"
"}"
"l 134"
"{"
	"a{YellowCard YellowSkull}"
	"g 22CB8\n"
	"h 22C60\n"
	"c 255 255 0"
"}"
"l 228"
"{"
	"m \"Any key will open this door\"\n"
	"r \"Any key will activate this object\"\n"
	"c 128 128 255"
"}"
"l 229"
"{"
	"a{BlueCard BlueSkull}"
	"a{YellowCard YellowSkull}"
	"a{RedCard RedSkull}"
	"m \"You need all three keys to open this door\"\n"
	"r \"You need all three keys to activate this object\""
"}"
;

