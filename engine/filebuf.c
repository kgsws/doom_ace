// kgsws' ACE Engine
////
// File writer and reader with buffering.
#include "sdk.h"
#include "utils.h"
#include "engine.h"
#include "wadfile.h"
#include "filebuf.h"

#define BUFFER_SIZE	(8*1024)

uint8_t file_buffer[BUFFER_SIZE];
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
		engine_error("READER", "Attempt to open second file!");

	ffd = lumpinfo[lump].fd;
	lump_offset = lumpinfo[lump].offset;
	lump_size = lumpinfo[lump].size;

	eptr = file_buffer;
	bptr = file_buffer;
}

void reader_open(uint8_t *name)
{
	int32_t tmp;

	if(ffd >= 0)
		engine_error("READER", "Attempt to open second file!");

	ffd = doom_open_RD(name);
	if(ffd < 0)
		engine_error("READER", "Unable to open '%s'!", name);

	lump_offset = 0;

	eptr = file_buffer;
	bptr = file_buffer;
}

void reader_close()
{
	if(ffd < 0)
		engine_error("READER", "Double close or something!");

	if(!lump_offset)
		doom_close(ffd);

	ffd = -1;
}

//
// rAPI - buffering

uint32_t reader_seek(uint32_t offs)
{
	if(lump_offset)
		engine_error("READER", "Can not seek in lumps!");
	eptr = file_buffer;
	bptr = file_buffer;
	return doom_lseek(ffd, offs, SEEK_SET) < 0;
}

uint32_t reader_get(void *ptr, uint32_t size)
{
	int32_t tmp;
	uint32_t avail = eptr - bptr;

	if(size > BUFFER_SIZE)
		engine_error("READER", "Attempt to read more than %uB!", BUFFER_SIZE);

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

	tmp = doom_read(ffd, file_buffer, tmp);
	if(tmp < 0)
		return 1;

	bptr = file_buffer;
	eptr = file_buffer + tmp;

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
		engine_error("READER", "Attempt to open second file!");

	ffd = doom_open_WR(name);
	if(ffd < 0)
		engine_error("READER", "Unable to create '%s'!", name);

	bptr = file_buffer;
}

void writer_close()
{
	if(ffd < 0)
		engine_error("WRITER", "Double close or something!");

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
		engine_error("WRITER", "File is not open!");

	if(bptr == file_buffer)
		return;

	size = bptr - file_buffer;
	ret = doom_write(ffd, file_buffer, size);
	if(ret != size)
		engine_error("WRITER", "Write failed!");

	bptr = file_buffer;
}

void writer_add(void *data, uint32_t size)
{
	// TODO: maybe partial writes and buffer fills?
	uint32_t left = BUFFER_SIZE - (bptr - file_buffer);

	if(ffd < 0)
		engine_error("WRITER", "File is not open!");

	if(size > BUFFER_SIZE)
		engine_error("WRITER", "Attempt to write more than %uB!", BUFFER_SIZE);

	if(size > left)
		writer_flush();

	memcpy(bptr, data, size);
	bptr += size;
}

void *writer_reserve(uint32_t size)
{
	uint32_t left = BUFFER_SIZE - (bptr - file_buffer);
	void *ret;

	if(ffd < 0)
		engine_error("WRITER", "File is not open!");

	if(size > BUFFER_SIZE)
		engine_error("WRITER", "Attempt to reserve more than %uB!", BUFFER_SIZE);

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
		engine_error("WRITER", "Write failed!");
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

void writer_add_from_fd(int32_t fd, uint32_t size)
{
	writer_flush();

	while(size)
	{
		uint32_t len = size > BUFFER_SIZE ? BUFFER_SIZE : size;
		uint32_t ret;

		ret = doom_read(fd, file_buffer, len);
		if(ret != len)
			engine_error("WRITER", "Read failed!");

		ret = doom_write(ffd, file_buffer, len);
		if(ret != len)
			engine_error("WRITER", "Write failed!");

		size -= len;
	}
}

