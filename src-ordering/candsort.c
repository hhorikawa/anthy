/*
 * ʸ����Ф������򥽡��Ȥ��롣
 * ����Ū�ˤ϶��ܤ���ʸ��⸫�ơ�ñ��η��ˤ��ɾ���򤹤롣
 * ���֤ä�����κ���⤹�롣
 *
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2001 9/22
 * Copyright (C) 2000-2003 TABATA Yusuke
 * Copyright (C) 2001 UGAWA Tomoharu
 *
 * $Id: candsort.c,v 1.27 2002/11/17 14:45:47 yusuke Exp $
 *
 */

#include <stdlib.h>
#include <segment.h>
#include <splitter.h>
#include "sorter.h"
#include "ordering.h"
#include <stdio.h>
#include <limits.h>

/* ��������ؽ��ˤ����� */
#define OCHAIRE_BASE OCHAIRE_SCORE
/* metaword����ʬ̵�������������Ȥ��Ρ��Ҥ餬�ʥ������ʤΥ����� */
#define NOCONV_WITH_BIAS 900000
/* ���̤θ��� */
#define NORMAL_BASE 100
/* ñ���� */
#define SINGLEWORD_BASE 10
/* ʣ��� */
#define COMPOUND_BASE (OCHAIRE_SCORE / 2)
/* ʣ���ΰ���ʬ���ʸ��ˤ������ */
#define COMPOUND_PART_BASE 2
/* ��°��Τ� */
#define DEPWORD_BASE (OCHAIRE_SCORE / 2)
/* �Ҥ餬�ʥ������ʤΥǥե���ȤΥ����� */
#define NOCONV_BASE 1

/* ̵���äݤ����������Ƥ�Ƚ�Ǥ��� */
static int
uncertain_segment_p(struct seg_ent *se)
{
  struct meta_word *mw;
  if (se->nr_metaword == 0) {
    return 0;
  }

  mw = se->mw_array[0];

  /* Ĺ����6�� */
  if (se->len * 3 >= mw->len * 5) {
    return 1;
  }
  return 0;
}

static void
release_redundant_candidate(struct seg_ent *se)
{
  int i, j;
  /* ����ϥ����Ȥ���Ƥ���Τ�score��0�θ��䤬�����¤�Ǥ��� */
  for (i = 0; i < se->nr_cands && se->cands[i]->score; i++);
  /* i������θ������� */
  if (i < se->nr_cands) {
    for (j = i; j < se->nr_cands; j++) {
      anthy_release_cand_ent(se->cands[j]);
    }
    se->nr_cands = i;
  }
}

/* qsort�Ѥθ�����Ӵؿ� */
static int
candidate_compare_func(const void *p1, const void *p2)
{
  const struct cand_ent *const *c1 = p1, *const *c2 = p2;
  return (*c2)->score - (*c1)->score;
}

static void
sort_segment(struct seg_ent *se)
{
  qsort(se->cands, se->nr_cands,
	sizeof(struct cand_ent *),
	candidate_compare_func);
}

static void
trim_kana_candidate(struct seg_ent *se)
{
  int i;
  if (se->cands[0]->flag & CEF_KATAKANA) {
    return ;
  }
  for (i = 1; i < se->nr_cands; i++) {
    if (se->cands[i]->flag & CEF_KATAKANA) {
      /* �������ޤǲ����� */
      se->cands[i]->score = NOCONV_BASE;
    }
  }
}

static void
check_dupl_candidate(struct seg_ent *se)
{
  int i,j;
  for (i = 0; i < se->nr_cands - 1; i++) {
    for (j = i + 1; j < se->nr_cands; j++) {
      if (!anthy_xstrcmp(&se->cands[i]->str, &se->cands[j]->str)) {
	/* �롼����ɤ��ޥå�������Τ��������֤Ȥ����٤� */
	se->cands[j]->score = 0;
	se->cands[i]->flag |= se->cands[j]->flag;
      }
    }
  }
}

/* �ʻ������Ƥˤ�ä��������줿�����ɾ������ */
static void
eval_candidate_by_metaword(struct seg_ent *seg,
			   struct cand_ent *ce)
{
  int i;
  int score = 1;
  int unassigned_len = 0;
  int assigned_len;

  /* �ޤ���ñ������٤ˤ��score��û� */
  for (i = 0; i < ce->nr_words; i++) {
    struct cand_elm *elm = &ce->elm[i];
    int one_score;
    int pos, div = 1;

    if (elm->nth < 0) {
      /* ���������Ƥ��оݳ��ʤΤǥ����å� */
      unassigned_len += elm->str.len;
      continue;
    }
    pos = anthy_wtype_get_pos(elm->wt);
    if (pos == POS_PRE || pos == POS_SUC) {
      div = 8;
    }
    /*
      ���٤�elm->ratio���ؤ������ط��ʤΤ�ξ�����Ѥ���Τ���Ŭ����
     */
    one_score = anthy_get_nth_dic_ent_freq(elm->se, elm->nth) * elm->ratio / (RATIO_BASE * div);
    score += INT_MAX / 256 / (one_score ? one_score : 1);
  }
  score = INT_MAX / 256 / score * ce->nr_words;

  if (ce->mw) {
    /* �⤷����ʸ��ǰ���ɾ�����⤫�ä��ʻ�ȸ�����ʻ줬���פ��Ƥ�������� */
    if (ce->mw->seg_class == seg->best_seg_class) {
      if (anthy_seg_class_is_depword(seg->best_seg_class)) {
	score = DEPWORD_BASE;
      } else {
	score *= 5;
      }
    }
  }
  /* ��Ω�����γ�礤���Ф���Ĵ�� */
  assigned_len = seg->len - unassigned_len;
  score = score * (assigned_len + 1) * (assigned_len + 1)
      /((seg->len + 1)*(seg->len + 1));
  ce->score = score;
}

/* �����ɾ������ */
static void
eval_candidate(struct seg_ent *seg, struct cand_ent *ce, int uncertain)
{
  if ((ce->flag &
       (CEF_OCHAIRE | CEF_SINGLEWORD | CEF_HIRAGANA |
	CEF_KATAKANA | CEF_GUESS | CEF_COMPOUND | CEF_COMPOUND_PART |
	CEF_BEST)) == 0) {
    /* splitter����ξ���(metaword)�ˤ�ä��������줿���� */
    eval_candidate_by_metaword(seg, ce);
  } else if (ce->flag & CEF_OCHAIRE) {
    ce->score = OCHAIRE_BASE;
  } else if (ce->flag & CEF_SINGLEWORD) {
    ce->score = SINGLEWORD_BASE;
  } else if (ce->flag & CEF_COMPOUND) {
    ce->score = COMPOUND_BASE;
  } else if (ce->flag & CEF_COMPOUND_PART) {
    ce->score = COMPOUND_PART_BASE;
  } else if (ce->flag & CEF_BEST) {
    ce->score = OCHAIRE_BASE;
  } else if (ce->flag & (CEF_HIRAGANA | CEF_KATAKANA | CEF_GUESS)) {
    if (uncertain) {
      /*
       * ����ʸ��ϳ����ʤɤǡ����������������Ҥ餬�ʥ������ʤ�
       * �����Ф��������褤
       */
      ce->score = NOCONV_WITH_BIAS;
      if (CEF_KATAKANA & ce->flag) {
	ce->score ++;
      }
      if (CEF_GUESS & ce->flag) {
	ce->score += 2;
      }
    } else {
      ce->score = NOCONV_BASE;
    }
  }
  ce->score += 1;
}

static void
eval_segment(struct seg_ent *se)
{
  int i;
  int uncertain = uncertain_segment_p(se);
  for (i = 0; i < se->nr_cands; i++) {
    eval_candidate(se, se->cands[i], uncertain);
  }
}

/** ������ƤФ�륨��ȥ�ݥ���� */
void
anthy_sort_candidate(struct segment_list *sl, int nth)
{
  int i;
  /* �ޤ�ɾ������ */
  for (i = nth; i < sl->nr_segments; i++) {
    eval_segment(anthy_get_nth_segment(sl, i));
  }
  /* �Ĥ��˥����Ȥ��� */
  for (i = nth; i < sl->nr_segments; i++) {
    sort_segment(anthy_get_nth_segment(sl, i));
  }
  /* ���֤ä�����ȥ�������㤤����0�����դ��� */
  for (i = nth; i < sl->nr_segments; i++) {
    check_dupl_candidate(anthy_get_nth_segment(sl, i));
  }
  /* �⤦�����ɥ����Ȥ��� */
  for ( i = nth ; i < sl->nr_segments ; i++){
    sort_segment(anthy_get_nth_segment(sl, i));
  }
  /* ɾ��0�θ������� */
  for (i = nth ;i < sl->nr_segments ; i++) {
    release_redundant_candidate(anthy_get_nth_segment(sl, i));
  }
  /**/
  anthy_reorder_candidates_by_relation(sl, nth);
  /* ����ˤ�������ѹ� */
  for (i = nth; i < sl->nr_segments; i++) {
    anthy_reorder_candidates_by_history(anthy_get_nth_segment(sl, i));
  }
  /* ����θ� */
  for (i = nth; i < sl->nr_segments; i++) {
    anthy_proc_swap_candidate(anthy_get_nth_segment(sl, i));
  }
  /* �ޤ������Ȥ��� */
  for ( i = nth ; i < sl->nr_segments ; i++){
    sort_segment(anthy_get_nth_segment(sl, i));
  }
  /* �������ʤθ��䤬��Ƭ�Ǥʤ���кǸ�˲� */
  for (i = nth; i < sl->nr_segments; i++) {
    trim_kana_candidate(anthy_get_nth_segment(sl, i));
  }
  /* �ޤ������Ȥ��� */
  for ( i = nth ; i < sl->nr_segments ; i++){
    sort_segment(anthy_get_nth_segment(sl, i));
  }
}
