// kgsws' ACE Engine
////
// Text parsing for scripts.
// Text parser offers various ways to parse text from memory.
// Memory used for text is in 'screens[]' so it is limited. Though the limit is quite high.
// This parser is DESCTRUCTIVE. Parsed script will be modified in memory.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "textpars.h"

uint8_t *tp_text_ptr;

//
// API

uint8_t *tp_get_keyword()
{
	// This function returns whitespace-separated words (and numbers).
	uint8_t *ret;

	// end check
	if(!*tp_text_ptr)
		return NULL;

	// skip whitespaces and newlines
	while(*tp_text_ptr == ' ' || *tp_text_ptr == '\t' || *tp_text_ptr == '\r' || *tp_text_ptr == '\n')
		tp_text_ptr++;

	// this is the keyword
	ret = tp_text_ptr;

	// skip the text
	while(*tp_text_ptr && *tp_text_ptr != ' ' && *tp_text_ptr != '\t' && *tp_text_ptr != '\r' && *tp_text_ptr != '\n')
		tp_text_ptr++;

	// terminate string
	if(*tp_text_ptr)
		*tp_text_ptr++ = 0;

	// return the keyword
	return ret;
}

void tp_load_lump(lumpinfo_t *li)
{
	if(li->size > TP_MEMORY_SIZE)
		I_Error("TextParser: '%.8s' is too large! Limit is %u but size is %u.\n", TP_MEMORY_SIZE, li->size);

	tp_text_ptr = TP_MEMORY_ADDR;
	tp_text_ptr[li->size] = 0;

	wad_read_lump(tp_text_ptr, li - *lumpinfo, TP_MEMORY_SIZE);
}

