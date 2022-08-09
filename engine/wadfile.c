// kgsws' ACE Engine
////
#include "sdk.h"
#include "utils.h"
#include "engine.h"
#include "wadfile.h"

//

uint8_t **wadfiles;
uint32_t *numlumps;
lumpinfo_t **lumpinfo;
void ***lumpcache;

static uint32_t lumpcount;

//
// funcs

static void wad_add(uint8_t *name)
{
	// TODO: single file support (.lmp)
	int32_t fd;
	uint32_t tmp;
	wadhead_t head;
	lumpinfo_t *info;
	wadlump_t *lump = (void*)0x0002D0A0 + doom_data_segment; // drawsegs
	uint32_t totalsize = 0;

	doom_printf("   %s ", name);

	fd = doom_open_RD(name);
	if(fd < 0)
	{
		doom_printf("- failed to open\n");
		return;
	}

	tmp = doom_read(fd, &head, sizeof(wadhead_t));
	if(	tmp != sizeof(wadhead_t) ||
		(head.id != 0x44415749 && head.id != 0x44415750)
	){
		doom_printf("- invalid\n");
		doom_close(fd);
		return;
	}

	tmp = lumpcount + head.numlumps;
	tmp *= sizeof(lumpinfo_t);

	// realloc lump info
	info = doom_realloc(*lumpinfo, tmp);
	if(!info)
	{
		doom_printf("- realloc failed\n");
		doom_close(fd);
		return;
	}

	*lumpinfo = info;
	info += lumpcount;
	lumpcount = lumpcount + head.numlumps;

	doom_lseek(fd, head.diroffs, SEEK_SET);

	// load lumps
	for(uint32_t i = 0; i < head.numlumps; i++, info++)
	{
		wadlump_t *lmp = lump + (i & 1023);

		if(lmp == lump)
		{
			uint32_t size = (head.numlumps - i) * sizeof(wadlump_t);

			if(size > 1024 * sizeof(wadlump_t))
				size = 1024 * sizeof(wadlump_t);

			tmp = doom_read(fd, lump, size);
			if(tmp != size)
				// reading failed; fill with dummy values
				memset(lump, 0, size);
		}

		info->wame = lmp->wame;
		info->handle = fd;
		info->position = lmp->offset;
		info->size = lmp->size;

		totalsize += lmp->size;
	}

	doom_printf("- %u entries, %u bytes\n", head.numlumps, totalsize);
}

//
// API

void wad_init()
{
	doom_printf("[ACE] wad_init\n");

	*lumpinfo = doom_malloc(1);

	for(uint32_t i = 0; i < MAXWADFILES && wadfiles[i]; i++)
		wad_add(wadfiles[i]);

	if(!lumpcount)
		I_Error("Umm. WADs are empty ...");

	*lumpcache = doom_malloc(lumpcount * sizeof(void*));
	if(!*lumpcache)
		I_Error("Failed to allocate lump cache.");

	*numlumps = lumpcount;
	memset(*lumpcache, 0, lumpcount * sizeof(void*));
}

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// disable 'W_InitMultipleFiles'
	{0x00038A30, CODE_HOOK | HOOK_UINT8, 0xC3},
	// disable call to 'W_Reload'
	{0x0002E858, CODE_HOOK | HOOK_SET_NOPS, 5},
	// import variables
	{0x00029730, DATA_HOOK | HOOK_IMPORT, (uint32_t)&wadfiles},
	{0x00074FA0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numlumps},
	{0x00074FA4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&lumpinfo},
	{0x00074F94, DATA_HOOK | HOOK_IMPORT, (uint32_t)&lumpcache},
};

