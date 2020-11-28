// kgsws' Doom ACE
// Text script parsing helper.
#include "engine.h"
#include "defs.h"
#include "textpars.h"

int tp_nl_is_ws; // newline is whitespace
int tp_kw_is_func; // keyword search in function arguments
uint8_t *tp_func_name_end; // for 'tp_get_function'
uint32_t tp_string_len; // length after 'tp_clean_string'

// cleanup already valid string
// destructive!
uint8_t *tp_clean_string(uint8_t *str)
{
	int escaped = 0;
	uint8_t *src = str + 1;
	uint8_t *dst = str;

	tp_string_len = 0;

	while(1)
	{
		register uint8_t tmp = *src++;

		if(escaped)
			escaped = 0;
		else
		if(tmp == '"')
			break;
		else
		if(tmp == '\\')
		{
			escaped = 1;
			continue;
		}

		tp_string_len++;
		*dst++ = tmp;
	}
	*dst = 0;
	return str;
}

// skip entire section, full feature set (strings and escaping)
// section in section is supported, but not different section types
uint8_t *tp_skip_section_full(uint8_t *start, uint8_t *end, uint8_t *mark, uint8_t **err)
{
	uint32_t depth = 1;
	uint32_t in_str = 0;
	uint32_t escaped = 0;
	uint32_t in_comment = 0;

	if(*start != mark[0])
	{
		*err = "section start expected";
		return NULL;
	}
	start++;

	while(start < end)
	{
		register uint8_t tmp = *start;

		if(in_comment == 1)
		{
			if(tmp == '\n' || tmp == '\r')
				in_comment = 0;
		} else
		if(in_comment == 2)
		{
			if(tmp == '*')
			{
				start++;
				if(start != end)
				{
					tmp = *start;
					if(tmp == '/')
						in_comment = 0;
				} else
					start--;
			}
		} else
		if(tmp == '"')
		{
			if(!escaped)
				in_str ^= 1;
		} else
		{
			if(in_str)
			{
				if(!escaped)
				{
					if(tmp == '\n' || tmp == '\r')
						break;
					if(tmp == '\\')
					{
						escaped = 1;
						goto skip_escape;
					}
				}
			} else
			{
				if(tmp == '/')
				{
					start++;
					if(start != end)
					{
						tmp = *start;
						if(tmp == '/')
						{
							in_comment = 1;
						} else
						if(tmp == '*')
						{
							in_comment = 2;
						} else
							start--;
					} else
						start--;
				} else
				if(tmp == mark[0])
				{
					depth++;
				} else
				if(tmp == mark[1])
				{
					depth--;
					if(!depth)
						return start + 1;
				}
			}
		}

		escaped = 0;
skip_escape:
		start++;
	}
	// failed
	if(in_str)
		*err = "unclosed string";
	else
	if(depth)
		*err = "missing section end";
	return NULL;
}

// get unsigned number
// returns skipped number
uint8_t *tp_get_uint(uint8_t *start, uint8_t *end, uint32_t *val)
{
	uint32_t num = 0;
	uint8_t *ptr = start;

	while(ptr < end)
	{
		register uint8_t tmp = *ptr;
		if(tmp > '9')
			break;
		if(tmp < '0')
			break;
		num *= 10;
		num += tmp - '0';
		ptr++;
	}
	if(ptr == start)
		return NULL;
	*val = num;
	return ptr;
}

// get fixed_t number
// returns skipped number
uint8_t *tp_get_fixed(uint8_t *start, uint8_t *end, fixed_t *val)
{
	int is_negative;
	uint32_t num;
	fixed_t ret;
	uint8_t *ptr = start;

	if(*ptr == '-')
	{
		ptr++;
		if(ptr == end)
			return NULL;
		is_negative = 1;
	} else
		is_negative = 0;

	// integer part
	while(ptr < end)
	{
		register uint8_t tmp = *ptr;
		if(tmp > '9')
			break;
		if(tmp < '0')
			break;
		num *= 10;
		num += tmp - '0';
		ptr++;
		if(ptr == end)
			return NULL;
	}
	if(ptr == start)
		return NULL;
	ret = num << FRACBITS;

	// decimal part (optional)
	if(*ptr == '.')
	{
		uint8_t *ppp;
		ptr++;
		if(ptr == end)
			return NULL;
		num = 0;
		start = ptr;
		while(ptr < end)
		{
			register uint8_t tmp = *ptr;
			if(tmp > '9')
				break;
			if(tmp < '0')
				break;
			ptr++;
		}

		ppp = ptr;
		if(ppp < end)
		while(ppp > start)
		{
			ppp--;
			num += (*ppp - '0') << FRACBITS;
			num /= 10;
		}

		ret |= num;
	}

	if(is_negative)
		ret = -ret;

	*val = ret;
	return ptr;
}

// non-case compare and skip
// returns skipped keyword
// assuming 'templ' is lowercase;
uint8_t *tp_ncompare_skip(uint8_t *src, uint8_t *eof, uint8_t *templ)
{
	while(src < eof)
	{
		register uint8_t tmp = *src;

		if(!*templ)
		{
			if(tmp == 0)
				return src;
			if(tmp == ' ')
				return src;
			if(tmp == '\t')
				return src;
			if(tmp == '\n')
				return src;
			if(tmp == '\r')
				return src;
			if(tp_kw_is_func)
			{
				if(tmp == '|')
					return src;
				if(tmp == ',')
					return src;
				if(tmp == ')')
					return src;
			}
		}

		if(tmp >= 'A' && tmp <= 'Z')
			tmp |= 0x20;
		if(tmp != *templ)
			return NULL;

		src++;
		templ++;
	}
	return NULL;
}

// get function keyword (with arguments)
// returns skipped keyword
uint8_t *tp_get_function(uint8_t *start, uint8_t *end)
{
	int in_arg = 0;

	while(1)
	{
		register uint8_t tmp = *start;
		if(in_arg == 1)
		{
			if(tmp == ')')
				in_arg = 2;
		} else
		{
			if(tmp == ' ')
				break;
			if(tmp == '\t')
				break;
			if(tmp == ')')
				return NULL;
		}
		if(tmp == '\n')
			break;
		if(tmp == '\r')
			break;
		if(tmp == '(')
		{
			if(in_arg)
				return NULL;
			in_arg = 1;
			tp_func_name_end = start;
		}
		start++;
		if(start == end)
			return NULL;
	}
	if(!in_arg)
		tp_func_name_end = start;
	if(in_arg == 1)
		return NULL;
	return start;
}

// get string (text in quotes)
// returns skipped string
uint8_t *tp_get_string(uint8_t *start, uint8_t *end)
{
	int escaped = 0;

	if(*start != '"')
		return NULL;
	start++;

	while(start < end)
	{
		register uint8_t tmp = *start;

		if(!escaped)
		{
			if(tmp == '\n' || tmp == '\r')
				break;
			if(tmp == '"')
			{
				start++;
				if(start == end)
					break;
				return start;
			}
			if(tmp == '\\')
				escaped = 1;
		} else
			escaped = 0;

		start++;
		if(start == end)
			return NULL;
	}
	return NULL;
}

// get keyword (text until whitespace)
// returns skipped keyword
uint8_t *tp_get_keyword(uint8_t *start, uint8_t *end)
{
	while(1)
	{
		register uint8_t tmp = *start;
		if(tmp == ' ')
			break;
		if(tmp == '\t')
			break;
		if(tmp == '\n')
			break;
		if(tmp == '\r')
			break;
		start++;
		if(start == end)
			return NULL;
	}
	return start;
}

// check if whitespace or comment follows
int tp_is_ws_next(uint8_t *start, uint8_t *end, int allow_comments)
{
	register uint8_t tmp = *start;
	if(tmp == ' ')
		return 1;
	if(tmp == '\t')
		return 1;
	if(tp_nl_is_ws)
	{
		if(tmp == '\n')
			return 1;
		if(tmp == '\r')
			return 1;
	}
	if(allow_comments && tmp == '/')
	{
		start++;
		if(start == end)
			return 0;
		tmp = *start;
		if(tmp == '/')
			return 1;
		if(tmp == '*')
			return 1;
	}
	return 0;
}

// skip whitespaces and comments
uint8_t *tp_skip_wsc(uint8_t *start, uint8_t *end)
{
	int in_comment = 0;

	while(start != end)
	{
		register uint8_t tmp = *start;

		if(in_comment == 1)
		{
			if(tmp == '\n' || tmp == '\r')
			{
				in_comment = 0;
				if(tp_nl_is_ws)
					goto skip;
			} else
				goto skip;
		} else
		if(in_comment == 2)
		{
			if(tmp == '*')
			{
				start++;
				if(start != end)
				{
					if(*start == '/')
						in_comment = 0;
				}
			}
			goto skip;
		}

		if(tmp == ' ')
			goto skip;
		if(tmp == '\t')
			goto skip;
		if(tp_nl_is_ws)
		{
			if(tmp == '\n')
				goto skip;
			if(tmp == '\r')
				goto skip;
		}
		if(tmp == '/')
		{
			start++;
			if(start != end)
			{
				tmp = *start;
				if(tmp == '/')
				{
					in_comment = 1;
					goto skip;
				} else
				if(tmp == '*')
				{
					in_comment = 2;
					goto skip;
				}
			}
			start--;
		}

		return start;
skip:
		start++;
	}
	return start;
}

// skip whitespaces
uint8_t *tp_skip_ws(uint8_t *start, uint8_t *end)
{
	while(start != end)
	{
		if(*start == ' ')
			goto skip;
		if(*start == '\t')
			goto skip;
		if(tp_nl_is_ws)
		{
			if(*start == '\n')
				goto skip;
			if(*start == '\r')
				goto skip;
		}
		return start;
skip:
		start++;
	}
	return start;
}

// kinda strcmp, with case check
// t2 must be uppercase
uint8_t *tp_nc_compare(uint8_t *t1, uint8_t *t2)
{
	while(1)
	{
		register uint8_t tmp = *t1++;

		if(tmp >= 'A' && tmp <= 'Z')
			tmp |= 0x20;

		if(tmp != *t2++)
			return NULL;
		if(!tmp)
			break;
	}
	return t1;
}

