// kgsws' ACE Engine
////
#include "sdk.h"
#include "utils.h"
#include "engine.h"
#include "wadfile.h"

//

static num64_t range_defs[4];

//
// funcs

static void wad_add(uint8_t *name)
{
	int32_t fd;
	uint32_t tmp;
	wadhead_t head;
	lumpinfo_t *info;
	wadlump_t *lump = (wadlump_t*)d_drawsegs; // temporary memory
	uint32_t totalsize;
	uint_fast8_t is_ignored = 0;

	doom_printf("   %s ", name);

	fd = doom_open_RD(name);
	if(fd < 0)
	{
		doom_printf("- failed to open\n");
		return;
	}

	totalsize = strlen(name);
	if(totalsize > 4 && (*((uint32_t*)(name + totalsize - 4)) | 0x20202020) != 0x6461772E)
	{
		// load entire file as a single lump
		uint8_t *ptr, *dst;

		tmp = numlumps;

		// realloc lump info
		numlumps++;
		lumpinfo = ldr_realloc(lumpinfo, numlumps * sizeof(lumpinfo_t));
		info = lumpinfo + tmp;

		// copy file name
		ptr = name + totalsize - 4;

		while(ptr > name && ptr[-1] != '\\' && ptr[-1] != '/')
			*ptr--;

		info->wame = 0;
		dst = info->name;
		while(*ptr && *ptr != '.' && ptr < info->name + 8)
		{
			uint8_t in = *ptr++;
			if(in >= 'a' && in <= 'z')
				in &= ~0x20;
			*dst++ = in;
		}

		// create entry
		info->fd = fd;
		info->offset = 0;
		info->size = doom_filelength(fd);

		doom_printf("- file, %u bytes; %.8s\n", info->size, info->name);

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

	tmp = numlumps;

	// realloc lump info
	numlumps += head.numlumps;
	lumpinfo = ldr_realloc(lumpinfo, numlumps * sizeof(lumpinfo_t));
	info = lumpinfo + tmp;

	doom_lseek(fd, head.diroffs, SEEK_SET);

	// load lumps
	totalsize = 0;
	for(uint32_t i = 0; i < head.numlumps; i++, info++)
	{
		wadlump_t *lmp = lump + (i & 511);

		if(lmp == lump)
		{
			uint32_t size = (head.numlumps - i) * sizeof(wadlump_t);

			if(size > 512 * sizeof(wadlump_t))
				size = 512 * sizeof(wadlump_t);

			tmp = doom_read(fd, lump, size);
			if(tmp != size)
				// reading failed; fill with dummy values
				memset(lump, 0, size);
		}

		info->wame = lmp->wame;
		info->fd = fd;
		info->offset = lmp->offset;
		info->size = lmp->size;

		// check for ignore
		if(is_ignored)
		{
			if(info->wame == 0x534B49505F454341) // ACE_PIKS
				is_ignored = 0;
			info->wame |= 0x80;
		} else
		{
			if(info->wame == 0x50494B535F454341) // ACE_SKIP
			{
				is_ignored = 1;
				info->wame |= 0x80;
			}
		}

		totalsize += lmp->size;
	}

	doom_printf("- %u entries, %u bytes\n", head.numlumps, totalsize);
}

//
// API

void wad_init()
{
	doom_printf("[ACE] wad_init\n");

	ldr_alloc_message = "WAD";

	lumpinfo = ldr_malloc(1);

	for(uint32_t i = 0; i < MAXWADFILES && wadfiles[i]; i++)
		wad_add(wadfiles[i]);
#if 0
	if(!numlumps)
		engine_error("Umm. WADs are empty ...", "WAD");
#endif
	if(numlumps >= 65535)
		engine_error("WAD", "Wow. Soo many lumps ...");

	lumpcache = ldr_malloc(numlumps * sizeof(void*));
	memset(lumpcache, 0, numlumps * sizeof(void*));
}

uint64_t wad_name64(const uint8_t *name)
{
	union
	{
		uint64_t w;
		uint8_t b[8];
	} nm;

	nm.w = 0;
	for(uint32_t i = 0; i < 8; i++)
	{
		register uint8_t in = name[i];
		if(!in)
			break;
		if(in >= 'a' && in <= 'z')
			in &= ~0x20; // uppercase only
		nm.b[i] = in;
	}

	return nm.w;
}

int32_t wad_check_lump(const uint8_t *name)
{
	uint64_t wame;
	uint32_t idx;
	lumpinfo_t *li = lumpinfo;

	// search as 64bit number
	wame = wad_name64(name);

	// do a backward search
	idx = numlumps;
	do
	{
		idx--;
		if(li[idx].wame == wame)
			return idx;
	} while(idx);

	// not found
	return -1;
}

int32_t wad_get_lump(const uint8_t *name)
{
	int32_t idx;

	idx = wad_check_lump(name);
	if(idx < 0)
		engine_error("WAD", "Can't find lump %s!", name);

	return idx;
}

uint32_t wad_read_lump(void *dest, int32_t idx, uint32_t limit)
{
	int32_t ret;
	lumpinfo_t *li = lumpinfo + idx;

	if(li->size < limit)
		limit = li->size;

	doom_lseek(li->fd, li->offset, SEEK_SET);
	ret = doom_read(li->fd, dest, limit);
	if(ret != limit)
		engine_error("WAD", "Lump %.8s read failed!", li->name);

	return limit;
}

void *wad_cache_lump(int32_t idx, uint32_t *size)
{
	uint8_t *data;
	lumpinfo_t *li = lumpinfo + idx;

	if(!li->size)
		engine_error("WAD", "Lump %.8s is empty!");

	data = doom_malloc(li->size + 4); // extra space for text files
	if(!data)
		engine_error("WAD", "Lump %.8s allocation failed!", li->name);

	wad_read_lump(data, idx, li->size);
	data[li->size] = 0; // string terminator

	if(size)
		*size = li->size;

	return data;
}

void *wad_cache_optional(const uint8_t *name, uint32_t *size)
{
	int32_t idx;
	uint8_t *data;
	lumpinfo_t *li;

	idx = wad_check_lump(name);
	if(idx < 0)
		return NULL;

	li = lumpinfo + idx;

	data = doom_malloc(li->size + 4); // extra space for text files
	if(!data)
		engine_error("WAD", "Lump %.8s allocation failed!", li->name);

	wad_read_lump(data, idx, li->size);
	data[li->size] = 0; // string terminator

	if(size)
		*size = li->size;

	return data;
}

void wad_handle_range(uint16_t ident, void (*cb)(lumpinfo_t*))
{
	lumpinfo_t *li = lumpinfo;
	lumpinfo_t *le = lumpinfo + numlumps;
	uint32_t is_inside = 0;

	if(ident > 255)
	{
		range_defs[0].u64 = (0x0054524154535F00 << 8) | ident;
		range_defs[2].u64 = (0x000000444E455F00 << 8) | ident;
		range_defs[1].u64 = 0xFFFFFFFFFFFFFFFF;
		range_defs[3].u64 = 0xFFFFFFFFFFFFFFFF;
	} else
	{
		range_defs[0].u64 = 0x0054524154535F00 | ident;
		range_defs[2].u64 = 0x000000444E455F00 | ident;
		ident |= ident << 8;
		range_defs[1].u64 = 0x54524154535F0000 | ident;
		range_defs[3].u64 = 0x0000444E455F0000 | ident;
	}

	for( ; li < le; li++)
	{
		if(is_inside)
		{
			if(li->wame == range_defs[2].u64 || li->wame == range_defs[3].u64)
				is_inside = 0;
			else
				cb(li);
		} else
		{
			if(li->wame == range_defs[0].u64)
				is_inside = 1;
			else
			if(li->wame == range_defs[1].u64)
				is_inside = 2;
		}
	}

	if(is_inside)
		engine_error("WAD", "Unclosed range %.8s / %.8s\n", range_defs[is_inside-1].u8, range_defs[is_inside+1].u8);
}

void wad_handle_lump(const uint8_t *name, void (*cb)(lumpinfo_t*))
{
	uint64_t wame;
	lumpinfo_t *li = lumpinfo;

	// search as 64bit number
	wame = wad_name64(name);

	for(uint32_t i = 0; i < numlumps; i++, li++)
	{
		if(li->wame == wame)
			cb(li);
	}
}

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// disable 'W_InitMultipleFiles'
	{0x00038A30, CODE_HOOK | HOOK_UINT8, 0xC3},
	// disable call to 'W_Reload' in 'P_SetupLevel'
	{0x0002E858, CODE_HOOK | HOOK_SET_NOPS, 5},
};

