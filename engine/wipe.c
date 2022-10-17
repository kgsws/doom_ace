// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "config.h"
#include "render.h"
#include "wipe.h"

#define VIDEO_RAM	(void*)0x000A0000

static uint32_t wipe_tick;
static uint32_t wipe_type;

//
uint8_t *wipe_name[NUM_WIPE_TYPES] =
{
	[WIPE_NONE] = "none",
	[WIPE_MELT] = "melt",
	[WIPE_SCROLL_LEFT] = "left",
	[WIPE_SCROLL_RIGHT] = "right",
	[WIPE_SCROLL_UP] = "up",
	[WIPE_SCROLL_DOWN] = "down",
	[WIPE_SCROLL_HORIZONTAL] = "horiz",
	[WIPE_SCROLL_VERTICAL] = "vert",
	[WIPE_CROSSFADE] = "fade",
};

//
// functions

static void copy_column(uint8_t *src, uint32_t x, uint32_t dy, uint32_t length)
{
	uint8_t *dst;

	if(dy >= SCREENHEIGHT)
		return;

	if(length + dy > SCREENHEIGHT)
		length = SCREENHEIGHT - dy;

	dst = VIDEO_RAM + x + dy * SCREENWIDTH;

	while(length)
	{
		*dst = *src;
		dst += SCREENWIDTH;
		src += SCREENWIDTH;
		length--;
	}
}

//
// wipe drawers

static uint32_t wipe_melt()
{
	// the original effect
	uint32_t done = 1;

	if(!wipe_tick)
	{
		// init
		uint8_t idx = gametic;

		floorclip[0] = -(rndtable[idx++] & 15);
		for(uint32_t x = 1; x < SCREENWIDTH; x++)
		{
			int32_t r = (rndtable[idx++] % 3) - 1;
			floorclip[x] = floorclip[x-1] + r;
			if(floorclip[x] > 0)
				floorclip[x] = 0;
			else
			if(floorclip[x] == -16)
				floorclip[x] = -15;
		}

		wipe_tick++;
	}

	for(uint32_t x = 0; x < SCREENWIDTH; x++)
	{
		int32_t offset = floorclip[x];

		if(offset < 0)
		{
			floorclip[x]++;
			done = 0;

			// old frame covers entire column
			copy_column(screen_buffer + x + (SCREENWIDTH * SCREENHEIGHT), x, offset, SCREENHEIGHT);
		} else
		if(offset < SCREENHEIGHT)
		{
			offset += offset < 16 ? offset + 1 : 8;
			floorclip[x] = offset;
			done = 0;

			// new frame
			copy_column(screen_buffer + x, x, 0, offset);

			// old frame melts
			copy_column(screen_buffer + x + (SCREENWIDTH * SCREENHEIGHT), x, offset, SCREENHEIGHT);
		}
	}

	return done;
}

static uint32_t wipe_left()
{
	int32_t offs;

	if(!wipe_tick)
		wipe_tick = 2;

	offs = (FRACUNIT + finecosine[wipe_tick * 140]) * SCREENWIDTH;
	offs >>= (FRACBITS + 1);

	for(int32_t x = offs; x < SCREENWIDTH; x++)
		copy_column(screen_buffer + (x - offs), x, 0, SCREENHEIGHT);
	offs--;
	for(int32_t x = offs; x >= 0; x--)
		copy_column(screen_buffer + (SCREENWIDTH - 1 + (x - offs)) + (SCREENWIDTH * SCREENHEIGHT), x, 0, SCREENHEIGHT);

	return ++wipe_tick >= 28;
}

static uint32_t wipe_right()
{
	int32_t offs;

	if(!wipe_tick)
		wipe_tick = 2;

	offs = (FRACUNIT + finecosine[wipe_tick * 140]) * SCREENWIDTH;
	offs >>= (FRACBITS + 1);
	offs = (SCREENWIDTH-1) - offs;

	for(int32_t x = offs; x < SCREENWIDTH; x++)
		copy_column(screen_buffer + (x - offs) + (SCREENWIDTH * SCREENHEIGHT), x, 0, SCREENHEIGHT);
	offs--;
	for(int32_t x = offs; x >= 0; x--)
		copy_column(screen_buffer + (SCREENWIDTH - 1 + (x - offs)), x, 0, SCREENHEIGHT);

	return ++wipe_tick >= 28;
}

static uint32_t wipe_up()
{
	int32_t offs;

	if(!wipe_tick)
		wipe_tick = 2;

	offs = (FRACUNIT + finecosine[wipe_tick * 140]) * SCREENHEIGHT;
	offs >>= (FRACBITS + 1);
	offs = (SCREENHEIGHT-1) - offs;

	dwcopy(VIDEO_RAM, screen_buffer + offs * SCREENWIDTH + (SCREENWIDTH * SCREENHEIGHT), (SCREENHEIGHT - offs) * (SCREENWIDTH / sizeof(uint32_t)));
	dwcopy(VIDEO_RAM + (SCREENHEIGHT - offs) * SCREENWIDTH, screen_buffer, offs * (SCREENWIDTH / sizeof(uint32_t)));

	return ++wipe_tick >= 28;
}

static uint32_t wipe_down()
{
	int32_t offs;

	if(!wipe_tick)
		wipe_tick = 2;

	offs = (FRACUNIT + finecosine[wipe_tick * 140]) * SCREENHEIGHT;
	offs >>= (FRACBITS + 1);

	dwcopy(VIDEO_RAM, screen_buffer + offs * SCREENWIDTH, (SCREENHEIGHT - offs) * (SCREENWIDTH / sizeof(uint32_t)));
	dwcopy(VIDEO_RAM + (SCREENHEIGHT - offs) * SCREENWIDTH, screen_buffer + (SCREENWIDTH * SCREENHEIGHT), offs * (SCREENWIDTH / sizeof(uint32_t)));

	return ++wipe_tick >= 28;
}

static uint32_t wipe_horiz()
{
	int32_t offs;
	int32_t stop;

	if(!wipe_tick)
		wipe_tick = 2;

	offs = (FRACUNIT + finecosine[wipe_tick * 70]) * SCREENWIDTH;
	offs >>= (FRACBITS + 1);
	offs -= SCREENWIDTH / 2;
	stop = SCREENWIDTH - offs;

	for(int32_t x = offs; x < stop; x++)
		copy_column(screen_buffer + x, x, 0, SCREENHEIGHT);
	for(int32_t x = 0; x < offs; x++)
		copy_column(screen_buffer + x + ((SCREENWIDTH / 2) - 1 - offs) + (SCREENWIDTH * SCREENHEIGHT), x, 0, SCREENHEIGHT);
	for(int32_t x = stop; x < SCREENWIDTH; x++)
		copy_column(screen_buffer + (x - stop) + (SCREENWIDTH / 2) + (SCREENWIDTH * SCREENHEIGHT), x, 0, SCREENHEIGHT);

	return ++wipe_tick >= 28;
}

static uint32_t wipe_vert()
{
	int32_t offs;
	int32_t stop;

	if(!wipe_tick)
		wipe_tick = 2;

	offs = (FRACUNIT + finecosine[wipe_tick * 70]) * SCREENHEIGHT;
	offs >>= (FRACBITS + 1);
	offs -= SCREENHEIGHT / 2;
	stop = SCREENHEIGHT - offs;

	dwcopy(VIDEO_RAM + offs * SCREENWIDTH, screen_buffer + offs * SCREENWIDTH, (stop - offs) * (SCREENWIDTH / sizeof(uint32_t)));
	dwcopy(VIDEO_RAM, screen_buffer + ((SCREENHEIGHT / 2) - 1 - offs) * SCREENWIDTH + (SCREENWIDTH * SCREENHEIGHT), offs * (SCREENWIDTH / sizeof(uint32_t)));
	dwcopy(VIDEO_RAM + stop * SCREENWIDTH, screen_buffer + (SCREENHEIGHT / 2) * SCREENWIDTH + (SCREENWIDTH * SCREENHEIGHT), (SCREENHEIGHT - offs) * (SCREENWIDTH / sizeof(uint32_t)));

	return ++wipe_tick >= 28;
}

static uint32_t wipe_fade()
{
	if(wipe_tick & 1)
	{
		// 1 frameskip
		wipe_tick++;
		return 0;
	}

	uint8_t *dst = VIDEO_RAM;
	uint8_t *src0 = screen_buffer + (SCREENWIDTH * SCREENHEIGHT); // old screen
	uint8_t *src1 = screen_buffer; // new screen

	for(uint32_t y = 0; y < SCREENHEIGHT; y++)
	{
		for(uint32_t x = 0; x < SCREENWIDTH; x++)
		{
			if((x ^ y) & 1) // dither
			{
				if(wipe_tick < 2)
					*dst = *src0;
				else
				if(wipe_tick < 6)
					*dst = render_trn0[*src0 * 256 + *src1];
				else
				if(wipe_tick < 10)
					*dst = render_trn1[*src0 * 256 + *src1];
				else
				if(wipe_tick < 14)
					*dst = render_trn1[*src0 + *src1 * 256];
				else
					*dst = render_trn0[*src0 + *src1 * 256];
			} else
			{
				if(wipe_tick < 4)
					*dst = render_trn0[*src0 * 256 + *src1];
				else
				if(wipe_tick < 8)
					*dst = render_trn1[*src0 * 256 + *src1];
				else
				if(wipe_tick < 12)
					*dst = render_trn1[*src0 + *src1 * 256];
				else
				if(wipe_tick < 16)
					*dst = render_trn0[*src0 + *src1 * 256];
				else
					*dst = *src1;
			}

			dst++;
			src0++;
			src1++;
		}
	}

	return ++wipe_tick > 16;
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void wipe_start()
{
	if(mod_config.wipe_type < NUM_WIPE_TYPES)
		wipe_type = mod_config.wipe_type;
	else
		wipe_type = extra_config.wipe_type;

	wipe_tick = 0;

	if(wipe_type)
		// save old screen
		dwcopy(screen_buffer + (SCREENWIDTH * SCREENHEIGHT), screen_buffer, (SCREENWIDTH * SCREENHEIGHT) / sizeof(uint32_t));
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t wipe_draw()
{
	uint32_t done = 0;

	switch(wipe_type)
	{
		case WIPE_MELT:
			done = wipe_melt();
		break;
		case WIPE_SCROLL_LEFT:
			done = wipe_left();
		break;
		case WIPE_SCROLL_RIGHT:
			done = wipe_right();
		break;
		case WIPE_SCROLL_UP:
			done = wipe_up();
		break;
		case WIPE_SCROLL_DOWN:
			done = wipe_down();
		break;
		case WIPE_SCROLL_HORIZONTAL:
			done = wipe_horiz();
		break;
		case WIPE_SCROLL_VERTICAL:
			done = wipe_vert();
		break;
		case WIPE_CROSSFADE:
			done = wipe_fade();
		break;
		default:
			done = 1;
		break;
	}

	I_WaitVBL(2);

	return done;
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'wipe_StartScreen' in 'D_Display'
	{0x0001D239, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)wipe_start},
	// disable 'wipe_EndScreen'
	{0x0001D4BB, CODE_HOOK | HOOK_SET_NOPS, 5},
	// replace call to 'wipe_ScreenWipe' in 'D_Display'; disable call to 'I_UpdateNoBlit' and 'M_Drawer'
	{0x0001D4D5, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)wipe_draw},
	{0x0001D4DA, CODE_HOOK | HOOK_UINT32, 0x25EBC085},
	// disable wipe delay (it uses VBlank now)
	{0x0001D4CA, CODE_HOOK | HOOK_UINT32, 0x09EB},
};

