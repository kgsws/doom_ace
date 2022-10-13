// kgsws' ACE Engine
////
// This handles both game and mod configuration.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "config.h"
#include "player.h"
#include "map.h"
#include "stbar.h"
#include "ldr_flat.h"
#include "ldr_texture.h"
#include "ldr_sprite.h"
#include "extra3d.h"
#include "render.h"

#define HEIGHTBITS	12
#define HEIGHTUNIT	(1 << HEIGHTBITS)

//

static visplane_t *fakeplane_ceiling;
static visplane_t *fakeplane_floor;
static extraplane_t *fakesource;

static int16_t *e_floorclip;
static int16_t *e_ceilingclip;

static int32_t clip_height_bot;
static int32_t clip_height_top;
static uint16_t masked_col_step;

static fixed_t cy_weapon;
static fixed_t cy_look;

static fixed_t mlook_pitch;

static void *ptr_visplanes;
static vissprite_t *ptr_vissprites;
static drawseg_t *ptr_drawsegs;

uint8_t r_palette[768];

// mouselook scale
static const uint16_t look_scale_table[] =
{
	1370,
	1030,
	820,
	685,
	585,
	515,
	455,
};

// basic colors
uint8_t r_color_black;

// hooks
static hook_t hook_drawseg[];
static hook_t hook_visplane[];
static hook_t hook_vissprite[];

//
// API

uint8_t r_find_color(uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t ret = 0;
	uint32_t best = 0xFFFFFFFF;
	uint8_t *pal = r_palette;

	for(uint32_t i = 0; i < 256; i++)
	{
		int32_t tmp;
		uint32_t value;

		if(pal[0] == r && pal[1] == g && pal[2] == b)
			return i;

		tmp = pal[0];
		tmp -= r;
		tmp *= tmp;
		value = tmp;

		tmp = pal[1];
		tmp -= g;
		tmp *= tmp;
		value += tmp;

		tmp = pal[2];
		tmp -= b;
		tmp *= tmp;
		value += tmp;

		pal += 3;

		if(value < best)
		{
			ret = i;
			best = value;
		}
	}

	return ret;
}

void r_init_palette(uint8_t *palette)
{
	memcpy(r_palette, palette, 768);
}

void init_render()
{
	ldr_alloc_message = "Render";

	// find some colors
	r_color_black = r_find_color(0, 0, 0);

	// drawseg limit
	if(mod_config.drawseg_count > 256)
	{
		doom_printf("[RENDER] New drawseg limit %u\n", mod_config.drawseg_count);
		// allocate new memory
		ptr_drawsegs = ldr_malloc(mod_config.drawseg_count * sizeof(drawseg_t));
		// update values in hooks
		hook_drawseg[0].value = (uint32_t)ptr_drawsegs + mod_config.drawseg_count * sizeof(drawseg_t);
		for(int i = 1; i <= 3; i++)
			hook_drawseg[i].value = (uint32_t)ptr_drawsegs;
		// install hooks
		utils_install_hooks(hook_drawseg, 4);
	} else
		ptr_drawsegs = d_drawsegs;

	// visplane limit
	if(mod_config.visplane_count > 128)
	{
		doom_printf("[RENDER] New visplane limit %u\n", mod_config.visplane_count);
		// allocate new memory
		ptr_visplanes = ldr_malloc(mod_config.visplane_count * sizeof(visplane_t));
		memset(ptr_visplanes, 0, mod_config.visplane_count * sizeof(visplane_t));
		// update values in hooks
		for(int i = 0; i <= 4; i++)
			hook_visplane[i].value = (uint32_t)ptr_visplanes;
		for(int i = 5; i <= 6; i++)
			hook_visplane[i].value = mod_config.visplane_count;
		// install hooks
		utils_install_hooks(hook_visplane, 7);
	}

	// vissprite limit
	if(mod_config.vissprite_count > 128)
	{
		doom_printf("[RENDER] New vissprite limit %u\n", mod_config.vissprite_count);
		// allocate new memory
		ptr_vissprites = ldr_malloc(mod_config.vissprite_count * sizeof(vissprite_t));
		// update values in hooks
		for(int i = 0; i <= 4; i++)
			hook_vissprite[i].value = (uint32_t)ptr_vissprites;
		hook_vissprite[5].value = (uint32_t)ptr_vissprites + mod_config.vissprite_count * sizeof(vissprite_t);
		// install hooks
		utils_install_hooks(hook_vissprite, 6);
	} else
		ptr_vissprites = d_vissprites;

	// extra planes
	if(mod_config.e3dplane_count < 16)
		mod_config.e3dplane_count = 16;
	e3d_init(mod_config.e3dplane_count);
}

//
// draw

void draw_solid_column(void *data, int32_t fc, int32_t cc, int32_t height)
{
	int32_t top, bot;

	if(height < 0)
	{
		top = 0;
		bot = SCREENHEIGHT - 1;
	} else
	{
		top = sprtopscreen;
		bot = top + spryscale * height;
		top = (top + FRACUNIT - 1) >> FRACBITS;
		bot = (bot - 1) >> FRACBITS;
	}

	if(bot >= fc)
		bot = fc - 1;
	if(top <= cc)
		top = cc + 1;

	if(top <= bot)
	{
		dc_yl = top;
		dc_yh = bot;
		dc_source = data;
		(*colfunc)();
	}
}

void draw_masked_column(column_t *column, int32_t fc, int32_t cc)
{
	int32_t top, bot;
	fixed_t basetexturemid;

	basetexturemid = dc_texturemid;

	while(column->topdelta != 255)
	{
		top = sprtopscreen + spryscale * column->topdelta;
		bot = top + spryscale * column->length;

		top = (top + FRACUNIT - 1) >> FRACBITS;
		bot = (bot - 1) >> FRACBITS;

		if(bot >= fc)
			bot = fc - 1;
		if(top <= cc)
			top = cc + 1;

		if(top <= bot)
		{
			dc_yl = top;
			dc_yh = bot;
			dc_source = (uint8_t*)column + 3;
			dc_texturemid = basetexturemid - (column->topdelta << FRACBITS);
			(*colfunc)();
		}
		column = (column_t*)((uint8_t*)column + column->length + 4);
	}

	dc_texturemid = basetexturemid;
}

__attribute((regparm(2),no_caller_saved_registers))
void R_RenderMaskedSegRange(drawseg_t *ds, int32_t x1, int32_t x2)
{
	int32_t lightnum;
	int32_t scalestep;
	int32_t height;
	int32_t topfrac, topstep;
	int32_t botfrac, botstep;
	uint8_t **wlight;
	int32_t texnum = 0;
	uint16_t *tcol = (uint16_t*)ds->maskedtexturecol;
	seg_t *seg = ds->curline;
	sector_t *frontsector = seg->frontsector;
	sector_t *backsector = seg->backsector;

	// texture
	if(	clip_height_top < 0x7FFFFFFF &&
		frontsector->tag != backsector->tag
	){
		extraplane_t *pl = backsector->exfloor;
		while(pl)
		{
			if(	*pl->texture &&
				clip_height_top <= pl->source->ceilingheight &&
				clip_height_top > pl->source->floorheight
			){
				texnum = texturetranslation[*pl->texture];
				dc_texturemid = pl->source->ceilingheight - viewz;
				height = -1;
				break;
			}
			pl = pl->next;
		}
	}

	if(!texnum)
	{
		texnum = texturetranslation[seg->sidedef->midtexture];
		if(texnum)
		{
			// mid texture offsets
			if(seg->linedef->flags & ML_DONTPEGBOTTOM)
			{
				fixed_t mid = frontsector->floorheight > backsector->floorheight ? frontsector->floorheight : backsector->floorheight;
				dc_texturemid = mid + textureheight[texnum] - viewz;
			} else
			{
				fixed_t mid = frontsector->ceilingheight < backsector->ceilingheight ? frontsector->ceilingheight : backsector->ceilingheight;
				dc_texturemid = mid - viewz;
			}
			height = textureheight[texnum] >> FRACBITS;
		}
	}

	dc_texturemid += seg->sidedef->rowoffset;

	if(!texnum)
	{
		// there is no texture to draw
		// but step has to be flipped anyway
		for(int32_t x = x1; x <= x2; x++)
			if((tcol[x] & 0x8000) == masked_col_step)
				tcol[x] ^= 0x8000;
		return;
	}

	// light

	lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT) + extralight;

	if(seg->v1->y == seg->v2->y)
		lightnum--;
	else
	if(seg->v1->x == seg->v2->x)
		lightnum++;

	if(lightnum < 0)
		wlight = scalelight[0];
	else
	if(lightnum >= LIGHTLEVELS)
		wlight = scalelight[LIGHTLEVELS-1];
	else
		wlight = scalelight[lightnum];

	// scale

	scalestep = ds->scalestep;
	spryscale = ds->scale1 + (x1 - ds->x1) * scalestep;
	mfloorclip = ds->sprbottomclip;
	mceilingclip = ds->sprtopclip;

	if(fixedcolormap)
		dc_colormap = fixedcolormap;

	// clip

	if(clip_height_bot > -0x7FFFFFFF)
	{
		int temp = (clip_height_bot - viewz) >> 4;
		botfrac = (centeryfrac >> 4) - FixedMul(temp, ds->scale1);
		botstep = -FixedMul(scalestep, temp);
		botfrac += botstep * (x1 - ds->x1);
	}

	if(clip_height_top < 0x7FFFFFFF)
	{
		int32_t temp = (clip_height_top - viewz) >> 4;
		topfrac = (centeryfrac >> 4) - FixedMul(temp, ds->scale1);
		topstep = -FixedMul(scalestep, temp);
		topfrac += topstep * (x1 - ds->x1);
	}

	// draw

	for(dc_x = x1; dc_x <= x2; dc_x++)
	{
		if((tcol[dc_x] & 0x8000) == masked_col_step)
		{
			int32_t mfc, mcc;
			void *data;

			if(!fixedcolormap)
			{
				uint32_t index = spryscale >> LIGHTSCALESHIFT;
				if(index >= MAXLIGHTSCALE)
					index = MAXLIGHTSCALE - 1;
				dc_colormap = wlight[index];
			}

			sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
			dc_iscale = 0xFFFFFFFF / (uint32_t)spryscale;

			mfc = mfloorclip[dc_x];
			mcc = mceilingclip[dc_x];

			if(clip_height_bot > -0x7FFFFFFF)
			{
				int32_t tmp = (botfrac >> HEIGHTBITS) + 1;
				if(tmp < mfc)
					mfc = tmp;
			}

			if(clip_height_top < 0x7FFFFFFF)
			{
				int32_t tmp = ((topfrac + HEIGHTUNIT - 1) >> HEIGHTBITS) - 1;
				if(tmp > mcc)
					mcc = tmp;
			}

			data = texture_get_column(texnum, tcol[dc_x] & 0x7FFF);
			if(tex_was_composite)
				draw_solid_column(data, mfc, mcc, height);
			else
				draw_masked_column(data - 3, mfc, mcc);

			tcol[dc_x] ^= 0x8000;
		}

		if(clip_height_bot > -0x7FFFFFFF)
			botfrac += botstep;
		if(clip_height_top < 0x7FFFFFFF)
			topfrac += topstep;
		spryscale += scalestep;
	}
}

static void R_RenderSegStripe(uint32_t texture, fixed_t top, fixed_t bot, int32_t light)
{
	fixed_t tfrac;
	fixed_t tstep;
	fixed_t bfrac;
	fixed_t bstep;
	fixed_t scalefrac = rw_scale;

	light = (light >> LIGHTSEGSHIFT) + extralight;

	if(curline->v1->y == curline->v2->y)
		light--;
	else
	if(curline->v1->x == curline->v2->x)
		light++;

	if(light < 0)
		walllights = scalelight[0];
	else if (light >= LIGHTLEVELS)
		walllights = scalelight[LIGHTLEVELS-1];
	else
		walllights = scalelight[light];

	top -= viewz >> 4;
	bot -= viewz >> 4;

	tstep = -FixedMul(rw_scalestep, top);
	tfrac = (centeryfrac >> 4) - FixedMul(top, scalefrac);

	bstep = -FixedMul(rw_scalestep, bot);
	bfrac = (centeryfrac >> 4) - FixedMul(bot, scalefrac);

	for(uint32_t x = rw_x; x < rw_stopx; x++)
	{
		angle_t angle;
		fixed_t texturecolumn;
		uint32_t index;

		dc_yl = (tfrac + HEIGHTUNIT - 1) >> HEIGHTBITS;
		if(dc_yl < ceilingclip[x] + 1)
			dc_yl = ceilingclip[x] + 1;

		dc_yh = bfrac >> HEIGHTBITS;
		if(dc_yh >= floorclip[x])
			dc_yh = floorclip[x] - 1;

		angle = (rw_centerangle + xtoviewangle[x]) >> ANGLETOFINESHIFT;
		texturecolumn = rw_offset - FixedMul(finetangent[angle], rw_distance);
		texturecolumn >>= FRACBITS;

		index = scalefrac >> LIGHTSCALESHIFT;
		if(index >= MAXLIGHTSCALE)
			index = MAXLIGHTSCALE - 1;

		dc_colormap = walllights[index];
		dc_x = x;
		dc_iscale = 0xFFFFFFFF / (uint32_t)scalefrac;

		dc_source = texture_get_column(texture, texturecolumn);
		colfunc();

		scalefrac += rw_scalestep;
		tfrac += tstep;
		bfrac += bstep;
	}
}

static void render_striped_seg(uint32_t texture, fixed_t ht, fixed_t hb)
{
	extraplane_t *pl;
	fixed_t h0;
	uint16_t light;

	ht >>= 4;
	hb >>= 4;

	h0 = ht;
	light = frontsector->lightlevel;

	pl = frontsector->exfloor;
	while(pl)
	{
		fixed_t h1 = *pl->height >> 4;
		if(h1 >= ht)
		{
			h0 = h1;
			light = *pl->light;
		} else
		if(h1 < h0 && light != *pl->light)
		{
			R_RenderSegStripe(texture, h0, h1, light);
			h0 = h1;
			light = *pl->light;
		}
		pl = pl->next;
	}
	if(h0 > hb)
		R_RenderSegStripe(texture, h0, hb, light);
}

__attribute((regparm(2),no_caller_saved_registers))
static void R_RenderSegLoop()
{
	if(!fixedcolormap && segtextured && frontsector->exfloor)
	{
		if(midtexture)
		{
			dc_texturemid = rw_midtexturemid;
			render_striped_seg(midtexture, frontsector->ceilingheight, frontsector->floorheight);
		} else
		{
			if(toptexture)
			{
				fixed_t top, bot;

				dc_texturemid = rw_toptexturemid;

				top = frontsector->ceilingheight;
				bot = backsector->ceilingheight;

				if(frontsector->floorheight > bot)
					bot = frontsector->floorheight;

				render_striped_seg(toptexture, top, bot);
			}
			if(bottomtexture)
			{
				fixed_t top, bot;

				dc_texturemid = rw_bottomtexturemid;

				top = backsector->floorheight;
				bot = frontsector->floorheight;

				if(frontsector->ceilingheight < top)
					top = frontsector->ceilingheight;

				render_striped_seg(bottomtexture, top, bot);
			}
		}
		segtextured = 0;
	}

	for(uint32_t x = rw_x; x < rw_stopx; x++)
	{
		int32_t top;
		int32_t bottom;
		int32_t yl;
		int32_t yh;
		fixed_t texturecolumn;

		yl = (topfrac + HEIGHTUNIT - 1) >> HEIGHTBITS;
		if(yl < ceilingclip[x] + 1)
			yl = ceilingclip[x] + 1;

		if(markceiling)
		{
			top = ceilingclip[x] + 1;
			bottom = yl - 1;

			if(bottom >= floorclip[x])
				bottom = floorclip[x] - 1;

			if(top <= bottom)
			{
				ceilingplane->top[x] = top;
				ceilingplane->bottom[x] = bottom;
			}
		}

		yh = bottomfrac >> HEIGHTBITS;
		if(yh >= floorclip[x])
			yh = floorclip[x] - 1;

		if(markfloor)
		{
			top = yh + 1;
			bottom = floorclip[x] - 1;
			if(top <= ceilingclip[x])
				top = ceilingclip[x] + 1;
			if(top <= bottom)
			{
				floorplane->top[x] = top;
				floorplane->bottom[x] = bottom;
			}
		}

		if(segtextured)
		{
			angle_t angle;
			uint32_t index;

			angle = (rw_centerangle + xtoviewangle[x]) >> ANGLETOFINESHIFT;
			texturecolumn = rw_offset - FixedMul(finetangent[angle], rw_distance);
			texturecolumn >>= FRACBITS;

			index = rw_scale >> LIGHTSCALESHIFT;
			if(index >= MAXLIGHTSCALE)
				index = MAXLIGHTSCALE - 1;

			dc_colormap = walllights[index];
			dc_x = x;
			dc_iscale = 0xFFFFFFFF / (uint32_t)rw_scale;
		}

		if(midtexture)
		{
			if(segtextured)
			{
				dc_yl = yl;
				dc_yh = yh;
				dc_texturemid = rw_midtexturemid;
				dc_source = texture_get_column(midtexture, texturecolumn);
				colfunc();
			}
			ceilingclip[x] = viewheight;
			floorclip[x] = -1;
		} else
		{
			if(toptexture)
			{
				int32_t mid = pixhigh >> HEIGHTBITS;
				pixhigh += pixhighstep;

				if(mid >= floorclip[x])
					mid = floorclip[x] - 1;

				if(mid >= yl)
				{
					if(segtextured)
					{
						dc_yl = yl;
						dc_yh = mid;
						dc_texturemid = rw_toptexturemid;
						dc_source = texture_get_column(toptexture, texturecolumn);
						colfunc();
					}
					ceilingclip[x] = mid;
				} else
					ceilingclip[x] = yl - 1;
			} else
			{
				if(markceiling)
					ceilingclip[x] = yl - 1;
			}

			if(bottomtexture)
			{
				int32_t mid = (pixlow + HEIGHTUNIT - 1) >> HEIGHTBITS;
				pixlow += pixlowstep;

				if(mid <= ceilingclip[x])
					mid = ceilingclip[x] + 1;

				if(mid <= yh)
				{
					if(segtextured)
					{
						dc_yl = mid;
						dc_yh = yh;
						dc_texturemid = rw_bottomtexturemid;
						dc_source = texture_get_column(bottomtexture, texturecolumn);
						colfunc();
					}
					floorclip[x] = mid;
				} else
					floorclip[x] = yh + 1;
			} else
			{
				if(markfloor)
					floorclip[x] = yh + 1;
			}

			if(maskedtexture)
				maskedtexturecol[x] = texturecolumn & 0x7FFF;
		}

		rw_scale += rw_scalestep;
		topfrac += topstep;
		bottomfrac += bottomstep;
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
void R_DrawVisSprite(vissprite_t *vis)
{
	column_t *column;
	int32_t texturecolumn;
	fixed_t frac;
	patch_t *patch;
	int32_t fc, cc;

	patch = W_CacheLumpNum(sprite_lump[vis->patch], PU_CACHE);

	if(!vis->mo)
	{
		// TODO: fuzz
		if(fixedcolormap)
			dc_colormap = fixedcolormap;
		else
		if(vis->psp->state->frame & FF_FULLBRIGHT)
			dc_colormap = colormaps;
		else
		{
			sector_t *sec = viewplayer->mo->subsector->sector;
			extraplane_t *pl = sec->exfloor;
			int32_t light = sec->lightlevel;
			uint8_t **slight;

			while(pl)
			{
				if(viewz <= *pl->height)
				{
					light = *pl->light;
					break;
				}
				pl = pl->next;
			}

			light = (light >> LIGHTSEGSHIFT) + extralight;
			if(light < 0)		
				slight = scalelight[0];
			else
			if(light >= LIGHTLEVELS)
				slight = scalelight[LIGHTLEVELS-1];
			else
				slight = scalelight[light];

			dc_colormap = slight[MAXLIGHTSCALE-1];
		}
	} else
	{
		// TODO: render style
		if(fixedcolormap)
			dc_colormap = fixedcolormap;
		else
		if(vis->mo->frame & FF_FULLBRIGHT)
			dc_colormap = colormaps;
		else
		{
			sector_t *sec = vis->mo->subsector->sector;
			extraplane_t *pl = sec->exfloor;
			int32_t light = sec->lightlevel;
			int32_t index;
			uint8_t **slight;

			while(pl)
			{
				if(clip_height_top <= *pl->height)
				{
					light = *pl->light;
					break;
				}
				pl = pl->next;
			}

			light = (light >> LIGHTSEGSHIFT) + extralight;

			if(light < 0)
				slight = scalelight[0];
			else
			if(light >= LIGHTLEVELS)
				slight = scalelight[LIGHTLEVELS-1];
			else
				slight = scalelight[light];

			index = vis->scale >> LIGHTSCALESHIFT;
			if(index >= MAXLIGHTSCALE)
				index = MAXLIGHTSCALE - 1;

			dc_colormap = slight[index];
		}

		// TODO: dc_translation
	}

	dc_iscale = abs(vis->xiscale);
	dc_texturemid = vis->texturemid;
	frac = vis->startfrac;
	spryscale = vis->scale;
	sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);

	if(clip_height_bot > -0x7FFFFFFF)
	{
		if(clip_height_bot > dc_texturemid + viewz)
			return;
		fc = (centeryfrac - FixedMul(clip_height_bot - viewz, spryscale)) / FRACUNIT;
		if(fc < 0)
			return;
	} else
		fc = 0x10000;

	if(clip_height_top < 0x7FFFFFFF)
	{
		cc = ((centeryfrac - FixedMul(clip_height_top - viewz, spryscale)) / FRACUNIT) - 1;
		if(cc >= SCREENHEIGHT)
			return;
	} else
		cc = -0x10000;

	for(dc_x = vis->x1; dc_x <= vis->x2; dc_x++, frac += vis->xiscale)
	{
		int32_t mfc, mcc;

		mfc = mfloorclip[dc_x];
		mcc = mceilingclip[dc_x];

		if(cc > mcc)
			mcc = cc;
		if(fc < mfc)
			mfc = fc;

		texturecolumn = frac >> FRACBITS;
		column = (column_t*)((uint8_t*)patch + patch->offs[texturecolumn]);
		draw_masked_column(column, mfc, mcc);
	}
}

static inline void draw_player_sprites()
{
	centery = cy_weapon;
	centeryfrac = cy_weapon << FRACBITS;

	clip_height_top = 0x7FFFFFFF;
	clip_height_bot = -0x7FFFFFFF;

	R_DrawPlayerSprites();

	centery = cy_look;
	centeryfrac = cy_look << FRACBITS;
}

static void draw_masked()
{
	vissprite_t *spr;
	drawseg_t *ds;

	if(vissprite_p > ptr_vissprites)
	{
		for(spr = vsprsortedhead.next; spr != &vsprsortedhead; spr = spr->next)
			R_DrawSprite(spr);
	}

	for(ds = ds_p - 1; ds >= ptr_drawsegs; ds--)
		if(ds->maskedtexturecol)
			R_RenderMaskedSegRange(ds, ds->x1, ds->x2);

	// toggle clip stage
	masked_col_step ^= 0x8000;
}

static inline void draw_masked_range()
{
	fixed_t ht;
	extra_height_t *hh;

	R_SortVisSprites();

	// reset clip stage
	masked_col_step = 0;

	// from top to middle
	clip_height_top = 0x7FFFFFFF;
	hh = e3d_up_height;
	while(hh)
	{
		// sprites and lines
		clip_height_bot = hh->height;
		draw_masked();
		clip_height_top = clip_height_bot;
		// planes
		e3d_draw_height(hh->height);
		// next
		hh = hh->next;
	}
	ht = clip_height_top;

	// from bottom to middle
	clip_height_bot = -0x7FFFFFFF;
	hh = e3d_dn_height;
	while(hh)
	{
		// sprites and lines
		clip_height_top = hh->height;
		draw_masked();
		clip_height_bot = clip_height_top;
		// planes
		e3d_draw_height(hh->height);
		// next
		hh = hh->next;
	}

	// middle - sprites and lines
	clip_height_top = ht;
	draw_masked();
}

void r_draw_plane(visplane_t *pl)
{
	int32_t light;
	int32_t stop;

	ds_source = W_CacheLumpNum(flattranslation[pl->picnum], PU_CACHE);

	planeheight = abs(pl->height - viewz);

	light = (pl->lightlevel >> LIGHTSEGSHIFT) + extralight;

	if(light >= LIGHTLEVELS)
	    light = LIGHTLEVELS - 1;

	if(light < 0)
	    light = 0;

	planezlight = zlight[light];

	pl->top[pl->maxx + 1] = 255;
	pl->top[pl->minx - 1] = 255;

	stop = pl->maxx + 1;
	for(int32_t x = pl->minx; x <= stop; x++)
		R_MakeSpans(x, pl->top[x-1], pl->bottom[x-1], pl->top[x], pl->bottom[x]);
}

//
// BSP

static void store_fake_range(int32_t start, int32_t stop)
{
	seg_t *seg = curline;
	fixed_t hyp;
	fixed_t sineval;
	angle_t distangle, offsetangle;
	fixed_t scale;
	fixed_t scalestep;
	int32_t world;
	int32_t worldstep;
	int32_t worldfrac;

	rw_normalangle = seg->angle + ANG90;
	offsetangle = abs(rw_normalangle - rw_angle1);

	if(offsetangle > ANG90)
		offsetangle = ANG90;

	distangle = ANG90 - offsetangle;
	hyp = R_PointToDist(seg->v1->x, seg->v1->y);
	sineval = finesine[distangle >> ANGLETOFINESHIFT];
	rw_distance = FixedMul(hyp, sineval);

	scale = R_ScaleFromGlobalAngle(viewangle + xtoviewangle[start]);

	if(stop > start)
		scalestep = (R_ScaleFromGlobalAngle(viewangle + xtoviewangle[stop]) - scale) / (stop - start);

	world = *fakesource->height - viewz;
	world >>= 4;

	worldstep = -FixedMul(scalestep, world);
	worldfrac = (centeryfrac >> 4) - FixedMul(world, scale);

	if(fakeplane_floor)
	{
		fakeplane_floor = e3d_check_plane(fakeplane_floor, start, stop);
		if(!fakeplane_floor)
			return;

		while(start <= stop)
		{
			int32_t top;
			int32_t bot;

			top = ((worldfrac + HEIGHTUNIT - 1) >> HEIGHTBITS) - 1;
			if(top <= ceilingclip[start])
				top = ceilingclip[start] + 1;

			bot = e_floorclip[start] - 1;

			if(top <= bot)
			{
				fakeplane_floor->top[start] = top;
				fakeplane_floor->bottom[start] = bot;
			}

			worldfrac += worldstep;
			start++;
		}
	} else
	if(fakeplane_ceiling)
	{
		fakeplane_ceiling = e3d_check_plane(fakeplane_ceiling, start, stop);
		if(!fakeplane_ceiling)
			return;

		while(start <= stop)
		{
			int32_t top;
			int32_t bot;

			bot = (worldfrac >> HEIGHTBITS) + 1;
			top = e_ceilingclip[start] + 1;

			if(bot >= floorclip[start])
				bot = floorclip[start] - 1;

			if(top <= bot)
			{
				fakeplane_ceiling->top[start] = top;
				fakeplane_ceiling->bottom[start] = bot;
			}

			worldfrac += worldstep;
			start++;
		}
	} else
	if(e_floorclip)
	{
		while(start <= stop)
		{
			int32_t y;

			y = (worldfrac + HEIGHTUNIT - 1) >> HEIGHTBITS;

			if(y > floorclip[start])
				y = floorclip[start];

			e_floorclip[start] = y;

			worldfrac += worldstep;
			start++;
		}
	} else
	if(e_ceilingclip)
	{
		while(start <= stop)
		{
			int32_t y;

			y = worldfrac >> HEIGHTBITS;

			if(y < ceilingclip[start])
				y = ceilingclip[start];

			e_ceilingclip[start] = y;

			worldfrac += worldstep;
			start++;
		}
	}
}

static inline void clip_fake_segment(int32_t first, int32_t last)
{
	cliprange_t *start;

	start = solidsegs;
	while(start->last < first - 1)
		start++;

	if(first < start->first)
	{
		if(last < start->first - 1)
		{
			store_fake_range(first, last);
			return;
		}
		store_fake_range(first, start->first - 1);
	}

	if(last <= start->last)
		return;

	while(last >= (start + 1)->first - 1)
	{
		store_fake_range(start->last + 1, (start + 1)->first - 1);
		start++;
		if(last <= start->last)
			return;
	}

	store_fake_range(start->last + 1, last);
}

static __attribute((regparm(3),no_caller_saved_registers)) // three!
void r_add_line(seg_t *line, int32_t x2, int32_t x1)
{
	// NOTE: this is only a second part

	if(fakesource)
	{
		clip_fake_segment(x1, x2-1);
		return;
	}

	backsector = line->backsector;

	if(!backsector)
		goto clipsolid;

	if(frontsector->floorheight >= frontsector->ceilingheight)
		goto clipsolid;

	if(backsector->ceilingheight <= frontsector->floorheight || backsector->floorheight >= frontsector->ceilingheight)
		goto clipsolid;

	if(backsector->ceilingheight != frontsector->ceilingheight || backsector->floorheight != frontsector->floorheight)
		goto clippass;

	if(	backsector->ceilingpic == frontsector->ceilingpic &&
		backsector->floorpic == frontsector->floorpic &&
		backsector->lightlevel == frontsector->lightlevel &&
		line->sidedef->midtexture == 0 &&
		!frontsector->exfloor && !backsector->exfloor
	)
		return;

clippass:
	R_ClipPassWallSegment(x1, x2-1);
	return;

clipsolid:
	R_ClipSolidWallSegment(x1, x2-1);
}

static __attribute((regparm(2),no_caller_saved_registers))
void R_Subsector(uint32_t num)
{
	uint32_t count, idx;
	seg_t *line;
	subsector_t *sub;
	extraplane_t *pl;

	sub = subsectors + num;
	frontsector = sub->sector;

	//
	// fake lines

	idx = 0;
	pl = frontsector->exfloor;
	while(pl)
	{
		uint16_t light;

		if(*pl->height < frontsector->floorheight)
		{
			pl = pl->next;
			continue;
		}

		if(*pl->height >= viewz)
		{
			// out of sight; but must add height for light effects and sides
			e3d_add_height(*pl->height);
			pl = pl->next;
			continue;
		}

		if(pl->next)
			light = *pl->next->light;
		else
			light = frontsector->lightlevel;

		fakesource = pl;
		fakeplane_floor = e3d_find_plane(*pl->height, *pl->pic, light);
		if(fakeplane_floor)
		{
			e3d_add_height(*pl->height);
			count = sub->numlines;
			line = segs + sub->firstline;
			e_floorclip = e3d_floorclip + idx * SCREENWIDTH;

			while(count--)
			{
				R_AddLine(line);
				line++;
			}
		}

		idx++;
		pl = pl->next;
	}

	fakeplane_floor = NULL;
	idx = 0;
	pl = frontsector->exceiling;
	while(pl)
	{
		uint16_t light;

		if(*pl->height > frontsector->ceilingheight)
		{
			pl = pl->next;
			continue;
		}

		if(*pl->height <= viewz)
		{
			// out of sight; but must add height for light effects and sides
			e3d_add_height(*pl->height);
			pl = pl->next;
			continue;
		}

		light = *pl->light; // TODO: this light is often correct, but not always

		fakesource = pl;
		fakeplane_ceiling = e3d_find_plane(*pl->height, *pl->pic, light);
		if(fakeplane_ceiling)
		{
			e3d_add_height(*pl->height);
			count = sub->numlines;
			line = segs + sub->firstline;
			e_ceilingclip = e3d_ceilingclip + idx * SCREENWIDTH;

			while(count--)
			{
				R_AddLine(line);
				line++;
			}
		}

		idx++;
		pl = pl->next;
	}

	//
	// real lines

	fakeplane_ceiling = NULL;
	fakeplane_floor = NULL;
	fakesource = NULL;

	count = sub->numlines;
	line = segs + sub->firstline;

	if(frontsector->floorheight < viewz)
	{
		// find correct light in extra3D
		uint16_t light = frontsector->lightlevel;

		pl = frontsector->exfloor;
		while(pl)
		{
			if(*pl->height >= frontsector->floorheight)
			{
				light = *pl->light;
				break;
			}
			pl = pl->next;
		}

		floorplane = R_FindPlane(frontsector->floorheight, frontsector->floorpic, light);
	} else
		floorplane = NULL;

	if(frontsector->ceilingheight > viewz || frontsector->ceilingpic == skyflatnum)
	{
		// find correct light in extra3D
		uint16_t light = frontsector->lightlevel;

		if(frontsector->ceilingpic == skyflatnum)
			pl = NULL;
		else
			pl = frontsector->exfloor;

		while(pl)
		{
			if(*pl->height >= frontsector->ceilingheight)
			{
				light = *pl->light;
				break;
			}
			pl = pl->next;
		}

		ceilingplane = R_FindPlane(frontsector->ceilingheight, frontsector->ceilingpic, light);
	} else
		ceilingplane = NULL;

	if(frontsector->validcount != validcount)
	{
		frontsector->validcount = validcount;
		for(mobj_t *mo = frontsector->thinglist; mo; mo = mo->snext)
			R_ProjectSprite(mo);
	}

	while(count--)
	{
		// real line
		R_AddLine(line);
		// fake line
		if(line->backsector)
		{
			e_ceilingclip = NULL;
			idx = 0;
			pl = line->backsector->exfloor;
			while(pl)
			{
				fakesource = pl;

				if(*pl->height >= line->backsector->floorheight)
				{
					e_floorclip = e3d_floorclip + idx * SCREENWIDTH;
					R_AddLine(line);
					idx++;
				}

				pl = pl->next;
			}

			e_floorclip = NULL;
			idx = 0;
			pl = line->backsector->exceiling;
			while(pl)
			{
				fakesource = pl;

				if(*pl->height <= line->backsector->ceilingheight)
				{
					e_ceilingclip = e3d_ceilingclip + idx * SCREENWIDTH;
					R_AddLine(line);
					idx++;
				}

				pl = pl->next;
			}

			fakesource = NULL;
		}
		// next
		line++;
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
void extra_mark_check()
{
	if(	frontsector->exfloor || backsector->exfloor ||
		backsector->ceilingheight <= frontsector->floorheight ||
		backsector->floorheight >= frontsector->ceilingheight ||
		backsector->floorheight >= backsector->ceilingheight
	){
		markceiling = 1;
		markfloor = 1;
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t masked_side_check(side_t *side)
{
	if(side->midtexture)
		return 1;

	if(	(backsector->exfloor || backsector->exceiling) &&
		frontsector->tag != backsector->tag
	)
		return 1;

	return 0;
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void custom_SetupFrame(player_t *pl)
{
	fixed_t pn;
	int32_t div;

	if(screenblocks < 10)
		div = look_scale_table[screenblocks - 3];
	else
		div = 410;

	if(extra_config.mouse_look > 1)
		pn = 0;
	else
		pn = finetangent[(pl->mo->pitch + ANG90) >> ANGLETOFINESHIFT] / div;

	if(mlook_pitch != pn)
	{
		fixed_t cy = viewheight / 2;

		mlook_pitch = pn;

		if(pn > 0)
			// allow weapon offset when looking up
			// but not full range
			cy_weapon = cy + (pn >> 4);
		else
			// and not at all when looking down
			cy_weapon = cy;

		cy += pn;
		cy_look = cy;
		centery = cy;
		centeryfrac = cy << FRACBITS;

		// tables for planes
		for(int i = 0; i < viewheight; i++)
		{
			int dy = ((i - cy) << FRACBITS) + FRACUNIT / 2;
			dy = abs(dy);
			yslope[i] = FixedDiv(viewwidth / 2 * FRACUNIT, dy);
		}
	}

	// now run the original
	R_SetupFrame(pl);
}

static __attribute((regparm(2),no_caller_saved_registers))
void hook_RenderPlayerView(player_t *pl)
{
	if(!automapactive)
	{
		// cleanup new stuff
		e3d_reset();
		// 3D view
		R_RenderPlayerView(pl);
		// masked
		draw_masked_range();
		// weapon sprites
		draw_player_sprites();
	}

	// status bar
	stbar_draw(pl);
}

// expanded drawseg limit
static hook_t hook_drawseg[] =
{
	// change limit check pointer in 'R_StoreWallRange'
	{0x00036d0f, CODE_HOOK | HOOK_UINT32, 0},
	// drawseg base pointer at various locations
	{0x00033596, CODE_HOOK | HOOK_UINT32, 0}, // R_ClearDrawSegs
	{0x000383e4, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawSprite
	{0x00038561, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawSprite
};

// expanded visplane limit
static hook_t hook_visplane[] =
{
	// visplane base pointer at various locations
	{0x000361f1, CODE_HOOK | HOOK_UINT32, 0}, // R_ClearPlanes
	{0x0003628e, CODE_HOOK | HOOK_UINT32, 0}, // R_FindPlane
	{0x000362bb, CODE_HOOK | HOOK_UINT32, 0}, // R_FindPlane
	{0x000364fe, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawPlanes
	{0x00036545, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawPlanes
	// visplane max count at various locations
	{0x000362d1, CODE_HOOK | HOOK_UINT32, 0}, // R_FindPlane
	{0x0003650f, CODE_HOOK | HOOK_UINT32, 0}, // R_DrawPlanes
};

// expanded vissprite limit
static hook_t hook_vissprite[] =
{
	// vissprite base pointer at various locations
	{0x00037a66, CODE_HOOK | HOOK_UINT32, 0}, // R_ClearSprites
	{0x000382c0, CODE_HOOK | HOOK_UINT32, 0}, // R_SortVisSprites
	{0x000382e4, CODE_HOOK | HOOK_UINT32, 0}, // R_SortVisSprites
	{0x0003830c, CODE_HOOK | HOOK_UINT32, 0}, // R_SortVisSprites
	{0x00038311, CODE_HOOK | HOOK_UINT32, 0}, // R_SortVisSprites
	// vissprite max pointer
	{0x00037e5a, CODE_HOOK | HOOK_UINT32, 0}, // R_ProjectSprite
};

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace call to 'R_RenderPlayerView' in 'D_Display'
	{0x0001D361, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_RenderPlayerView},
	// replace call to 'R_SetupFrame' in 'R_RenderPlayerView'
	{0x00035FB0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)custom_SetupFrame},
	// skip 'R_RenderPlayerView' in 'R_RenderPlayerView'
	{0x00035FE3, CODE_HOOK | HOOK_JMP_DOOM, 0x0001F170},
	// replace 'R_DrawVisSprite'
	{0x000381FE, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)R_DrawVisSprite},
	{0x000385CE, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)R_DrawVisSprite},
	// replace part of 'R_AddLine'
	{0x000337E0, CODE_HOOK | HOOK_UINT32, 0xD889F289},
	{0x000337E4, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)r_add_line},
	{0x000337E9, CODE_HOOK | HOOK_UINT16, 0x6AEB},
	// replace 'R_Subsector'
	{0x00033ACF, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)R_Subsector},
	{0x00033ADB, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)R_Subsector},
	// replace 'R_RenderSegLoop'
	{0x0003750A, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)R_RenderSegLoop},
	// add extra 'mark' checks to 'R_StoreWallRange'
	{0x000370C1, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)extra_mark_check},
	{0x000370C6, CODE_HOOK | HOOK_UINT16, 0x21EB},
	// replace 'masked' check in 'R_StoreWallRange'
	{0x000371AD, CODE_HOOK | HOOK_UINT16, 0x0D89},
	{0x000371AF, CODE_HOOK | HOOK_ABSADDR_DATA, 0x0005A1CC},
	{0x000371B3, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)masked_side_check},
	{0x000371B8, CODE_HOOK | HOOK_UINT16, 0xC085},
	// replace call to 'R_RenderMaskedSegRange' in 'R_DrawSprite'
	{0x0003846D, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_masked_range_draw},
	// skip 'vis->colormap' in 'R_ProjectSprite'; set 'vis->mo = mo'
	{0x00037F31, CODE_HOOK | HOOK_UINT32, 0x7389 | (offsetof(vissprite_t, mo) << 16)},
	{0x00037F34, CODE_HOOK | HOOK_UINT16, 0x17EB},
	// skip 'vis->colormap' in 'R_DrawPSprite'; set vis->psp
	{0x000381B2, CODE_HOOK | HOOK_UINT32, 0x7089 | (offsetof(vissprite_t, psp) << 16)},
	{0x000381B5, CODE_HOOK | HOOK_UINT16, 0x41EB},
	// replace 'yslope' calculation in 'R_ExecuteSetViewSize'
	{0x00035C10, CODE_HOOK | HOOK_UINT32, 0xFFFFFFB8},
	{0x00035C14, CODE_HOOK | HOOK_UINT16, 0xA37F},
	{0x00035C16, CODE_HOOK | HOOK_UINT32, (uint32_t)&mlook_pitch},
	{0x00035C1A, CODE_HOOK | HOOK_UINT16, 0x5AEB},
	// allow screen size over 11
	{0x00035A8A, CODE_HOOK | HOOK_UINT8, 0x7C},
	{0x00022D2A, CODE_HOOK | HOOK_UINT8, 10},
	{0x000235F0, CODE_HOOK | HOOK_UINT8, 10},
	// call 'R_RenderPlayerView' even in automap
	{0x0001D32B, CODE_HOOK | HOOK_UINT16, 0x07EB},
	// disable "drawseg overflow" error in 'R_DrawPlanes'
	{0x000364C9, CODE_HOOK | HOOK_UINT16, 0x2BEB},
};

