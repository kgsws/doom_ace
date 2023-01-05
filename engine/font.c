// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "render.h"
#include "font.h"

typedef struct
{
	uint32_t magic;
	uint8_t version;
	uint8_t line_height;
	int8_t size_over;
	int8_t size_under;
	int8_t space;
	int8_t size_inner;
	union
	{
		struct
		{
			uint8_t colors_used;
			uint8_t color_max;
		};
		uint16_t num_chars; // overwritten
	};
	uint32_t reserved; // overwritten with data offset
	uint8_t pal_count;
} __attribute__((packed)) bmf_head_t;

typedef struct
{
	uint8_t code;
	uint8_t width, height;
	int8_t ox, oy;
	int8_t space;
} __attribute__((packed)) bmf_char_t;

// original ZDoom color ranges
// values are sourced from https://c.eev.ee/doom-text-generator
static const uint8_t colortable[] =
{
	//// brick
	255,
	0x47, 0x00, 0x00, 0xFF, 0xB8, 0xB8,
	//// tan
	255,
	0x33, 0x2B, 0x13, 0xFF, 0xEB, 0xDF,
	//// gray
	255,
	0x27, 0x27, 0x27, 0xEF, 0xEF, 0xEF,
	//// green
	255,
	0x0B, 0x17, 0x07, 0x77, 0xFF, 0x6F,
	//// brown
	255,
	0x53, 0x3F, 0x2F, 0xBF, 0xA7, 0x8F,
	//// gold
	255,
	0x73, 0x2B, 0x00, 0xFF, 0xFF, 0x73,
	//// red
	255,
	0x3F, 0x00, 0x00, 0xFF, 0x00, 0x00,
	//// blue
	255,
	0x00, 0x00, 0x27, 0x00, 0x00, 0xFF,
	//// orange
	255,
	0x20, 0x00, 0x00, 0xFF, 0x80, 0x00,
	//// white
	255,
	0x24, 0x24, 0x24, 0xFF, 0xFF, 0xFF,
	//// yellow
	64,
	0x27, 0x27, 0x27, 0x51, 0x51, 0x51,
	207,
	0x78, 0x49, 0x18, 0xF3, 0xA7, 0x18,
	255,
	0xF3, 0xA8, 0x2A, 0xFC, 0xD0, 0x43,
	//// untranslated
	// this one is not generated
	//// black
	255,
	0x13, 0x13, 0x13, 0x50, 0x50, 0x50,
	//// lightblue
	255,
	0x00, 0x00, 0x73, 0xB4, 0xB4, 0xFF,
	//// cream
	255,
	0xCF, 0x83, 0x53, 0xFF, 0xD7, 0xBB,
	//// olive
	255,
	0x2F, 0x37, 0x1F, 0x7B, 0x7F, 0x50,
	//// darkgreen
	255,
	0x0B, 0x17, 0x07, 0x43, 0x93, 037,
	//// darkred
	255,
	0x2B, 0x00, 0x00, 0xAF, 0x2B, 0x2B,
	//// darkbrown
	255,
	0x1F, 0x17, 0x0B, 0xA3, 0x6B, 0x3F,
	//// purple
	255,
	0x23, 0x00, 0x23, 0xCF, 0x00, 0xCF,
	//// darkgray
	255,
	0x23, 0x23, 0x23, 0x8B, 0x8B, 0x8B,
	//// cyan
	255,
	0x00, 0x1F, 0x1F, 0x00, 0xF0, 0xF0,
	//// ice
	94,
	0x34, 0x34, 0x50, 0x7C, 0x7C, 0x98,
	255,
	0x7C, 0x7C, 0x98, 0xE0, 0xE0, 0xE0,
	//// fire
	104,
	0x66, 0x00, 0x00, 0xD5, 0x76, 0x04,
	255,
	0xD5, 0x76, 0x04, 0xFF, 0xFF, 0x00,
	//// sapphire
	94,
	0x00, 0x04, 0x68, 0x50, 0x6C, 0xFC,
	255,
	0x50, 0x6C, 0xFC, 0x50, 0xEC, 0xFC,
	//// teal
	90,
	0x00, 0x1F, 0x1F, 0x23, 0x67, 0x73,
	255,
	0x23, 0x67, 0x73, 0x7B, 0xB3, 0xC3,
};

//
// range generator

uint32_t scale_range(uint32_t value)
{
	// first color is transparent
	// other colors are range from darkest to brightest
	value *= (FONT_COLOR_COUNT-2);
	value /= 255;
	return 1 + value;
}

void generate_range(uint32_t first, uint32_t last, const uint8_t *src, uint8_t *dst)
{
	uint32_t count;
	fixed_t vr, vg, vb;
	fixed_t sr, sg, sb;

	first = scale_range(first);
	last = scale_range(last);

	count = last - first + 1;

	vr = (fixed_t)*src++ << 22;
	vg = (fixed_t)*src++ << 22;
	vb = (fixed_t)*src++ << 22;

	sr = (fixed_t)*src++ << 22;
	sg = (fixed_t)*src++ << 22;
	sb = (fixed_t)*src++ << 22;

	sr -= vr;
	sr /= count;

	sg -= vg;
	sg /= count;

	sb -= vb;
	sb /= count;

	for(uint32_t i = 0; i < count; i++)
	{
		dst[first + i] = r_find_color(vr >> 22, vg >> 22, vb >> 22);
		vr += sr;
		vg += sg;
		vb += sb;
	}
}

//
// drawing

static bmf_char_t *find_char(void *ptr, uint32_t count, uint8_t code)
{
	for(uint32_t i = 0; i < count; i++)
	{
		bmf_char_t *bc = (void*)ptr;
		if(bc->code == code)
			return bc;
		ptr += sizeof(bmf_char_t) + bc->width * bc->height;
	}
	return NULL;
}

static void font_draw_char(int32_t x, int32_t y, bmf_char_t *bc)
{
	uint8_t *src = (void*)(bc + 1);
	uint8_t *dst = screen_buffer;
	int32_t yy, xx, ss;

	x += bc->ox;
	y += bc->oy;

	if(x >= SCREENWIDTH)
		return;

	if(y >= SCREENHEIGHT)
		return;

	xx = x + bc->width;

	if(xx < 0)
		return;

	yy = y + bc->height;

	if(yy < 0)
		return;

	if(x < 0)
	{
		src -= x;
		x = 0;
	}
	if(xx > SCREENWIDTH)
		xx = SCREENWIDTH;

	if(y < 0)
	{
		src -= y * (int32_t)bc->width;
		y = 0;
	}
	if(yy > SCREENHEIGHT)
		yy = SCREENHEIGHT;

	dst += x;
	dst += y * SCREENWIDTH;

	xx -= x;
	ss = SCREENWIDTH - xx;

	for( ; y < yy; y++)
	{
		for(int32_t i = 0; i < xx; i++)
		{
			if(src[i])
				*dst = src[i];
			dst++;
		}
		src += bc->width;
		dst += ss;
	}
}

void font_center_text(const uint8_t *text, int32_t font, uint32_t linecount)
{
	int32_t x = SCREENWIDTH / 2;
	int32_t y = SCREENHEIGHT / 2;
	void *data = font_load(font);
	bmf_head_t *head = data;
	bmf_char_t *bc;

	linecount *= head->line_height;
	y -= linecount / 2;

	while(1)
	{
		const uint8_t *ptr;
		int32_t xx;

		// get width
		xx = 0;
		ptr = text;
		while(*ptr && *ptr != '\n')
		{
			bmf_char_t *bc;

			bc = find_char(data + head->reserved, head->num_chars, *ptr);
			if(bc)
				xx += head->space + bc->space;

			ptr++;
		}

		// draw centered
		xx = x - xx / 2;
		ptr = text;
		while(*ptr && *ptr != '\n')
		{
			bmf_char_t *bc;

			bc = find_char(data + head->reserved, head->num_chars, *ptr);
			if(bc)
			{
				font_draw_char(xx, y, bc);
				xx += head->space + bc->space;
			}

			ptr++;
		}

		// end?
		if(!*ptr)
			break;

		// next line
		text = ptr + 1;
		y += head->line_height;
	}
}

//
// API

void font_generate()
{
	const uint8_t *ptr = colortable;
	uint8_t *dst = render_tables->fmap;

	for(uint32_t i = 0; i < FONT_TRANSLATION_COUNT; i++)
	{
		uint8_t first = 0;

		while(1)
		{
			uint8_t last = (uint32_t)*ptr++;

			generate_range(first, last, ptr, dst);
			ptr += 6;

			if(last >= 255)
				break;

			first = last + 1;
		}

		dst += FONT_COLOR_COUNT;
	}
}

void *font_load(int32_t lump)
{
	// WARNING: There are no bound checks!
	void *data;
	uint8_t *ptr;
	bmf_head_t *head;

	if(lumpcache[lump])
		// font is already processed
		return lumpcache[lump];

	data = W_CacheLumpNum(lump, PU_CACHE);
	ptr = data;
	head = data;

	if(head->magic != FONT_MAGIC_ID || head->version != FONT_VERSION)
		engine_error("FONT", "Invalid font %.8s.", lumpinfo[lump].name);

	ptr += sizeof(bmf_head_t);
	ptr += 3 * head->pal_count;
	ptr += *ptr + 1;

	head->num_chars = *((uint16_t*)ptr);
	ptr += sizeof(uint16_t);

	head->reserved = (void*)ptr - data;

	return data;
}

