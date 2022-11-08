// kgsws' ACE Engine
////
// Generic floor / ceiling mover.
// Replaces all original types. Used only in Hexen map format.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "sound.h"
#include "extra3d.h"
#include "think.h"
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
// thinkers

static __attribute((regparm(2),no_caller_saved_registers))
void think_ceiling(generic_mover_t *gm)
{
	sector_t *sec;
	uint32_t blocked;
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
	original = sec->ceilingheight;

	if(gm->direction == DIR_UP)
	{
		sec->ceilingheight += gm->speed_now;
		if(sec->ceilingheight > gm->top_height)
			sec->ceilingheight = gm->top_height;

		if(!gm->sndwait && gm->up_seq && gm->up_seq->move)
		{
			S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->move);
			gm->sndwait = gm->up_seq->repeat;
		}

		blocked = P_ChangeSector(sec, gm->flags & MVF_CRUSH);
		if(blocked && sec->e3d_origin)
		{
			if(gm->flags & MVF_BLOCK_STAY)
			{
				sec->ceilingheight = original;
				P_ChangeSector(sec, 0);
				return;
			}
		}

		if(sec->ceilingheight >= gm->top_height)
		{
			if(sec->e3d_origin)
				e3d_update_top(sec);

			if(gm->lighttag && !(gm->flags & MVF_SET_SPECIAL))
				light_effect(gm->lighttag, 0x10000);

			if(gm->up_seq && gm->up_seq->stop)
					S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->stop);

			if(gm->flags & MVF_TOP_REVERSE)
			{
				gm->direction = !gm->direction;
				gm->wait = gm->delay;
				gm->speed_now = gm->speed_start;
				return;
			}
			goto finish_move;
		}
	} else
	{
		sec->ceilingheight -= gm->speed_now;
		if(sec->ceilingheight < gm->bot_height)
			sec->ceilingheight = gm->bot_height;

		if(!gm->sndwait && gm->dn_seq && gm->dn_seq->move)
		{
			S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->move);
			gm->sndwait = gm->dn_seq->repeat;
		}

		no_extra_step = 1;
		blocked = P_ChangeSector(sec, gm->flags & MVF_CRUSH);
		if(blocked && !sec->e3d_origin)
		{
			if(gm->flags & MVF_BLOCK_GO_UP)
			{
				gm->direction = DIR_UP;
				if(gm->up_seq)
				{
					if(gm->up_seq->start)
						S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->start);
					gm->sndwait = gm->up_seq->delay;
				}
			}

			if(gm->flags & MVF_BLOCK_SLOW)
				gm->speed_now = FRACUNIT;

			if(gm->flags & (MVF_BLOCK_STAY | MVF_BLOCK_GO_UP))
			{
				sec->ceilingheight = original;
				P_ChangeSector(sec, 0);
				no_extra_step = 0;
				return;
			}
		}
		no_extra_step = 0;

		if(sec->ceilingheight <= gm->bot_height)
		{
			if(sec->e3d_origin)
				e3d_update_top(sec);

			if(gm->lighttag && !(gm->flags & MVF_SET_SPECIAL))
				light_effect(gm->lighttag, 0);

			if(gm->dn_seq && gm->dn_seq->stop)
				S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->stop);

			if(gm->flags & MVF_BOT_REVERSE)
			{
				gm->direction = !gm->direction;
				gm->wait = gm->delay;
				gm->speed_now = gm->speed_start;
				return;
			}
			goto finish_move;
		}
	}

	if(sec->e3d_origin)
		e3d_update_top(sec);

	if(gm->lighttag && !(gm->flags & MVF_SET_SPECIAL))
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

static __attribute((regparm(2),no_caller_saved_registers))
void think_floor(generic_mover_t *gm)
{
	sector_t *sec;
	uint32_t blocked;
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
		sec->floorheight += gm->speed_now;
		if(sec->floorheight > gm->top_height)
			sec->floorheight = gm->top_height;

		if(!gm->sndwait && gm->up_seq && gm->up_seq->move)
		{
			S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->move);
			gm->sndwait = gm->up_seq->repeat;
		}

		blocked = P_ChangeSector(sec, gm->flags & MVF_CRUSH);
		if(blocked && !sec->e3d_origin)
		{
			if(gm->flags & MVF_BLOCK_GO_DN)
			{
				gm->direction = DIR_DOWN;
				if(gm->dn_seq)
				{
					if(gm->dn_seq->start)
						S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->start);
					gm->sndwait = gm->dn_seq->delay;
				}
			}

			if(gm->flags & MVF_BLOCK_SLOW)
				gm->speed_now = FRACUNIT;

			if(gm->flags & (MVF_BLOCK_STAY | MVF_BLOCK_GO_DN))
			{
				sec->floorheight = original;
				P_ChangeSector(sec, 0);
				return;
			}
		}

		if(sec->floorheight >= gm->top_height)
		{
			if(sec->e3d_origin)
				e3d_update_bot(sec);

			if(gm->up_seq && gm->up_seq->stop)
					S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->stop);

			if(gm->flags & MVF_TOP_REVERSE)
			{
				gm->direction = !gm->direction;
				gm->wait = gm->delay;
				gm->speed_now = gm->speed_start;
				return;
			}
			goto finish_move;
		}
	} else
	{
		sec->floorheight -= gm->speed_now;
		if(sec->floorheight < gm->bot_height)
			sec->floorheight = gm->bot_height;

		if(!gm->sndwait && gm->dn_seq && gm->dn_seq->move)
		{
			S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->move);
			gm->sndwait = gm->dn_seq->repeat;
		}

		no_extra_step = 1;
		blocked = P_ChangeSector(sec, gm->flags & MVF_CRUSH);
		if(blocked && sec->e3d_origin)
		{
			if(gm->flags & MVF_BLOCK_STAY)
			{
				sec->floorheight = original;
				P_ChangeSector(sec, 0);
				no_extra_step = 0;
				return;
			}
		}
		no_extra_step = 0;

		if(sec->floorheight <= gm->bot_height)
		{
			if(sec->e3d_origin)
				e3d_update_bot(sec);

			if(gm->dn_seq && gm->dn_seq->stop)
				S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->stop);

			if(gm->flags & MVF_BOT_REVERSE)
			{
				gm->direction = !gm->direction;
				gm->wait = gm->delay;
				gm->speed_now = gm->speed_start;
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

static __attribute((regparm(2),no_caller_saved_registers))
void think_dual(generic_mover_t *gm)
{
	sector_t *sec;
	uint32_t blocked;

	if(gm->sndwait)
		gm->sndwait--;

	sec = gm->sector;

	if(gm->direction == DIR_UP)
	{
		sec->floorheight += gm->speed_now;
		sec->ceilingheight = sec->floorheight + gm->gap_height;

		if(!gm->sndwait && gm->up_seq && gm->up_seq->move)
		{
			S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->move);
			gm->sndwait = gm->up_seq->repeat;
		}

		blocked = P_ChangeSector(sec, gm->flags & MVF_CRUSH);
		if(blocked)
		{
			sec->floorheight -= gm->speed_now;
			sec->ceilingheight = sec->floorheight + gm->gap_height;
			P_ChangeSector(sec, 0);
			return;
		}

		if(sec->floorheight >= gm->top_height)
		{
			sec->floorheight = gm->top_height;
			sec->ceilingheight = sec->floorheight + gm->gap_height;

			if(sec->e3d_origin)
			{
				e3d_update_bot(sec);
				e3d_update_top(sec);
			}

			if(gm->up_seq && gm->up_seq->stop)
					S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->stop);

			goto finish_move;
		}
	} else
	{
		sec->floorheight -= gm->speed_now;
		sec->ceilingheight = sec->floorheight + gm->gap_height;

		if(!gm->sndwait && gm->dn_seq && gm->dn_seq->move)
		{
			S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->move);
			gm->sndwait = gm->dn_seq->repeat;
		}

		no_extra_step = 1;
		blocked = P_ChangeSector(sec, gm->flags & MVF_CRUSH);
		if(blocked)
		{
			sec->floorheight += gm->speed_now;
			sec->ceilingheight = sec->floorheight + gm->gap_height;
			P_ChangeSector(sec, 0);
			no_extra_step = 0;
			return;
		}
		no_extra_step = 0;

		if(sec->floorheight <= gm->bot_height)
		{
			sec->floorheight = gm->bot_height;
			sec->ceilingheight = sec->floorheight + gm->gap_height;

			if(sec->e3d_origin)
			{
				e3d_update_bot(sec);
				e3d_update_top(sec);
			}

			if(gm->dn_seq && gm->dn_seq->stop)
				S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->stop);

			goto finish_move;
		}
	}

	if(sec->e3d_origin)
	{
		e3d_update_bot(sec);
		e3d_update_top(sec);
	}

	return;

finish_move:
	gm->thinker.function = (void*)-1;
	sec->specialactive &= ~(ACT_FLOOR|ACT_CEILING);
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
		if(snd->start)
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
		if(snd->start)
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
// API - both

generic_mover_t *generic_dual(sector_t *sec, uint32_t dir, uint32_t def_seq, uint32_t is_fast)
{
	generic_mover_t *gm;
	sound_seq_t *seq;

	if(sec->specialactive & (ACT_FLOOR|ACT_CEILING))
		return NULL;
	sec->specialactive |= ACT_FLOOR | ACT_CEILING;

	gm = Z_Malloc(sizeof(generic_mover_t), PU_LEVELSPEC, NULL);
	memset(gm, 0, sizeof(generic_mover_t));
	gm->thinker.function = think_dual;
	gm->sector = sec;
	gm->direction = dir;
	think_add(&gm->thinker);

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
		if(snd->start)
			S_StartSound((mobj_t*)&gm->sector->soundorg, snd->start);
		gm->sndwait = snd->delay;
	}

	return gm;
}

