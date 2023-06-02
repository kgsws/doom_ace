// kgsws' ACE Engine
////
// Linear drawing. Replaces unchained mode.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "render.h"
#include "config.h"
#include "map.h"
#include "vesa.h"
#include "draw.h"

#define AMAP_Y_SCALE	55000

typedef struct
{
	fixed_t x, y;
} map_point_t;

typedef struct
{
	map_point_t a, b;
} map_line_t;

//

uint8_t *dr_tinttab;
uint8_t ds_maskcolor;

uint8_t *draw_patch_color;

static uint8_t am_forced_color;
static fixed_t mhax_y, mhax_h;

static const uint8_t unknown_flat[4] = {96, 111, 111, 96};

static const uint8_t doom_lock_id[] =
{
	26, 130,
	27, 131,
	28, 129,
	32, 130,
	33, 129,
	34, 131,
	99, 130,
	133, 130,
	134, 129,
	135, 129,
	136, 131,
	137, 131,
	// terminator
	0
};

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
void R_DrawColumnTint0()
{
	int32_t count;
	uint8_t *dest;
	uint32_t frac;
	uint32_t step;

	count = dc_yh - dc_yl;

	if(count < 0)
		return;

	dest = ylookup[dc_yl] + columnofs[dc_x] + vesa_offset;
	frac = dc_texturemid + (dc_yl - centery) * dc_iscale;
	frac <<= r_dc_mask.u8[0];
	step = dc_iscale << r_dc_mask.u8[0];

	do
	{
		uint8_t color = dc_colormap[dc_source[frac >> r_dc_mask.u8[2]]];
		*dest = dr_tinttab[*dest + color * 256];
		dest += SCREENWIDTH;
		frac += step;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawColumnTint1()
{
	int32_t count;
	uint8_t *dest;
	uint32_t frac;
	uint32_t step;

	count = dc_yh - dc_yl;

	if(count < 0)
		return;

	dest = ylookup[dc_yl] + columnofs[dc_x] + vesa_offset;
	frac = dc_texturemid + (dc_yl - centery) * dc_iscale;
	frac <<= r_dc_mask.u8[0];
	step = dc_iscale << r_dc_mask.u8[0];

	do
	{
		uint8_t color = dc_colormap[dc_source[frac >> r_dc_mask.u8[2]]];
		*dest = dr_tinttab[*dest * 256 + color];
		dest += SCREENWIDTH;
		frac += step;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawShadowColumn()
{
	int32_t count;
	uint8_t *dest;

	count = dc_yh - dc_yl;

	if(count < 0)
		return;

	dest = ylookup[dc_yl] + columnofs[dc_x] + vesa_offset;

	do
	{
		*dest = dc_colormap[*dest];
		dest += SCREENWIDTH;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawFuzzColumn()
{
	int32_t count;
	uint8_t *dest;

	if(!dc_yl)
		dc_yl = 1;

	if(dc_yh > viewheight - 2)
		dc_yh = viewheight - 2;

	count = dc_yh - dc_yl;

	if(count < 0)
		return;

	dest = ylookup[dc_yl] + columnofs[dc_x] + vesa_offset;

	do
	{
		*dest = colormaps[6*256 + dest[fuzzoffset[fuzzpos]]];

		if(++fuzzpos >= FUZZTABLE)
			fuzzpos = 0;

		dest += SCREENWIDTH;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawTranslatedColumn()
{
	int32_t count;
	uint8_t *dest;
	uint32_t frac;
	uint32_t step;

	count = dc_yh - dc_yl;

	if(count < 0)
		return;

	dest = ylookup[dc_yl] + columnofs[dc_x] + vesa_offset;
	frac = dc_texturemid + (dc_yl - centery) * dc_iscale;
	frac <<= r_dc_mask.u8[0];
	step = dc_iscale << r_dc_mask.u8[0];

	do
	{
		*dest = dc_colormap[dc_translation[dc_source[frac >> r_dc_mask.u8[2]]]];
		dest += SCREENWIDTH;
		frac += step;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawTranslatedColumnTint0()
{
	int32_t count;
	uint8_t *dest;
	uint32_t frac;
	uint32_t step;

	count = dc_yh - dc_yl;

	if(count < 0)
		return;

	dest = ylookup[dc_yl] + columnofs[dc_x] + vesa_offset;
	frac = dc_texturemid + (dc_yl - centery) * dc_iscale;
	frac <<= r_dc_mask.u8[0];
	step = dc_iscale << r_dc_mask.u8[0];

	do
	{
		uint8_t color = dc_colormap[dc_translation[dc_source[frac >> r_dc_mask.u8[2]]]];
		*dest = dr_tinttab[*dest + color * 256];
		dest += SCREENWIDTH;
		frac += step;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawTranslatedColumnTint1()
{
	int32_t count;
	uint8_t *dest;
	uint32_t frac;
	uint32_t step;

	count = dc_yh - dc_yl;

	if(count < 0)
		return;

	dest = ylookup[dc_yl] + columnofs[dc_x] + vesa_offset;
	frac = dc_texturemid + (dc_yl - centery) * dc_iscale;
	frac <<= r_dc_mask.u8[0];
	step = dc_iscale << r_dc_mask.u8[0];

	do
	{
		uint8_t color = dc_colormap[dc_translation[dc_source[frac >> r_dc_mask.u8[2]]]];
		*dest = dr_tinttab[*dest * 256 + color];
		dest += SCREENWIDTH;
		frac += step;
	} while(count--);
}

//
// span drawers

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawUnknownSpan()
{
	uint32_t position, step;
	uint8_t *dest;
	uint32_t count;
	uint32_t px, py;

	dest = ylookup[ds_y] + columnofs[ds_x1] + vesa_offset;
	count = ds_x2 - ds_x1;

	px = ds_xfrac;
	py = ds_yfrac;

	do
	{
		uint32_t position;
		position = (px >> 18) & 2;
		position |= (py >> 19) & 1;
		*dest++ = ds_colormap[unknown_flat[position]];
		px += ds_xstep;
		py += ds_ystep;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawSpanTint0()
{
	uint8_t *dest;
	uint32_t count;
	uint32_t px, py;

	dest = ylookup[ds_y] + columnofs[ds_x1] + vesa_offset;
	count = ds_x2 - ds_x1;

	px = ds_xfrac;
	py = ds_yfrac;

	do
	{
		uint8_t color;
		uint32_t position;
		position = (py >> 10) & 0x0FC0;
		position |= (px >> 16) & 0x3F;
		color = ds_colormap[ds_source[position]];
		*dest = dr_tinttab[*dest + color * 256];
		dest++;
		px += ds_xstep;
		py += ds_ystep;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawSpanTint1()
{
	uint8_t *dest;
	uint32_t count;
	uint32_t px, py;

	dest = ylookup[ds_y] + columnofs[ds_x1] + vesa_offset;
	count = ds_x2 - ds_x1;

	px = ds_xfrac;
	py = ds_yfrac;

	do
	{
		uint8_t color;
		uint32_t position;
		position = (py >> 10) & 0x0FC0;
		position |= (px >> 16) & 0x3F;
		color = ds_colormap[ds_source[position]];
		*dest = dr_tinttab[*dest * 256 + color];
		dest++;
		px += ds_xstep;
		py += ds_ystep;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawMaskedSpan()
{
	uint8_t *dest;
	uint32_t count;
	uint32_t px, py;

	dest = ylookup[ds_y] + columnofs[ds_x1] + vesa_offset;
	count = ds_x2 - ds_x1;

	px = ds_xfrac;
	py = ds_yfrac;

	do
	{
		uint8_t color;
		uint32_t position;
		position = (py >> 10) & 0x0FC0;
		position |= (px >> 16) & 0x3F;
		color = ds_source[position];
		if(color != ds_maskcolor)
			*dest = ds_colormap[color];
		dest++;
		px += ds_xstep;
		py += ds_ystep;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawMaskedSpanTint0()
{
	uint8_t *dest;
	uint32_t count;
	uint32_t px, py;

	dest = ylookup[ds_y] + columnofs[ds_x1] + vesa_offset;
	count = ds_x2 - ds_x1;

	px = ds_xfrac;
	py = ds_yfrac;

	do
	{
		uint8_t color;
		uint32_t position;
		position = (py >> 10) & 0x0FC0;
		position |= (px >> 16) & 0x3F;
		color = ds_source[position];
		if(color != ds_maskcolor)
		{
			color = ds_colormap[color];
			*dest = dr_tinttab[*dest + color * 256];
		}
		dest++;
		px += ds_xstep;
		py += ds_ystep;
	} while(count--);
}

__attribute((regparm(2),no_caller_saved_registers))
void R_DrawMaskedSpanTint1()
{
	uint8_t *dest;
	uint32_t count;
	uint32_t px, py;

	dest = ylookup[ds_y] + columnofs[ds_x1] + vesa_offset;
	count = ds_x2 - ds_x1;

	px = ds_xfrac;
	py = ds_yfrac;

	do
	{
		uint8_t color;
		uint32_t position;
		position = (py >> 10) & 0x0FC0;
		position |= (px >> 16) & 0x3F;
		color = ds_source[position];
		if(color != ds_maskcolor)
		{
			color = ds_colormap[color];
			*dest = dr_tinttab[*dest * 256 + color];
		}
		dest++;
		px += ds_xstep;
		py += ds_ystep;
	} while(count--);
}

//
// patch drawers

__attribute((regparm(3),no_caller_saved_registers)) // three!
void V_DrawPatchDirect(int32_t x, int32_t y, patch_t *patch)
{
	draw_patch_to_memory(patch, x - patch->x, y - patch->y, framebuffer, SCREENWIDTH, SCREENHEIGHT);
}

__attribute((regparm(3),no_caller_saved_registers)) // three!
void V_DrawPatchTranslated(int32_t x, int32_t y, patch_t *patch)
{
	int32_t xstop;
	column_t *column;
	uint8_t *desttop;
	uint32_t *coloffs;

	x -= patch->x;
	y -= patch->y;

	if(x >= SCREENWIDTH)
		return;

	if(y >= SCREENHEIGHT)
		return;

	xstop = x + patch->width;

	if(xstop <= 0)
		return;

	if(xstop > SCREENWIDTH)
		xstop = SCREENWIDTH;

	coloffs = patch->offs - x;

	if(x < 0)
		x = 0;

	desttop = framebuffer + x;

	for( ; x < xstop; x++, desttop++)
	{
		column = (column_t *)((uint8_t*)patch + coloffs[x]);

		while(column->topdelta != 0xFF)
		{
			uint8_t *source;
			uint8_t *dest;
			int32_t yy, ystop;

			source = (uint8_t*)column + 3;

			yy = y + column->topdelta;
			ystop = yy + column->length;

			column = (column_t *)((uint8_t*)column + column->length + 4);

			if(yy < 0)
			{
				source -= yy;
				yy = 0;
			}

			if(ystop > SCREENHEIGHT)
				ystop = SCREENHEIGHT;

			dest = desttop + yy * SCREENWIDTH;

			for( ; yy < ystop; yy++)
			{
				*dest = dc_translation[*source++];
				dest += SCREENWIDTH;
			}
		}
	}
}

__attribute((regparm(3),no_caller_saved_registers)) // three!
void V_DrawPatchTint0(int32_t x, int32_t y, patch_t *patch)
{
	int32_t xstop;
	column_t *column;
	uint8_t *desttop;
	uint32_t *coloffs;

	x -= patch->x;
	y -= patch->y;

	if(x >= SCREENWIDTH)
		return;

	if(y >= SCREENHEIGHT)
		return;

	xstop = x + patch->width;

	if(xstop <= 0)
		return;

	if(xstop > SCREENWIDTH)
		xstop = SCREENWIDTH;

	coloffs = patch->offs - x;

	if(x < 0)
		x = 0;

	desttop = framebuffer + x;

	for( ; x < xstop; x++, desttop++)
	{
		column = (column_t *)((uint8_t*)patch + coloffs[x]);

		while(column->topdelta != 0xFF)
		{
			uint8_t *source;
			uint8_t *dest;
			int32_t yy, ystop;

			source = (uint8_t*)column + 3;

			yy = y + column->topdelta;
			ystop = yy + column->length;

			column = (column_t *)((uint8_t*)column + column->length + 4);

			if(yy < 0)
			{
				source -= yy;
				yy = 0;
			}

			if(ystop > SCREENHEIGHT)
				ystop = SCREENHEIGHT;

			dest = desttop + yy * SCREENWIDTH;

			for( ; yy < ystop; yy++)
			{
				*dest = dr_tinttab[*dest + *source++ * 256];
				dest += SCREENWIDTH;
			}
		}
	}
}

__attribute((regparm(3),no_caller_saved_registers)) // three!
void V_DrawPatchTint1(int32_t x, int32_t y, patch_t *patch)
{
	int32_t xstop;
	column_t *column;
	uint8_t *desttop;
	uint32_t *coloffs;

	x -= patch->x;
	y -= patch->y;

	if(x >= SCREENWIDTH)
		return;

	if(y >= SCREENHEIGHT)
		return;

	xstop = x + patch->width;

	if(xstop <= 0)
		return;

	if(xstop > SCREENWIDTH)
		xstop = SCREENWIDTH;

	coloffs = patch->offs - x;

	if(x < 0)
		x = 0;

	desttop = framebuffer + x;

	for( ; x < xstop; x++, desttop++)
	{
		column = (column_t *)((uint8_t*)patch + coloffs[x]);

		while(column->topdelta != 0xFF)
		{
			uint8_t *source;
			uint8_t *dest;
			int32_t yy, ystop;

			source = (uint8_t*)column + 3;

			yy = y + column->topdelta;
			ystop = yy + column->length;

			column = (column_t *)((uint8_t*)column + column->length + 4);

			if(yy < 0)
			{
				source -= yy;
				yy = 0;
			}

			if(ystop > SCREENHEIGHT)
				ystop = SCREENHEIGHT;

			dest = desttop + yy * SCREENWIDTH;

			for( ; yy < ystop; yy++)
			{
				*dest = dr_tinttab[*dest * 256 + *source++];
				dest += SCREENWIDTH;
			}
		}
	}
}

//
// draw in cache

void draw_patch_to_memory(patch_t *patch, int32_t x, int32_t y, void *dst, uint32_t width, uint32_t height)
{
	int32_t xstop;
	column_t *column;
	uint8_t *desttop;
	uint32_t *coloffs;

	if(x >= width)
		return;

	if(y >= height)
		return;

	xstop = x + patch->width;

	if(xstop <= 0)
		return;

	if(xstop > width)
		xstop = width;

	coloffs = patch->offs - x;

	if(x < 0)
		x = 0;

	desttop = dst + x;

	for( ; x < xstop; x++, desttop++)
	{
		column = (column_t *)((uint8_t*)patch + coloffs[x]);

		while(column->topdelta != 0xFF)
		{
			uint8_t *source;
			uint8_t *dest;
			int32_t yy, ystop;

			source = (uint8_t*)column + 3;

			yy = y + column->topdelta;
			ystop = yy + column->length;

			column = (column_t *)((uint8_t*)column + column->length + 4);

			if(yy < 0)
			{
				source -= yy;
				yy = 0;
			}

			if(ystop > height)
				ystop = height;

			dest = desttop + yy * width;

			for( ; yy < ystop; yy++)
			{
				*dest = *source++;
				dest += width;
			}
		}
	}
}

//
// init

void init_draw()
{
	uint8_t *ptr;

	// I_FinishUpdate always copies the entire screen[0]; TODO: optimize
	// This is not used in VESA mode.
	*((uint8_t**)((void*)I_FinishUpdate + 4)) = screen_buffer;

	//
	// Next part makes specific assumptions about ASM code.

	// prepare tables for R_DrawColumn
	ptr = r_dc_unroll;
	for(int32_t i = 0; i < SCREENHEIGHT; i++)
	{
		int32_t idx = SCREENHEIGHT - i - 1;

		memcpy(ptr, loop_dc_start, sizeof(loop_dc_start));
		*((int32_t*)(ptr + sizeof(loop_dc_start) - sizeof(uint32_t))) = idx * -320;

		r_dc_jump[idx] = ptr;

		ptr += sizeof(loop_dc_start);
	}
	*((uint16_t*)ptr) = 0xC361;

	// prepare tables for R_DrawSpan
	ptr = r_ds_unroll;
	for(int32_t i = 0; i < SCREENWIDTH; i++)
	{
		int32_t idx = SCREENWIDTH - i - 1;

		memcpy(ptr, loop_ds_start, sizeof(loop_ds_start));
		*((int32_t*)(ptr + sizeof(loop_ds_start) - sizeof(uint32_t))) = -idx;

		r_ds_jump[idx] = ptr;

		ptr += sizeof(loop_ds_start);
	}
	*((uint16_t*)ptr) = 0xC361;

	am_forced_color = r_color_duplicate;
}

//
// hooks

static __attribute((regparm(3),no_caller_saved_registers)) // three!
void hook_draw_patch_direct(int32_t x, int32_t y, patch_t *patch)
{
	if(draw_patch_color)
	{
		dc_translation = draw_patch_color;
		V_DrawPatchTranslated(x, y, patch);
		return;
	}
	V_DrawPatchDirect(x, y, patch);
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t check_map_line(line_t *li)
{
	const uint8_t *ptr;
	const lockdef_t *ld;
	int32_t lockdef;

	if(li->flags & ML_DONTDRAW && !am_cheating)
		return 0;

	if(!mod_config.automap_lockdefs)
		return 1;

	if(!li->special)
		return 1;

	if(map_format == MAP_FORMAT_DOOM)
	{
		lockdef = -1;
		ptr = doom_lock_id;
		while(*ptr)
		{
			if(*ptr == li->special)
			{
				lockdef = ptr[1];
				break;
			}
			ptr += 2;
		}
	} else
	if(li->special == 13) // Door_LockedRaise
		lockdef = li->arg3;
	else
		return 1;

	ptr = lockdefs;
	while(ptr < (uint8_t*)lockdefs + lockdefs_size)
	{
		ld = (const lockdef_t*)ptr;

		if(ld->id == lockdef)
		{
			am_forced_color = ld->color;
			return 1;
		}

		ptr += ld->size;
	}

	return 1;
}

static __attribute((regparm(2),no_caller_saved_registers))
void draw_map_line(map_line_t *ml, uint32_t color)
{
	map_line_t fl;
	map_line_t lm;

	if(am_forced_color != r_color_duplicate)
	{
		color = am_forced_color;
		am_forced_color = r_color_duplicate;
	}

	lm = *ml;

	lm.a.y = FixedMul(lm.a.y, AMAP_Y_SCALE);
	lm.b.y = FixedMul(lm.b.y, AMAP_Y_SCALE);

	if(AM_clipMline(&lm, &fl))
		AM_drawFline(&fl, color);
}

static __attribute((regparm(2),no_caller_saved_registers))
void draw_map_grid(uint32_t color)
{
	fixed_t tmp;

	tmp = am_h;
	tmp /= (MAPBLOCKUNITS << FRACBITS);
	tmp += 5;
	tmp *= MAPBLOCKUNITS << FRACBITS;

	mhax_y = am_y - tmp;
	mhax_h = tmp * 2;

	AM_drawGrid(color);
}

static __attribute((regparm(2),no_caller_saved_registers))
void do_follow_player()
{
	fixed_t yy = am_plr->mo->y;
	am_plr->mo->y = FixedMul(am_plr->mo->y, AMAP_Y_SCALE);
	AM_doFollowPlayer();
	am_plr->mo->y = yy;
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
	{0x000392A0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)hook_draw_patch_direct},
	// replace 'I_FinishUpdate'
	{0x0001D4A6, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)I_FinishUpdate},
	{0x0001D4FC, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)I_FinishUpdate},
	// hack check in 'AM_drawWalls'
	{0x00025E2F, CODE_HOOK | HOOK_UINT16, 0xD801},
	{0x00025E31, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)check_map_line},
	{0x00025E36, CODE_HOOK | HOOK_UINT16, 0xC085},
	{0x00025E38, CODE_HOOK | HOOK_SET_NOPS, 5},
	// replace 'AM_drawMline'
	{0x00025CC0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)draw_map_line},
	// replace call to 'AM_doFollowPlayer' in 'AM_Ticker'
	{0x000256F8, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)do_follow_player},
	// replace call to 'AM_drawGrid' in 'AM_Drawer'
	{0x00026339, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)draw_map_grid},
	// replace 'm_y' in 'AM_drawGrid'
	{0x00025D1E, CODE_HOOK | HOOK_UINT32, (uint32_t)&mhax_y},
	{0x00025D56, CODE_HOOK | HOOK_UINT32, (uint32_t)&mhax_y},
	{0x00025D7C, CODE_HOOK | HOOK_UINT32, (uint32_t)&mhax_y},
	// replace 'm_h' in 'AM_drawGrid'
	{0x00025D24, CODE_HOOK | HOOK_UINT32, (uint32_t)&mhax_h},
	{0x00025D82, CODE_HOOK | HOOK_UINT32, (uint32_t)&mhax_h},
	// (automap) use netgame player drawing code, fixed color
	{0x000260A6, CODE_HOOK | HOOK_UINT16, 0x69EB},
	{0x00026146, CODE_HOOK | HOOK_UINT32, 0x0FEBD1B1},
	// (automap) disable teleport line color hack
	{0x00025E68, CODE_HOOK | HOOK_UINT16, 0x14EB},
	// (automap) map background color memset
	{0x00026324, CODE_HOOK | HOOK_UINT16, 0x00B2},
};

