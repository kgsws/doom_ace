// kgsws' ACE Engine
////
// Default MAPINFO stuff.

// original clusters
static const map_cluster_t doom_cluster[] =
{
	[CLUSTER_D1_EPISODE1] = {.text_leave = (void*)0x000204C0, .lump_patch = -1, .cluster_num = 1},
	[CLUSTER_D1_EPISODE2] = {.text_leave = (void*)0x0002067C, .lump_patch = -1, .cluster_num = 2},
	[CLUSTER_D1_EPISODE3] = {.text_leave = (void*)0x00020850, .lump_patch = -1, .cluster_num = 3},
	[CLUSTER_D1_EPISODE4] = {.text_leave = NULL, .lump_patch = -1, .cluster_num = 4}, // text is added manually
	[CLUSTER_D2_1TO6] = {.text_leave = (void*)0x00020A40, .lump_patch = -1, .cluster_num = 5},
	[CLUSTER_D2_7TO11] = {.text_leave = (void*)0x00020BD8, .lump_patch = -1, .cluster_num = 6},
	[CLUSTER_D2_12TO20] = {.text_leave = (void*)0x00020E44, .lump_patch = -1, .cluster_num = 7},
	[CLUSTER_D2_21TO30] = {.text_leave = (void*)0x00020F80, .lump_patch = -1, .cluster_num = 8},
	[CLUSTER_D2_LVL31] = {.text_enter = (void*)0x00021170, .lump_patch = -1, .cluster_num = 9},
	[CLUSTER_D2_LVL32] = {.text_enter = (void*)0x00021218, .lump_patch = -1, .cluster_num = 10},
};
static const uint8_t *cluster_flat[DEF_CLUSTER_COUNT] =
{
	[CLUSTER_D1_EPISODE1] = (void*)0x000212A8,
	[CLUSTER_D1_EPISODE2] = (void*)0x000212B4,
	[CLUSTER_D1_EPISODE3] = (void*)0x000212BC,
	[CLUSTER_D1_EPISODE4] = NULL, // MFLR8_3
	[CLUSTER_D2_1TO6] = (void*)0x00021278,
	[CLUSTER_D2_7TO11] = (void*)0x00021280,
	[CLUSTER_D2_12TO20] = (void*)0x00021288,
	[CLUSTER_D2_21TO30] = (void*)0x00021290,
	[CLUSTER_D2_LVL31] = (void*)0x00021298,
	[CLUSTER_D2_LVL32] = (void*)0x000212A0,
};

// missing text
static uint8_t *ep4_text =
	"the spider mastermind must have sent forth\n"
	"its legions of hellspawn before your\n"
	"final confrontation with that terrible\n"
	"beast from hell.  but you stepped forward\n"
	"and brought forth eternal damnation and\n"
	"suffering upon the horde as a true hero\n"
	"would in the face of something so evil.\n"
	"\n"
	"besides, someone was gonna pay for what\n"
	"happened to daisy, your pet rabbit.\n"
	"\n"
	"but now, you see spread before you more\n"
	"potential pain and gibbitude as a nation\n"
	"of demons run amok among our cities.\n"
	"\n"
	"next stop, hell on earth!";

