// kgsws' ACE Engine
////

// writer

void writer_open(uint8_t *name);
void writer_close();

void writer_flush();
void writer_add(void *ptr, uint32_t size);
void *writer_reserve(uint32_t size);
void *writer_write(void *data, uint32_t size);
void writer_add_u32(uint32_t value);
void writer_add_u16(uint16_t value);
void writer_add_wame(uint64_t *wame);

// reader

void reader_open_lump(int32_t lump);
void reader_open(uint8_t *name);
void reader_close();

uint32_t reader_seek(uint32_t offs);
uint32_t reader_get(void *ptr, uint32_t size);
uint32_t reader_get_u32(uint32_t *ptr);
uint32_t reader_get_u16(uint16_t *ptr);
uint32_t reader_get_wame(uint64_t *ptr);

