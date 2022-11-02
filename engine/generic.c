// kgsws' ACE Engine
////
// Generic floor / ceiling mover.
// Replaces all original types. Used only in Hexen map format.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "sound.h"
#include "think.h"
#include "generic.h"

//
// thinkers

static __attribute((regparm(2),no_caller_saved_registers))
void think_ceiling(generic_mover_t *gm)
{
	sector_t *sec;

	if(gm->sndwait)
		gm->sndwait--;

	if(gm->wait)
	{
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
		sec->ceilingheight += gm->speed;
		if(sec->ceilingheight >= gm->top_height)
		{
			sec->ceilingheight = gm->top_height;
			if(gm->up_seq)
			{
				if(gm->up_seq->stop)
					S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->stop);
				gm->sndwait = gm->up_seq->delay;
			}
			if(gm->flags & MVF_TOP_REVERSE)
			{
				gm->direction = !gm->direction;
				gm->wait = gm->delay;
				return;
			}
			goto finish_move;
		} else
		if(!gm->sndwait && gm->up_seq && gm->up_seq->move)
		{
			S_StartSound((mobj_t*)&gm->sector->soundorg, gm->up_seq->move);
			gm->sndwait = gm->up_seq->repeat;
		}
	} else
	{
		sec->ceilingheight -= gm->speed;
		if(sec->ceilingheight <= gm->bot_height)
		{
			sec->ceilingheight = gm->bot_height;
			if(gm->dn_seq)
			{
				if(gm->dn_seq->stop)
					S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->stop);
				gm->sndwait = gm->dn_seq->delay;
			}
			if(gm->flags & MVF_BOT_REVERSE)
			{
				gm->direction = !gm->direction;
				gm->wait = gm->delay;
				return;
			}
			goto finish_move;
		} else
		if(!gm->sndwait && gm->dn_seq && gm->dn_seq->move)
		{
			S_StartSound((mobj_t*)&gm->sector->soundorg, gm->dn_seq->move);
			gm->sndwait = gm->dn_seq->repeat;
		}
	}

	return;

finish_move:
	gm->thinker.function = (void*)-1;
	sec->specialactive &= ~ACT_CEILING;
}

//
// API

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

