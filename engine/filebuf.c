// kgsws' ACE Engine
////
// File writer and reader with buffering.
#include "sdk.h"
#include "filebuf.h"

#define BUFFER_SIZE	(8*1024)

// reuse relocation space
extern uint8_t _reloc_start[];

static uint8_t *bptr;
static uint8_t *eptr;
static int ffd = -1;

//
// rAPI

void reader_open(uint8_t *name)
{
	int32_t tmp;

	if(ffd >= 0)
		I_Error("[READER] Attempt to open second file!");

	ffd = doom_open_RD(name);
	if(ffd < 0)
		I_Error("[READER] Unable to open '%s'!", name);

	eptr = _reloc_start;
	bptr = _reloc_start;
}

void reader_close()
{
	if(ffd < 0)
		I_Error("[READER] Double close or something!");

	doom_close(ffd);

	ffd = -1;
}

//
// rAPI - buffering

uint32_t reader_seek(uint32_t offs)
{
	eptr = _reloc_start;
	bptr = _reloc_start;
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

	tmp = doom_read(ffd, _reloc_start, BUFFER_SIZE);
	if(tmp < 0)
		I_Error("[READER] Read failed!");

	bptr = _reloc_start;
	eptr = _reloc_start + tmp;

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

	bptr = _reloc_start;
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

	if(bptr == _reloc_start)
		return;

	size = bptr - _reloc_start;
	ret = doom_write(ffd, _reloc_start, size);
	if(ret != size)
		I_Error("[WRITER] Write failed!");

	bptr = _reloc_start;
}

void writer_add(void *data, uint32_t size)
{
	// TODO: maybe partial writes and buffer fills?
	uint32_t left = BUFFER_SIZE - (bptr - _reloc_start);

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
	uint32_t left = BUFFER_SIZE - (bptr - _reloc_start);
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

