// kgsws' ACE Engine
////
// Generic floor / ceiling mover. Generic light effect.
// Replaces all original types. Used only in Hexen map format.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "sound.h"
#include "map.h"
#include "extra3d.h"
#include "think.h"
#include "mobj.h"
#include "generic.h"

//
// light effect

static void light_effect(uint32_t tag, fixed_t frac)
{
	for(uint32_t i = 0; i < numsectors; i++)
	{
		sector_t *sec = sectors + i;
		int32_t top, bot;

		if(sec->tag != tag)
			continue;

		top = 0;
		bot = sec->lightlevel;

		for(uint32_t j = 0; j < sec->linecount; j++)
		{
			line_t *li = sec->lines[j];
			sector_t *bs;

			if(li->frontsector == sec)
				bs = li->backsector;
			else
				bs = li->frontsector;
			if(!bs)
				continue;

			if(bs->lightlevel < bot)
				bot = bs->lightlevel;
			if(bs->lightlevel > top)
				top = bs->lightlevel;
		}

		bot += (((top - bot) * frac) >> 16);
		if(bot < 0)
			bot = 0;
		else
		if(bot > 255)
			bot = 255;

		sec->lightlevel = bot;
	}
}

//
//

static uint32_t plane_movement(sector_t *sec, fixed_t dist, uint32_t crush, uint32_t what_plane, uint32_t do_stop)
{
	uint32_t blocked;
	plane_link_t *plink;

	// TODO: find lowest DIST possible to prevent floors in ceilings (like ZDoom does)

	e3d_plane_move = 1;

	if(what_plane & 1)
		sec->ceilingheight += dist;
	if(what_plane & 2)
		sec->floorheight += dist;

	blocked = mobj_change_sector(sec, crush);

	if(sec->extra->plink)
	{
		plink = sec->extra->plink;
		while(plink->target)
		{
			if(	(plink->use_ceiling && what_plane & 1) ||
				(!plink->use_ceiling && what_plane & 2)
			){
				sector_t *ss = plink->target;

				if(plink->link_ceiling)
					ss->ceilingheight += dist;
				if(plink->link_floor)
					ss->floorheight += dist;

				blocked |= mobj_change_sector(ss, crush);
			}

			plink++;
		}
	}

	if(blocked)
	{
		if(do_stop)
		{
			if(what_plane & 1)
				sec->ceilingheight -= dist;
			if(what_plane & 2)
				sec->floorheight -= dist;
			mobj_change_sector(sec, 0);

			if(sec->extra->plink)
			{
				plink = sec->extra->plink;
				while(plink->target)
				{
					if(	(plink->use_ceiling && what_plane & 1) ||
						(!plink->use_ceiling && what_plane & 2)
					){
						sector_t *ss = plink->target;

						if(plink->link_ceiling)
							ss->ceilingheight -= dist;
						if(plink->link_floor)
							ss->floorheight -= dist;

						mobj_change_sector(ss, 0);
					}

					plink++;
				}
			}
		}
	}

	e3d_plane_move = 0;

	return blocked;
}

//
// thinkers

__attribute((regparm(2),no_caller_saved_registers))
void think_ceiling(generic_mover_t *gm)
{
	sector_t *sec;

	if(gm->sndwait)
		gm->sndwait--;

	if(gm->wait)
	{
		if(gm->flags & MVF_WAIT_STOP)
			return;
		gm->wait--;
		if(!gm->wait)
		{
			seq_sounds_t *seq;
			seq = gm->direction == DIR_UP ? gm->up_seq : gm->dn_seq;
			if(seq)
			{
				if(seq->start)
					S_StartSound((mobj_t*)&gm->sector->soundorg, seq->start);
				gm->sndwait = seq->delay;
			}
		} else
			return;
	}

	sec = gm->sector;

	if(gm->direction == DIR_UP)
	{
		fixed_t dist;

		if(sec->ceilingheight + gm->speed_now > gm->top_height)
			dist = gm->top_height - sec->ceilingheight;
		else
			dist = gm->speed_now;

		if(!gm->sndwait && gm->up_seq && gm->up_seq->move)
		{
			S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->move);
			gm->sndwait = gm->up_seq->repeat;
		}

		if(plane_movement(sec, dist, 0, 1, 1))
		{
			if(gm->flags & MVF_BLOCK_GO_DN)
			{
				gm->direction = DIR_DOWN;
				if(gm->speed_dn)
					gm->speed_now = gm->speed_dn;
				if(gm->dn_seq)
				{
					if(gm->dn_seq->start)
						S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->start);
					gm->sndwait = gm->dn_seq->delay;
				}
			}

			if(gm->flags & MVF_BLOCK_SLOW)
				gm->speed_now = FRACUNIT / 8;

			return;
		}

		if(sec->ceilingheight >= gm->top_height)
		{
			if(sec->e3d_origin)
				e3d_update_top(sec);

			if(gm->lighttag)
				light_effect(gm->lighttag, 0x10000);

			if(!gm->up_seq || !gm->up_seq->stop)
			{
				gm->sndwait = 0;
				S_StopSound((mobj_t*)&gm->sector->soundorg);
			} else
				S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->stop);

			if(gm->speed_dn)
			{
				gm->direction = DIR_DOWN;
				gm->wait = gm->delay;
				gm->speed_now = gm->speed_dn;
				return;
			}
			goto finish_move;
		}
	} else
	{
		fixed_t dist;

		if(sec->ceilingheight - gm->speed_now < gm->bot_height)
			dist = gm->bot_height - sec->ceilingheight;
		else
			dist = -gm->speed_now;

		if(!gm->sndwait && gm->dn_seq && gm->dn_seq->move)
		{
			S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->move);
			gm->sndwait = gm->dn_seq->repeat;
		}

		if(plane_movement(sec, dist, gm->crush, 1, gm->flags & (MVF_BLOCK_STAY|MVF_BLOCK_GO_UP)))
		{
			if(gm->flags & MVF_BLOCK_GO_UP)
			{
				gm->direction = DIR_UP;
				if(gm->speed_up)
					gm->speed_now = gm->speed_up;
				if(gm->up_seq)
				{
					if(gm->up_seq->start)
						S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->start);
					gm->sndwait = gm->up_seq->delay;
				}
			}

			if(gm->flags & MVF_BLOCK_SLOW)
				gm->speed_now = FRACUNIT / 8;

			if(gm->flags & (MVF_BLOCK_STAY | MVF_BLOCK_GO_UP))
				return;
		}

		if(sec->ceilingheight <= gm->bot_height)
		{
			if(sec->e3d_origin)
				e3d_update_top(sec);

			if(gm->lighttag)
				light_effect(gm->lighttag, 0);

			if(!gm->dn_seq || !gm->dn_seq->stop)
			{
				gm->sndwait = 0;
				S_StopSound((mobj_t*)&gm->sector->soundorg);
			} else
				S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->stop);

			if(gm->speed_up)
			{
				gm->direction = DIR_UP;
				gm->wait = gm->delay;
				gm->speed_now = gm->speed_up;
				return;
			}
			goto finish_move;
		}
	}

	if(sec->e3d_origin)
		e3d_update_top(sec);

	if(gm->lighttag)
	{
		fixed_t frac;
		frac = FixedDiv(sec->ceilingheight - gm->bot_height, gm->top_height - gm->bot_height);
		light_effect(gm->lighttag, frac);
	}

	return;

finish_move:
	if(gm->flags & MVF_SET_TEXTURE)
		sec->ceilingpic = gm->texture;
	if(gm->flags & MVF_SET_SPECIAL)
		sec->special = gm->special;

	gm->thinker.function = (void*)-1;
	sec->specialactive &= ~ACT_CEILING;
}

__attribute((regparm(2),no_caller_saved_registers))
void think_floor(generic_mover_t *gm)
{
	sector_t *sec;
	fixed_t original;

	if(gm->sndwait)
		gm->sndwait--;

	if(gm->wait)
	{
		if(gm->flags & MVF_WAIT_STOP)
			return;
		gm->wait--;
		if(!gm->wait)
		{
			seq_sounds_t *seq;
			seq = gm->direction == DIR_UP ? gm->up_seq : gm->dn_seq;
			if(seq)
			{
				if(seq->start)
					S_StartSound((mobj_t*)&gm->sector->soundorg, seq->start);
				gm->sndwait = seq->delay;
			}
		} else
			return;
	}

	sec = gm->sector;
	original = sec->floorheight;

	if(gm->direction == DIR_UP)
	{
		fixed_t dist;

		if(sec->floorheight + gm->speed_now > gm->top_height)
			dist = gm->top_height - sec->floorheight;
		else
			dist = gm->speed_now;

		if(!gm->sndwait && gm->up_seq && gm->up_seq->move)
		{
			S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->move);
			gm->sndwait = gm->up_seq->repeat;
		}

		if(plane_movement(sec, dist, gm->crush, 2, gm->flags & (MVF_BLOCK_STAY|MVF_BLOCK_GO_DN)))
		{
			if(gm->flags & MVF_BLOCK_GO_DN)
			{
				gm->direction = DIR_DOWN;
				if(gm->speed_dn)
					gm->speed_now = gm->speed_dn;
				if(gm->dn_seq)
				{
					if(gm->dn_seq->start)
						S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->start);
					gm->sndwait = gm->dn_seq->delay;
				}
			}

			if(gm->flags & MVF_BLOCK_SLOW)
				gm->speed_now = FRACUNIT / 8;

			if(gm->flags & (MVF_BLOCK_STAY | MVF_BLOCK_GO_DN))
				return;
		}

		if(sec->floorheight >= gm->top_height)
		{
			if(sec->e3d_origin)
				e3d_update_bot(sec);

			if(!gm->up_seq || !gm->up_seq->stop)
			{
				gm->sndwait = 0;
				S_StopSound((mobj_t*)&gm->sector->soundorg);
			} else
				S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->stop);

			if(gm->speed_dn)
			{
				gm->direction = DIR_DOWN;
				gm->wait = gm->delay;
				gm->speed_now = gm->speed_dn;
				return;
			}
			goto finish_move;
		}
	} else
	{
		fixed_t dist;

		if(sec->floorheight - gm->speed_now < gm->bot_height)
			dist = gm->bot_height - sec->floorheight;
		else
			dist = -gm->speed_now;

		if(!gm->sndwait && gm->dn_seq && gm->dn_seq->move)
		{
			S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->move);
			gm->sndwait = gm->dn_seq->repeat;
		}

		if(plane_movement(sec, dist, 0, 2, 1))
		{
			if(gm->flags & MVF_BLOCK_GO_UP)
			{
				gm->direction = DIR_UP;
				if(gm->speed_up)
					gm->speed_now = gm->speed_up;
				if(gm->up_seq)
				{
					if(gm->up_seq->start)
						S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->start);
					gm->sndwait = gm->up_seq->delay;
				}
			}

			if(gm->flags & MVF_BLOCK_SLOW)
				gm->speed_now = FRACUNIT / 8;

			return;
		}

		if(sec->floorheight <= gm->bot_height)
		{
			if(sec->e3d_origin)
				e3d_update_bot(sec);

			if(!gm->dn_seq || !gm->dn_seq->stop)
			{
				gm->sndwait = 0;
				S_StopSound((mobj_t*)&gm->sector->soundorg);
			} else
				S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->stop);

			if(gm->speed_up)
			{
				gm->direction = DIR_UP;
				gm->wait = gm->delay;
				gm->speed_now = gm->speed_up;
				return;
			}
			goto finish_move;
		}
	}

	if(sec->e3d_origin)
		e3d_update_bot(sec);

	return;

finish_move:
	if(gm->flags & MVF_SET_TEXTURE)
		sec->floorpic = gm->texture;
	if(gm->flags & MVF_SET_SPECIAL)
		sec->special = gm->special;

	gm->thinker.function = (void*)-1;
	sec->specialactive &= ~ACT_FLOOR;
}

__attribute((regparm(2),no_caller_saved_registers))
void think_light(generic_light_t *gl)
{
	sector_t *sec;
	fixed_t level;
	uint32_t finished = 0;

	if(gl->wait)
	{
		gl->wait--;
		return;
	}

	sec = gl->sector;
	level = gl->level;

	if(gl->direction == DIR_UP)
	{
		level += gl->speed;
		if(level >= gl->top)
		{
			level = gl->top;
			if(gl->flags & LIF_TOP_REVERSE)
			{
				gl->wait = gl->delay_top;
				gl->direction = DIR_DOWN;
			} else
				finished = 1;
		}
	} else
	{
		level -= gl->speed;
		if(level <= gl->bot)
		{
			level = gl->bot;
			if(gl->flags & LIF_BOT_REVERSE)
			{
				gl->wait = gl->delay_bot;
				gl->direction = DIR_UP;
			} else
				finished = 1;
		}
	}

	sec->lightlevel &= 0xFE00;
	sec->lightlevel |= level >> FRACBITS;
	gl->level = level;

	if(!finished)
		return;

	gl->thinker.function = (void*)-1;
	sec->specialactive &= ~ACT_LIGHT;
}

//
// API - ceiling

generic_mover_t *generic_ceiling(sector_t *sec, uint32_t dir, uint32_t def_seq, uint32_t is_fast)
{
	generic_mover_t *gm;
	sound_seq_t *seq;

	if(sec->specialactive & ACT_CEILING)
		return NULL;
	sec->specialactive |= ACT_CEILING;

	gm = Z_Malloc(sizeof(generic_mover_t), PU_LEVELSPEC, NULL);
	memset(gm, 0, sizeof(generic_mover_t));
	gm->thinker.function = think_ceiling;
	gm->sector = sec;
	gm->direction = dir;
	think_add(&gm->thinker);

	gm->seq_save = def_seq | (!!is_fast << 7);

	seq = snd_seq_by_sector(sec, def_seq);
	if(seq)
	{
		seq_sounds_t *snd;
		if(is_fast)
		{
			gm->up_seq = &seq->fast_open;
			gm->dn_seq = &seq->fast_close;
		} else
		{
			gm->up_seq = &seq->norm_open;
			gm->dn_seq = &seq->norm_close;
		}
		snd = dir == DIR_UP ? gm->up_seq : gm->dn_seq;
		if(snd->start && !map_skip_stuff)
			S_StartSound((mobj_t*)&gm->sector->soundorg, snd->start);
		gm->sndwait = snd->delay;
	}

	return gm;
}

generic_mover_t *generic_ceiling_by_sector(sector_t *sec)
{
	for(thinker_t *th = thcap.next; th != &thcap; th = th->next)
	{
		generic_mover_t *gm;

		if(th->function != think_ceiling)
			continue;

		gm = (generic_mover_t*)th;
		if(gm->sector == sec)
			return gm;
	}

	return NULL;
}

//
// API - floor

generic_mover_t *generic_floor(sector_t *sec, uint32_t dir, uint32_t def_seq, uint32_t is_fast)
{
	generic_mover_t *gm;
	sound_seq_t *seq;

	if(sec->specialactive & ACT_FLOOR)
		return NULL;
	sec->specialactive |= ACT_FLOOR;

	gm = Z_Malloc(sizeof(generic_mover_t), PU_LEVELSPEC, NULL);
	memset(gm, 0, sizeof(generic_mover_t));
	gm->thinker.function = think_floor;
	gm->sector = sec;
	gm->direction = dir;
	think_add(&gm->thinker);

	gm->seq_save = def_seq | (!!is_fast << 7);

	seq = snd_seq_by_sector(sec, def_seq);
	if(seq)
	{
		seq_sounds_t *snd;
		if(is_fast)
		{
			gm->up_seq = &seq->fast_open;
			gm->dn_seq = &seq->fast_close;
		} else
		{
			gm->up_seq = &seq->norm_open;
			gm->dn_seq = &seq->norm_close;
		}
		snd = dir == DIR_UP ? gm->up_seq : gm->dn_seq;
		if(snd->start && !map_skip_stuff)
			S_StartSound((mobj_t*)&gm->sector->soundorg, snd->start);
		gm->sndwait = snd->delay;
	}

	return gm;
}

generic_mover_t *generic_floor_by_sector(sector_t *sec)
{
	for(thinker_t *th = thcap.next; th != &thcap; th = th->next)
	{
		generic_mover_t *gm;

		if(th->function != think_floor)
			continue;

		gm = (generic_mover_t*)th;
		if(gm->sector == sec)
			return gm;
	}

	return NULL;
}

//
// API - light

generic_light_t *generic_light(sector_t *sec)
{
	generic_light_t *gl;

	if(sec->specialactive & ACT_LIGHT)
		return NULL;
	sec->specialactive |= ACT_LIGHT;

	gl = Z_Malloc(sizeof(generic_mover_t), PU_LEVELSPEC, NULL);
	memset(gl, 0, sizeof(generic_mover_t));
	gl->thinker.function = think_light;
	gl->sector = sec;
	gl->level = (sec->lightlevel & 0x1FF) << FRACBITS;
	think_add(&gl->thinker);

	return gl;
}

generic_light_t *generic_light_by_sector(sector_t *sec)
{
	for(thinker_t *th = thcap.next; th != &thcap; th = th->next)
	{
		generic_light_t *gl;

		if(th->function != think_light)
			continue;

		gl = (generic_light_t*)th;
		if(gl->sector == sec)
			return gl;
	}

	return NULL;
}

