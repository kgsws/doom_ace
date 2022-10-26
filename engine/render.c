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
#include "draw.h"
#include "textpars.h"
#include "render.h"

#define HEIGHTBITS	12
#define HEIGHTUNIT	(1 << HEIGHTBITS)

#define COLORMAP_SIZE	(256 * 33)

//

static visplane_t *fakeplane_ceiling;
static visplane_t *fakeplane_floor;
static extraplane_t *fakesource;

static int16_t *e_floorclip;
static int16_t *e_ceilingclip;

static fixed_t clip_height_bot;
static fixed_t clip_height_top;
static uint16_t masked_col_step;

static fixed_t cy_weapon;
static fixed_t cy_look;

static fixed_t mlook_pitch;

static visplane_t *ptr_visplanes;
static vissprite_t *ptr_vissprites;
static drawseg_t *ptr_drawsegs;

int32_t render_tables_lump = -1;
render_tables_t *render_tables;
uint8_t *render_trn0;
uint8_t *render_trn1;
uint8_t *render_add;
uint8_t *render_translation;

uint64_t *translation_alias;
uint32_t translation_count = NUM_EXTRA_TRANSLATIONS;

pal_col_t r_palette[256];

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
// draw

static void setup_colfunc_tint(uint16_t alpha)
{
	if(alpha > 255)
	{
		colfunc = R_DrawColumnTint0;
		dr_tinttab = render_add;
		return;
	}

	if(alpha > 250)
	{
		colfunc = R_DrawColumn;
		return;
	}

	if(alpha > 178)
	{
		colfunc = R_DrawColumnTint0;
		dr_tinttab = render_trn0;
		return;
	}

	if(alpha > 127)
	{
		colfunc = R_DrawColumnTint0;
		dr_tinttab = render_trn1;
		return;
	}

	if(alpha > 76)
	{
		colfunc = R_DrawColumnTint1;
		dr_tinttab = render_trn1;
		return;
	}

	colfunc = R_DrawColumnTint1;
	dr_tinttab = render_trn0;
}

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
		colfunc();
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
			colfunc();
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

	// texture - extra floors
	if(	clip_height_top < ONCEILINGZ &&
		clip_height_bot > ONFLOORZ &&
		frontsector->tag != backsector->tag
	){
		extraplane_t *pl;

		// back
		pl = backsector->exfloor;
		while(pl)
		{
			if(	pl->alpha &&
				*pl->texture &&
				clip_height_top <= pl->source->ceilingheight &&
				clip_height_top > pl->source->floorheight
			){
				texnum = texturetranslation[*pl->texture];
				dc_texturemid = pl->source->ceilingheight - viewz;
				height = -1;
				setup_colfunc_tint(pl->alpha);
				break;
			}
			pl = pl->next;
		}

		// font
		if(!texnum)
		{
			pl = frontsector->exfloor;
			while(pl)
			{
				if(	pl->alpha &&
					*pl->texture &&
					clip_height_top <= pl->source->ceilingheight &&
					clip_height_top > pl->source->floorheight
				){
					texnum = texturetranslation[*pl->texture];
					dc_texturemid = pl->source->ceilingheight - viewz;
					height = -1;
					setup_colfunc_tint(pl->alpha);
					break;
				}
				pl = pl->next;
			}
		}
	}

	if(!texnum)
	{
		// texture - middle
		texnum = texturetranslation[seg->sidedef->midtexture];
		if(texnum)
		{
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
		// TODO: translucent line
		colfunc = R_DrawColumn;
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

	if(clip_height_bot > ONFLOORZ)
	{
		int temp = (clip_height_bot - viewz) >> 4;
		botfrac = (centeryfrac >> 4) - FixedMul(temp, ds->scale1);
		botstep = -FixedMul(scalestep, temp);
		botfrac += botstep * (x1 - ds->x1);
	}

	if(clip_height_top < ONCEILINGZ)
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
			uint8_t *data;

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

			if(clip_height_bot > ONFLOORZ)
			{
				int32_t tmp = (botfrac >> HEIGHTBITS) + 1;
				if(tmp < mfc)
					mfc = tmp;
			}

			if(clip_height_top < ONCEILINGZ)
			{
				int32_t tmp = ((topfrac + HEIGHTUNIT - 1) >> HEIGHTBITS) - 1;
				if(tmp > mcc)
					mcc = tmp;
			}

			data = texture_get_column(texnum, tcol[dc_x] & 0x7FFF);
			if(tex_was_composite || (height < 0 && data[-2] >= 128))
				draw_solid_column(data, mfc, mcc, height);
			else
				draw_masked_column((column_t*)(data - 3), mfc, mcc);

			tcol[dc_x] ^= 0x8000;
		}

		if(clip_height_bot > ONFLOORZ)
			botfrac += botstep;
		if(clip_height_top < ONCEILINGZ)
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
		if(pl->light)
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

		if(segtextured || maskedtexture)
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
		colfunc = R_DrawColumn;

		if(	(viewplayer->mo->render_style == RS_FUZZ || viewplayer->mo->render_style == RS_INVISIBLE) &&
			(viewplayer->powers[pw_invisibility] > 4*32 || viewplayer->powers[pw_invisibility] & 8))
		{
			if(viewplayer->mo->render_style = RS_INVISIBLE)
			{
				colfunc = R_DrawShadowColumn;
				dc_colormap = colormaps + 32 * 256;
			} else
				colfunc = R_DrawFuzzColumn;
		} else
		{
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
					if(pl->light && viewz <= *pl->height)
						light = *pl->light;
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

			if(viewplayer->mo->render_style == RS_TRANSLUCENT && (viewplayer->powers[pw_invisibility] > 4*32 || viewplayer->powers[pw_invisibility] & 8))
				setup_colfunc_tint(viewplayer->mo->render_alpha);
		}
	} else
	{
		if(vis->mo->render_style == RS_FUZZ)
			colfunc = R_DrawFuzzColumn;
		else
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
				if(pl->light && clip_height_top <= *pl->height)
					light = *pl->light;
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

		switch(vis->mo->render_style)
		{
			case RS_NORMAL:
				colfunc = R_DrawColumn;
			break;
			case RS_SHADOW:
				colfunc = R_DrawShadowColumn;
				dc_colormap = colormaps + 15 * 256;
			break;
			case RS_TRANSLUCENT:
				setup_colfunc_tint(vis->mo->render_alpha);
			break;
			case RS_ADDITIVE:
				colfunc = R_DrawColumnTint0;
				dr_tinttab = render_add;
			break;
		}

		if(vis->mo->translation)
		{
			dc_translation = vis->mo->translation;
			if(colfunc == R_DrawColumn)
				colfunc = R_DrawTranslatedColumn;
			else
			if(colfunc == R_DrawColumnTint0)
				colfunc = R_DrawTranslatedColumnTint0;
			else
			if(colfunc == R_DrawColumnTint1)
				colfunc = R_DrawTranslatedColumnTint1;
		}
	}

	dc_iscale = abs(vis->xiscale);
	dc_texturemid = vis->texturemid;
	frac = vis->startfrac;
	spryscale = vis->scale;
	sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);

	fc = clip_height_bot;
	if(vis->mo && vis->mo->e3d_floorz > ONFLOORZ && vis->mo->e3d_floorz < viewz)
	{
		if(vis->mo->e3d_floorz >= clip_height_top)
			return;
		if(vis->mo->e3d_floorz == fc)
			fc = ONFLOORZ;
	}

	if(fc > ONFLOORZ)
	{
		fc = (((centeryfrac >> 4) - FixedMul((fc - viewz) >> 4, spryscale)) + HEIGHTUNIT - 1) >> HEIGHTBITS;
		if(fc < 0)
			return;
	} else
		fc = 0x10000;

	if(clip_height_top < ONCEILINGZ)
	{
		cc = (((centeryfrac >> 4) - FixedMul((clip_height_top - viewz) >> 4, spryscale)) + HEIGHTUNIT - 1) >> HEIGHTBITS;
		cc--;
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

	clip_height_top = ONCEILINGZ;
	clip_height_bot = ONFLOORZ;

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
	clip_height_top = ONCEILINGZ;
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
	clip_height_bot = ONFLOORZ;
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
	void *oldfunc = NULL;

	if(pl->minx > pl->maxx)
		return;

	if(pl->picnum == numflats)
	{
		oldfunc = spanfunc;
		spanfunc = R_DrawUnknownSpan;
	} else
	if(pl->picnum > numflats)
 		ds_source = flat_generate_composite(pl->picnum - numflats - 1);
	else
		ds_source = W_CacheLumpNum(flatlump[flattranslation[pl->picnum]], PU_CACHE);

	planeheight = abs(pl->height - viewz);

	light = (pl->light >> LIGHTSEGSHIFT) + extralight;

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

	if(oldfunc)
		spanfunc = oldfunc;
}

static void R_DrawPlanes()
{
	colfunc = R_DrawSkyColumn; // for sky texture

	for(visplane_t *pl = ptr_visplanes; pl < lastvisplane; pl++)
	{
		if(pl->picnum == skyflatnum)
		{
			dc_iscale = pspriteiscale;
			dc_colormap = fixedcolormap ? fixedcolormap : colormaps;
			dc_texturemid = skytexturemid;
			for(int32_t x = pl->minx; x <= pl->maxx; x++)
			{
				dc_yl = pl->top[x];
				dc_yh = pl->bottom[x];
				if(dc_yl <= dc_yh)
				{
					uint32_t angle = (viewangle + xtoviewangle[x]) >> ANGLETOSKYSHIFT;
					dc_x = x;
					dc_source = texture_get_column(texturetranslation[skytexture], angle);
					colfunc();
				}
			}
			continue;
		}

		r_draw_plane(pl);
	}
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

			top = (worldfrac + HEIGHTUNIT - 1) >> HEIGHTBITS;
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

			bot = worldfrac >> HEIGHTBITS;
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
	uint16_t light;

	sub = subsectors + num;
	frontsector = sub->sector;

	//
	// fake lines

	idx = 0;
	light = frontsector->lightlevel;
	pl = frontsector->exfloor;
	while(pl)
	{
		extraplane_t *pll;

		if(*pl->height < frontsector->floorheight)
		{
			if(pl->light)
				light = *pl->light;
			pl = pl->next;
			continue;
		}

		e3d_add_height(*pl->height);

		if(*pl->height >= viewz || !pl->alpha)
		{
			// out of sight; but must add height for light effects and sides
			if(pl->light)
				light = *pl->light;
			pl = pl->next;
			continue;
		}

		fakesource = pl;
		fakeplane_floor = e3d_find_plane(*pl->height, *pl->pic, light, pl->alpha);
		if(fakeplane_floor)
		{
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
		if(pl->light)
			light = *pl->light;
		pl = pl->next;
	}

	fakeplane_floor = NULL;
	idx = 0;
	pl = frontsector->exceiling;
	while(pl)
	{
		extraplane_t *pll;

		if(*pl->height > frontsector->ceilingheight)
		{
			pl = pl->next;
			continue;
		}

		e3d_add_height(*pl->height);

		if(*pl->height <= viewz || !pl->alpha)
		{
			// out of sight; but must add height for sides
			pl = pl->next;
			continue;
		}

		// find correct light
		light = frontsector->lightlevel;
		pll = frontsector->exfloor;
		while(pll)
		{
			if(pll->light && *pll->height >= *pl->height)
				light = *pll->light;
			pll = pll->next;
		}

		fakesource = pl;
		fakeplane_ceiling = e3d_find_plane(*pl->height, *pl->pic, light, pl->alpha);
		if(fakeplane_ceiling)
		{
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
		light = frontsector->lightlevel;

		pl = frontsector->exfloor;
		while(pl)
		{
			if(pl->light && *pl->height >= frontsector->floorheight)
				light = *pl->light;
			pl = pl->next;
		}

		floorplane = R_FindPlane(frontsector->floorheight, frontsector->floorpic, light);
	} else
		floorplane = NULL;

	if(frontsector->ceilingheight > viewz || frontsector->ceilingpic == skyflatnum)
	{
		// find correct light in extra3D
		light = frontsector->lightlevel;

		if(frontsector->ceilingpic == skyflatnum)
			pl = NULL;
		else
			pl = frontsector->exfloor;

		while(pl)
		{
			if(pl->light && *pl->height >= frontsector->ceilingheight)
				light = *pl->light;
			pl = pl->next;
		}

		ceilingplane = R_FindPlane(frontsector->ceilingheight, frontsector->ceilingpic, light);
	} else
		ceilingplane = NULL;

	if(frontsector->validcount != validcount)
	{
		frontsector->validcount = validcount;
		for(mobj_t *mo = frontsector->thinglist; mo; mo = mo->snext)
		{
			if(	mo->player != viewplayer &&
				mo->render_style < RS_INVISIBLE &&
				(mo->render_alpha || (mo->render_style != RS_TRANSLUCENT && mo->render_style != RS_ADDITIVE))
			){
				// check for floor "overdraw"
				extraplane_t *pl = frontsector->exfloor;
				mo->e3d_floorz = ONFLOORZ;
				while(pl)
				{
					if(pl->alpha == 255 && pl->flags & E3D_SOLID && *pl->height <= mo->z)
					{
						mo->e3d_floorz = *pl->height;
						break;
					}
					pl = pl->next;
				}

				// add
				R_ProjectSprite(mo);
			}
		}
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

				if(pl->alpha && *pl->height >= line->backsector->floorheight && *pl->height < viewz)
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

				if(pl->alpha && *pl->height <= line->backsector->ceilingheight && *pl->height > viewz)
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

	if(frontsector == linedef->frontsector)
	{
		if(linedef->iflags & MLI_EXTRA_FRONT)
			return 1;
	} else
	{
		if(linedef->iflags & MLI_EXTRA_BACK)
			return 1;
	}

	return 0;
}

//
// funcs

static uint8_t channel_mix(uint8_t back, uint8_t front, uint16_t alpha)
{
	// close enough
	return (((uint16_t)front * alpha) >> 8) + (((uint16_t)back * (256-alpha)) >> 8);
}

static void generate_translucent(uint8_t *dest, uint8_t alpha)
{
	pal_col_t *pc0 = r_palette;
	pal_col_t *pc1;

	for(uint32_t y = 0; y < 256; y++)
	{
		pc1 = r_palette;
		for(uint32_t x = 0; x < 256; x++)
		{
			uint8_t r, g, b;

			r = channel_mix(pc0->r, pc1->r, alpha);
			g = channel_mix(pc0->g, pc1->g, alpha);
			b = channel_mix(pc0->b, pc1->b, alpha);

			*dest++ = r_find_color(r, g, b);

			pc1++;
		}
		pc0++;
		if(render_tables_lump < 0 && y & 1)
			gfx_progress(1);
	}
}

static void generate_additive(uint8_t *dest)
{
	pal_col_t *pc0 = r_palette;
	pal_col_t *pc1;

	for(uint32_t y = 0; y < 256; y++)
	{
		pc1 = r_palette;
		for(uint32_t x = 0; x < 256; x++)
		{
			uint16_t r, g, b;

			r = pc0->r + pc1->r;
			if(r > 255)
				r = 255;
			g = pc0->g + pc1->g;
			if(g > 255)
				g = 255;
			b = pc0->b + pc1->b;
			if(b > 255)
				b = 255;

			*dest++ = r_find_color(r, g, b);

			pc1++;
		}
		pc0++;
		if(render_tables_lump < 0 && y & 1)
			gfx_progress(1);
	}
}

static void generate_color(uint8_t *dest, uint8_t r, uint8_t g, uint8_t b)
{
	pal_col_t *pal = r_palette;
	for(uint32_t i = 0; i < 256; i++, pal++)
	{
		uint8_t rr, gg, bb;

		rr = ((uint32_t)r * (uint32_t)pal->l) / 255;
		gg = ((uint32_t)g * (uint32_t)pal->l) / 255;
		bb = ((uint32_t)b * (uint32_t)pal->l) / 255;

		*dest++ = r_find_color(rr, gg, bb);
	}
}

static void generate_empty_range(uint8_t *dest)
{
	for(uint32_t i = 0; i < 256; i++)
		dest[i] = i;
}

static void generate_translation(uint8_t *dest, uint8_t first, uint8_t last, uint8_t r0, uint8_t r1)
{
	int32_t step;
	uint32_t now = r0 << 16;

	if(last < first)
	{
		step = last;
		last = first;
		first = step;
		step = r1;
		r1 = r0;
		r0 = step;
	}

	step = ((int32_t)r1 + 1) - (int32_t)r0;
	step <<= 16;
	step /= ((int32_t)last + 1) - (int32_t)first;

	for(uint32_t i = first; i <= last; i++)
	{
		dest[i] = now >> 16;
		now += step;
	}
}

//
// callbacks

static void cb_count_translations(lumpinfo_t *li)
{
	uint32_t i;
	uint8_t *kw;

	tp_load_lump(li);

	// get first name
	kw = tp_get_keyword();
	if(!kw)
		return;

	while(1)
	{
		i = translation_count++;
		translation_alias = ldr_realloc(translation_alias, translation_count * sizeof(uint64_t));
		translation_alias[i] = tp_hash64(kw);

		kw = tp_get_keyword();
		if(!kw)
			return;
		if(kw[0] != '=')
			return;

		while(1)
		{
			// skip range
			kw = tp_get_keyword();
			if(!kw)
				return;

			// check for end
			kw = tp_get_keyword();
			if(!kw)
				return;

			if(kw[0] != ',')
				break;
		}
	}
}

static void cb_parse_translations(lumpinfo_t *li)
{
	uint32_t i;
	uint8_t *kw;

	tp_load_lump(li);

	// get first name
	kw = tp_get_keyword();
	if(!kw)
		return;

	while(1)
	{
		i = translation_count++;

		kw = tp_get_keyword();
		if(!kw)
			return;
		if(kw[0] != '=')
			return;

		generate_empty_range(render_translation + i * 256);

		while(1)
		{
			uint32_t t0, t1, t2, t3;

			// parse range
			kw = tp_get_keyword();
			if(!kw)
				return;

			if(doom_sscanf(kw, "%u:%u=%u:%u", &t0, &t1, &t2, &t3) != 4 || (t0|t1|t2|t3) > 255)
				I_Error("[TRNSLATE] Unsupported translation '%s'!\n", kw);

			generate_translation(render_translation + i * 256, t0, t1, t2, t3);

			// check for end
			kw = tp_get_keyword();
			if(!kw)
				return;

			if(kw[0] != ',')
				break;
		}
	}
}

//
// API

uint8_t r_find_color(uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t ret = 0;
	uint32_t best = 0xFFFFFFFF;
	pal_col_t *pal = r_palette;

	for(uint32_t i = 0; i < 256; i++, pal++)
	{
		int32_t tmp;
		uint32_t value;

		if(pal->r == r && pal->g == g && pal->b == b)
			return i;

		tmp = pal->r;
		tmp -= r;
		tmp *= tmp;
		value = tmp;

		tmp = pal->g;
		tmp -= g;
		tmp *= tmp;
		value += tmp;

		tmp = pal->b;
		tmp -= b;
		tmp *= tmp;
		value += tmp;

		if(value < best)
		{
			ret = i;
			best = value;
		}
	}

	return ret;
}

uint8_t *r_translation_by_name(const uint8_t *name)
{
	uint64_t alias = tp_hash64(name);

	for(uint32_t i = 0; i < translation_count; i++)
	{
		if(alias == translation_alias[i])
			return render_translation + 256 * i;
	}

	return NULL;
}

void render_preinit(uint8_t *palette)
{
	int32_t lump;
	pal_col_t *pal = r_palette;

	// save palette
	for(uint32_t i = 0; i < 256; i++, pal++)
	{
		pal->r = *palette++;
		pal->g = *palette++;
		pal->b = *palette++;

		pal->l = pal->r;
		if(pal->l < pal->g)
			pal->l = pal->g;
		if(pal->l < pal->b)
			pal->l = pal->b;
	}

	// check for pre-calculated render tables
	lump = wad_check_lump("ACE_RNDR");
	if(lump < 0)
		return;

	if(lumpinfo[lump].size == sizeof(render_tables_t))
		render_tables_lump = lump;
}

void init_render()
{
	void *ptr;

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
		for(int i = 0; i <= 2; i++)
			hook_visplane[i].value = (uint32_t)ptr_visplanes;
		hook_visplane[3].value = mod_config.visplane_count;
		// install hooks
		utils_install_hooks(hook_visplane, 4);
	} else
		ptr_visplanes = d_visplanes;

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

	//
	// PASS 1

	translation_alias = ldr_malloc(sizeof(uint64_t));
	wad_handle_lump("TRNSLATE", cb_count_translations);

	//
	// PASS 2

	// initialize render style AND translation tables
	ptr = ldr_malloc(sizeof(render_tables_t) + COLORMAP_SIZE + translation_count * 256 + 256);
	ptr = (void*)(((uint32_t)ptr + 255) & ~255); // align for ASM optimisations

	render_translation = ptr + COLORMAP_SIZE + sizeof(render_tables_t);

	// extra translations
	generate_empty_range(render_translation + TRANSLATION_PLAYER2 * 256);
	generate_translation(render_translation + TRANSLATION_PLAYER2 * 256, 112, 127, 96, 111);
	generate_empty_range(render_translation + TRANSLATION_PLAYER3 * 256);
	generate_translation(render_translation + TRANSLATION_PLAYER3 * 256, 112, 127, 64, 79);
	generate_empty_range(render_translation + TRANSLATION_PLAYER4 * 256);
	generate_translation(render_translation + TRANSLATION_PLAYER4 * 256, 112, 127, 32, 47);
	generate_color(render_translation + TRANSLATION_ICE * 256, 148, 148, 172);

	translation_count = NUM_EXTRA_TRANSLATIONS;
	wad_handle_lump("TRNSLATE", cb_parse_translations);

	//
	// PASS 3

	// load original colormap - only 33 levels
	colormaps = ptr;
	wad_read_lump(ptr, wad_get_lump(dtxt_colormap), COLORMAP_SIZE);

	// new tables
	render_tables = ptr + COLORMAP_SIZE;

	// there are two translucency tables, 20/80 and 40/60
	render_trn0 = render_tables->trn0;
	render_trn1 = render_tables->trn1;
	// and one additive table, 100%
	render_add = render_tables->addt;

	if(render_tables_lump < 0)
	{
		// tables have to be generated
		// this is very slow on old PCs
		doom_printf("[RENDER] Generating tables ...\n");
		generate_translucent(render_tables->trn0, 60);
		generate_translucent(render_tables->trn1, 100);
		generate_additive(render_tables->addt);
		// these color translations are not perfect matches ...
		generate_color(render_tables->cmap + 256 * 0, 255, 200, 0);
		generate_color(render_tables->cmap + 256 * 1, 255, 0, 0);
		generate_color(render_tables->cmap + 256 * 2, 200, 255, 180);
		generate_color(render_tables->cmap + 256 * 3, 0, 0, 255);
		generate_color(render_tables->cmap + 256 * 4, 255, 255, 255);
	} else
	{
		// tables are provided in WAD file
		// beware: tables are based on palette!
		wad_read_lump(ptr, render_tables_lump, sizeof(render_tables_t));
	}

	if(M_CheckParm("-dumptables"))
		ldr_dump_buffer("ace_rndr.bin", ptr, sizeof(render_tables_t));
}

void render_player_view(player_t *pl)
{
	// cleanup new stuff
	e3d_reset();
	// solid drawing
	colfunc = R_DrawColumn;
	spanfunc = R_DrawSpan;
	// 3D view
	R_RenderPlayerView(pl);
	// masked
	draw_masked_range();
	// weapon sprites
	draw_player_sprites();
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
		render_player_view(pl);

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
	// visplane max count at various locations
	{0x000362d1, CODE_HOOK | HOOK_UINT32, 0}, // R_FindPlane
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
	// disable 'R_InitColormaps' in 'R_InitData'
	{0x00034646, CODE_HOOK | HOOK_UINT8, 0xC3},
	// disable 'R_InitTranslationTables' in 'R_Init'
	{0x00035E08, CODE_HOOK | HOOK_SET_NOPS, 5},
	// replace call to 'R_RenderPlayerView' in 'D_Display'
	{0x0001D361, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_RenderPlayerView},
	// replace call to 'R_SetupFrame' in 'R_RenderPlayerView'
	{0x00035FB0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)custom_SetupFrame},
	// skip 'R_DrawMasked' in 'R_RenderPlayerView'
	{0x00035FE3, CODE_HOOK | HOOK_JMP_DOOM, 0x0001F170},
	// skip call to 'R_DrawPlanes' in 'R_RenderPlayerView'
	{0x00035FDE, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)R_DrawPlanes},
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
};

