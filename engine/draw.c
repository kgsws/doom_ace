// kgsws' ACE Engine
////
// Linear drawing. Replaces unchained mode.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "draw.h"

uint8_t *dr_tinttab;
uint8_t ds_maskcolor;

static const uint8_t unknown_flat[4] = {96, 111, 111, 96};

static int16_t fuzzoffset[FUZZTABLE] =
{
	SCREENWIDTH,-SCREENWIDTH,SCREENWIDTH,-SCREENWIDTH,SCREENWIDTH,SCREENWIDTH,-SCREENWIDTH,
	SCREENWIDTH,SCREENWIDTH,-SCREENWIDTH,SCREENWIDTH,SCREENWIDTH,SCREENWIDTH,-SCREENWIDTH,
	SCREENWIDTH,SCREENWIDTH,SCREENWIDTH,-SCREENWIDTH,-SCREENWIDTH,-SCREENWIDTH,-SCREENWIDTH,
	SCREENWIDTH,-SCREENWIDTH,-SCREENWIDTH,SCREENWIDTH,SCREENWIDTH,SCREENWIDTH,SCREENWIDTH,-SCREENWIDTH,
	SCREENWIDTH,-SCREENWIDTH,SCREENWIDTH,SCREENWIDTH,-SCREENWIDTH,-SCREENWIDTH,SCREENWIDTH,
	SCREENWIDTH,-SCREENWIDTH,-SCREENWIDTH,-SCREENWIDTH,-SCREENWIDTH,SCREENWIDTH,SCREENWIDTH,
	SCREENWIDTH,SCREENWIDTH,-SCREENWIDTH,SCREENWIDTH,SCREENWIDTH,-SCREENWIDTH,SCREENWIDTH 
};

//
// column drawers

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawColumn()
{
	int32_t count;
	uint8_t *dest;
	fixed_t frac;

	count = dc_yh - dc_yl;

	if(count < 0)
		return;

	dest = screen_buffer + dc_yl * SCREENWIDTH + dc_x;
	frac = dc_texturemid + (dc_yl - centery) * dc_iscale;

	do
	{
		*dest = dc_colormap[dc_source[(frac >> FRACBITS)&127]];
		dest += SCREENWIDTH;
		frac += dc_iscale;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawColumnTint0()
{
	int32_t count;
	uint8_t *dest;
	fixed_t frac;

	count = dc_yh - dc_yl;

	if(count < 0)
		return;

	dest = screen_buffer + dc_yl * SCREENWIDTH + dc_x;
	frac = dc_texturemid + (dc_yl - centery) * dc_iscale;

	do
	{
		uint8_t color = dc_colormap[dc_source[(frac >> FRACBITS)&127]];;
		*dest = dr_tinttab[*dest + color * 256];
		dest += SCREENWIDTH;
		frac += dc_iscale;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawColumnTint1()
{
	int32_t count;
	uint8_t *dest;
	fixed_t frac;

	count = dc_yh - dc_yl;

	if(count < 0)
		return;

	dest = screen_buffer + dc_yl * SCREENWIDTH + dc_x;
	frac = dc_texturemid + (dc_yl - centery) * dc_iscale;

	do
	{
		uint8_t color = dc_colormap[dc_source[(frac >> FRACBITS)&127]];;
		*dest = dr_tinttab[*dest * 256 + color];
		dest += SCREENWIDTH;
		frac += dc_iscale;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawShadowColumn()
{
	int32_t count;
	uint8_t *dest;
	fixed_t frac;

	count = dc_yh - dc_yl;

	if(count < 0)
		return;

	dest = screen_buffer + dc_yl * SCREENWIDTH + dc_x;
	frac = dc_texturemid + (dc_yl - centery) * dc_iscale;

	do
	{
		*dest = colormaps[15 * 256 + *dest];
		dest += SCREENWIDTH;
		frac += dc_iscale;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawFuzzColumn()
{
	int32_t count;
	uint8_t *dest;

	if(!dc_yl)
		dc_yl = 1;

	if(dc_yh == SCREENHEIGHT - 1)
		dc_yh = SCREENHEIGHT - 2;

	count = dc_yh - dc_yl;

	if(count < 0)
		return;

	dest = screen_buffer + dc_yl * SCREENWIDTH + dc_x;

	do
	{
		*dest = colormaps[6*256 + dest[fuzzoffset[fuzzpos]]];

		if(++fuzzpos >= FUZZTABLE)
			fuzzpos = 0;

		dest += SCREENWIDTH;
	} while(count--);
}

//
// span drawers

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawSpan()
{
	uint32_t position, step;
	uint8_t *dest;
	uint32_t count;

	position = ((ds_xfrac << 10) & 0xffff0000) | ((ds_yfrac >> 6)  & 0x0000ffff);
	step = ((ds_xstep << 10) & 0xffff0000) | ((ds_ystep >> 6)  & 0x0000ffff);

	dest = screen_buffer + ds_y * SCREENWIDTH + ds_x1;
	count = ds_x2 - ds_x1;

	do
	{
		uint32_t xtemp, ytemp;
		ytemp = (position >> 4) & 0x0fc0;
		xtemp = (position >> 26);
		*dest++ = ds_colormap[ds_source[xtemp | ytemp]];
		position += step;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawUnknownSpan()
{
	uint32_t position, step;
	uint8_t *dest;
	uint32_t count;

	position = ((ds_xfrac << 10) & 0xffff0000) | ((ds_yfrac >> 6)  & 0x0000ffff);
	step = ((ds_xstep << 10) & 0xffff0000) | ((ds_ystep >> 6)  & 0x0000ffff);

	dest = screen_buffer + ds_y * SCREENWIDTH + ds_x1;
	count = ds_x2 - ds_x1;

	do
	{
		uint32_t xtemp, ytemp;
		ytemp = (position >> 12) & 2;
		xtemp = (position >> 29) & 1;
		*dest++ = ds_colormap[unknown_flat[xtemp | ytemp]];
		position += step;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawSpanTint0()
{
	uint32_t position, step;
	uint8_t *dest;
	uint32_t count;

	position = ((ds_xfrac << 10) & 0xffff0000) | ((ds_yfrac >> 6)  & 0x0000ffff);
	step = ((ds_xstep << 10) & 0xffff0000) | ((ds_ystep >> 6)  & 0x0000ffff);

	dest = screen_buffer + ds_y * SCREENWIDTH + ds_x1;
	count = ds_x2 - ds_x1;

	do
	{
		uint32_t xtemp, ytemp;
		uint8_t color;
		ytemp = (position >> 4) & 0x0fc0;
		xtemp = (position >> 26);
		color = ds_colormap[ds_source[xtemp | ytemp]];
		*dest = dr_tinttab[*dest + color * 256];
		dest++;
		position += step;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawSpanTint1()
{
	uint32_t position, step;
	uint8_t *dest;
	uint32_t count;

	position = ((ds_xfrac << 10) & 0xffff0000) | ((ds_yfrac >> 6)  & 0x0000ffff);
	step = ((ds_xstep << 10) & 0xffff0000) | ((ds_ystep >> 6)  & 0x0000ffff);

	dest = screen_buffer + ds_y * SCREENWIDTH + ds_x1;
	count = ds_x2 - ds_x1;

	do
	{
		uint32_t xtemp, ytemp;
		uint8_t color;
		ytemp = (position >> 4) & 0x0fc0;
		xtemp = (position >> 26);
		color = ds_colormap[ds_source[xtemp | ytemp]];
		*dest = dr_tinttab[*dest * 256 + color];
		dest++;
		position += step;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawMaskedSpan()
{
	uint32_t position, step;
	uint8_t *dest;
	uint32_t count;

	position = ((ds_xfrac << 10) & 0xffff0000) | ((ds_yfrac >> 6)  & 0x0000ffff);
	step = ((ds_xstep << 10) & 0xffff0000) | ((ds_ystep >> 6)  & 0x0000ffff);

	dest = screen_buffer + ds_y * SCREENWIDTH + ds_x1;
	count = ds_x2 - ds_x1;

	do
	{
		uint32_t xtemp, ytemp;
		uint8_t color;
		ytemp = (position >> 4) & 0x0fc0;
		xtemp = (position >> 26);
		color = ds_source[xtemp | ytemp];
		if(color != ds_maskcolor)
			*dest = ds_colormap[color];
		dest++;
		position += step;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawMaskedSpanTint0()
{
	uint32_t position, step;
	uint8_t *dest;
	uint32_t count;

	position = ((ds_xfrac << 10) & 0xffff0000) | ((ds_yfrac >> 6)  & 0x0000ffff);
	step = ((ds_xstep << 10) & 0xffff0000) | ((ds_ystep >> 6)  & 0x0000ffff);

	dest = screen_buffer + ds_y * SCREENWIDTH + ds_x1;
	count = ds_x2 - ds_x1;

	do
	{
		uint32_t xtemp, ytemp;
		uint8_t color;
		ytemp = (position >> 4) & 0x0fc0;
		xtemp = (position >> 26);
		color = ds_source[xtemp | ytemp];
		if(color != ds_maskcolor)
		{
			color = ds_colormap[color];
			*dest = dr_tinttab[*dest + color * 256];
		}
		dest++;
		position += step;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawMaskedSpanTint1()
{
	uint32_t position, step;
	uint8_t *dest;
	uint32_t count;

	position = ((ds_xfrac << 10) & 0xffff0000) | ((ds_yfrac >> 6)  & 0x0000ffff);
	step = ((ds_xstep << 10) & 0xffff0000) | ((ds_ystep >> 6)  & 0x0000ffff);

	dest = screen_buffer + ds_y * SCREENWIDTH + ds_x1;
	count = ds_x2 - ds_x1;

	do
	{
		uint32_t xtemp, ytemp;
		uint8_t color;
		ytemp = (position >> 4) & 0x0fc0;
		xtemp = (position >> 26);
		color = ds_source[xtemp | ytemp];
		if(color != ds_maskcolor)
		{
			color = ds_colormap[color];
			*dest = dr_tinttab[*dest * 256 + color];
		}
		dest++;
		position += step;
	} while(count--);
}

//
// patch drawers

__attribute((regparm(3),no_caller_saved_registers)) // three!
void V_DrawPatchDirect(int32_t x, int32_t y, patch_t *patch)
{
	uint32_t col;
	column_t *column;
	uint8_t *desttop;
	uint32_t w;

	x -= patch->x;
	y -= patch->y;

	if (x < 0 || x + patch->width > SCREENWIDTH || y < 0 || y + patch->height > SCREENHEIGHT)
		return;

	col = 0;
	desttop = screen_buffer + y * SCREENWIDTH + x;

	w = patch->width;

	for( ; col < w ; x++, col++, desttop++)
	{
		column = (column_t *)((uint8_t*)patch + patch->offs[col]);

		while(column->topdelta != 0xFF)
		{
			uint32_t count;
			uint8_t *source;
			uint8_t *dest;

			source = (uint8_t*)column + 3;
			dest = desttop + column->topdelta * SCREENWIDTH;
			count = column->length;

			while(count--)
			{
				*dest = *source++;
				dest += SCREENWIDTH;
			}

			column = (column_t *)((uint8_t*)column + column->length + 4);
		}
	}
}

//
// init

void init_draw()
{
	// I_FinishUpdate always copies the entire screen[0]; TODO: optimize
	*((uint8_t**)((void*)I_FinishUpdate + 4)) = screen_buffer;
}

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// disable unchained mode
	{0x0001A06C, CODE_HOOK | HOOK_JMP_DOOM, 0x0001A0EB},
	// disable 'I_UpdateNoBlit'
	{0x0001D31D, CODE_HOOK | HOOK_SET_NOPS, 5},
	{0x0001D4F2, CODE_HOOK | HOOK_SET_NOPS, 5},
	// replace column drawers
	{0x00035B49, CODE_HOOK | HOOK_UINT32, (uint32_t)R_DrawColumn},
	{0x00035B4E, CODE_HOOK | HOOK_UINT32, (uint32_t)R_DrawFuzzColumn},
	// replace span drawers
	{0x00035B58, CODE_HOOK | HOOK_UINT32, (uint32_t)R_DrawSpan},
	// replace patch drawers
	{0x000392A0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)V_DrawPatchDirect},
	// replace 'I_FinishUpdate'
	{0x0001D4A6, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)I_FinishUpdate},
	{0x0001D4FC, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)I_FinishUpdate},
};

