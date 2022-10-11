// kgsws' ACE Engine
////
// File writer and reader with buffering.
#include "sdk.h"
#include "utils.h"
#include "engine.h"
#include "wadfile.h"
#include "filebuf.h"

#define BUFFER_SIZE	(4*1024)

static uint8_t buffer[BUFFER_SIZE];
static uint8_t *bptr;
static uint8_t *eptr;
static int ffd = -1;
static uint32_t lump_offset;
static uint32_t lump_size;

//
// rAPI

void reader_open_lump(int32_t lump)
{
	int32_t tmp;

	if(ffd >= 0)
		I_Error("[READER] Attempt to open second file!");

	ffd = lumpinfo[lump].fd;
	lump_offset = lumpinfo[lump].offset;
	lump_size = lumpinfo[lump].size;

	eptr = buffer;
	bptr = buffer;
}

void reader_open(uint8_t *name)
{
	int32_t tmp;

	if(ffd >= 0)
		I_Error("[READER] Attempt to open second file!");

	ffd = doom_open_RD(name);
	if(ffd < 0)
		I_Error("[READER] Unable to open '%s'!", name);

	lump_offset = 0;

	eptr = buffer;
	bptr = buffer;
}

void reader_close()
{
	if(ffd < 0)
		I_Error("[READER] Double close or something!");

	if(!lump_offset)
		doom_close(ffd);

	ffd = -1;
}

//
// rAPI - buffering

uint32_t reader_seek(uint32_t offs)
{
	if(lump_offset)
		I_Error("[READER] Can not seek in lumps!");
	eptr = buffer;
	bptr = buffer;
	return doom_lseek(ffd, offs, SEEK_SET) < 0;
}

uint32_t reader_get(void *ptr, uint32_t size)
{
	int32_t tmp;
	uint32_t avail = eptr - bptr;

	if(size > BUFFER_SIZE)
		I_Error("[READER] Attempt to read more than %uB!", BUFFER_SIZE);

	if(avail >= size)
	{
		memcpy(ptr, bptr, size);
		bptr += size;
		return 0;
	}

	memcpy(ptr, bptr, avail);
	ptr += avail;
	size -= avail;

	tmp = BUFFER_SIZE;

	if(lump_offset)
	{
		if(!lump_size)
			return 1;
		doom_lseek(ffd, lump_offset, SEEK_SET);
		if(lump_size < BUFFER_SIZE)
			tmp = lump_size;
		lump_size -= tmp;
		lump_offset += tmp;
	}

	tmp = doom_read(ffd, buffer, tmp);
	if(tmp < 0)
		return 1;

	bptr = buffer;
	eptr = buffer + tmp;

	memcpy(ptr, bptr, size);
	bptr += size;

	return 0;
}

uint32_t reader_get_u32(uint32_t *ptr)
{
	return reader_get(ptr, sizeof(uint32_t));
}

uint32_t reader_get_u16(uint16_t *ptr)
{
	return reader_get(ptr, sizeof(uint16_t));
}

uint32_t reader_get_wame(uint64_t *ptr)
{
	return reader_get(ptr, sizeof(uint64_t));
}

//
// wAPI

void writer_open(uint8_t *name)
{
	if(ffd >= 0)
		I_Error("[WRITER] Attempt to open second file!");

	ffd = doom_open_WR(name);
	if(ffd < 0)
		I_Error("[WRITER] Unable to create '%s'!", name);

	bptr = buffer;
}

void writer_close()
{
	if(ffd < 0)
		I_Error("[WRITER] Double close or something!");

	writer_flush();
	doom_close(ffd);

	ffd = -1;
}

//
// wAPI - buffering

void writer_flush()
{
	int32_t size, ret;

	if(ffd < 0)
		I_Error("[WRITER] File is not open!");

	if(bptr == buffer)
		return;

	size = bptr - buffer;
	ret = doom_write(ffd, buffer, size);
	if(ret != size)
		I_Error("[WRITER] Write failed!");

	bptr = buffer;
}

void writer_add(void *data, uint32_t size)
{
	// TODO: maybe partial writes and buffer fills?
	uint32_t left = BUFFER_SIZE - (bptr - buffer);

	if(ffd < 0)
		I_Error("[WRITER] File is not open!");

	if(size > BUFFER_SIZE)
		I_Error("[WRITER] Attempt to write more than %uB!", BUFFER_SIZE);

	if(size > left)
		writer_flush();

	memcpy(bptr, data, size);
	bptr += size;
}

void *writer_reserve(uint32_t size)
{
	uint32_t left = BUFFER_SIZE - (bptr - buffer);
	void *ret;

	if(ffd < 0)
		I_Error("[WRITER] File is not open!");

	if(size > BUFFER_SIZE)
		I_Error("[WRITER] Attempt to reserve more than %uB!", BUFFER_SIZE);

	if(size > left)
		writer_flush();

	ret = bptr;
	bptr += size;

	return ret;
}

void *writer_write(void *data, uint32_t size)
{
	uint32_t ret;

	writer_flush();

	ret = doom_write(ffd, data, size);
	if(ret != size)
		I_Error("[WRITER] Write failed!");
}

void writer_add_wame(uint64_t *wame)
{
	writer_add(wame, sizeof(uint64_t));
}

void writer_add_u32(uint32_t value)
{
	writer_add(&value, sizeof(uint32_t));
}

void writer_add_u16(uint16_t value)
{
	writer_add(&value, sizeof(uint16_t));
}

