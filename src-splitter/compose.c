/*
 * ʸ����Ф��Ƹ���Υꥹ�Ȥ��������롣
 * make_candidates()��context����������ƤФ�롣
 *
 * ����������ϼ�����ˡ�ǹԤ�
 * (1)splitter��������Ƥ��ʻ���Ф���proc_splitter_info()
 *    ����������������
 * (2)�Ҥ餬�ʤΤߤȥ������ʤΤߤθ������������
 * (3)�Ǹ��ʸ�������Ȳ�ᤷ��̵�������������������
 */
/*
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2001 9/30
 * Copyright (C) 2000-2005 TABATA Yusuke
 * Copyright (C) 2004-2005 YOSHIDA Yuichi
 * Copyright (C) 2002 UGAWA Tomoharu
 *
 * $Id: compose.c,v 1.21 2005/05/15 04:15:59 yusuke Exp $
 */
/*
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <anthy.h> /* for ANTHY_*_ENCODING */
#include <conf.h>
#include <dic.h>
#include <splitter.h>
#include <segment.h>
#include "wordborder.h"


/* ͹���ֹ��index���� atoi�Ǿ�̤η夬����Ƥ���Τ�
 * 0���ɲä���3��⤷����7���index���������
 */
static void
make_zipcode_index(long long num, char *buf)
{
  const char *fmt = "";
  if (num < 10) {
    fmt = "00%d ";
  } else if (num < 100) {
    fmt = "0%d ";
  } else if (num < 1000) {
    fmt = "%d ";
  } else if (num < 10000) {
    fmt = "000%d ";
  } else if (num < 100000) {
    fmt = "00%d ";
  } else if (num < 1000000) {
    fmt = "0%d ";
  } else {
    fmt = "%d ";
  }
  sprintf(buf, fmt, (int)num);
}

/* ͹���ֹ漭�񤫤�õ�� */
static xstr *
search_zipcode_dict(xstr *xs, long long num)
{
  FILE *fp;
  char buf[1000];
  char index[30];
  int len;

  if (xs->len != 3 && xs->len != 7) {
    return NULL;
  }

  if (num > 9999999 || num < 1) {
    return NULL;
  }
  fp = fopen(anthy_conf_get_str("ZIPDICT_EUC"), "r");
  if (!fp) {
    return NULL;
  }
  make_zipcode_index(num, index);
  len = strlen(index);
  while (fgets(buf, 1000, fp)) {
    if (!strncmp(buf, index, len)) {
      char *tmp;
      /* ���Ԥ�ä� */
      buf[strlen(buf) - 1] = 0;
      /* ���ֺǸ�Υ��ڡ����Τ��Ȥ���̾�Ȥ��ƤȤ����(<=�ޤ���)*/
      tmp = strrchr(buf, ' ');
      tmp ++;
      fclose(fp);
      return anthy_cstr_to_xstr(tmp, ANTHY_EUC_JP_ENCODING);
    }
  }
  fclose(fp);
  return NULL;
}

static struct cand_ent *
alloc_cand_ent(void)
{
  struct cand_ent *ce;
  ce = (struct cand_ent *)malloc(sizeof(struct cand_ent));
  ce->nr_words = 0;
  ce->elm = NULL;
  ce->mw = NULL;
  ce->core_elm_index = -1;
  return ce;
}

/*
 * �����ʣ������
 */
static struct cand_ent *
dup_candidate(struct cand_ent *ce)
{
  struct cand_ent *ce_new;
  int i;
  ce_new = alloc_cand_ent();
  ce_new->nr_words = ce->nr_words;
  ce_new->str.len = ce->str.len;
  ce_new->str.str = anthy_xstr_dup_str(&ce->str);
  ce_new->elm = malloc(sizeof(struct cand_elm)*ce->nr_words);
  ce_new->flag = ce->flag;
  ce_new->core_elm_index = ce->core_elm_index;
  ce_new->mw = ce->mw;
  ce_new->score = ce->score;

  for (i = 0 ; i < ce->nr_words ; i++) {
    ce_new->elm[i] = ce->elm[i];
  }
  return ce_new;
}

/** ʸ��˸�����ɲä��� */
static void
push_back_candidate(struct seg_ent *seg, struct cand_ent *ce)
{
  /* seg_ent�˸���ce���ɲ� */
  seg->nr_cands++;
  seg->cands = (struct cand_ent **)
    realloc(seg->cands, sizeof(struct cand_ent *) * seg->nr_cands);
  seg->cands[seg->nr_cands - 1] = ce;
}


static void
push_back_zipcode_candidate(struct seg_ent *seg)
{
  struct cand_ent *ce;
  long long code;
  xstr *str;

  code = anthy_xstrtoll(&seg->str);
  if (code == -1) {
    return ;
  }
  str = search_zipcode_dict(&seg->str, code);
  if (!str) {
    return ;
  }

  ce = alloc_cand_ent();
  ce->str = *str;
  ce->flag = CEF_SINGLEWORD;
  push_back_candidate(seg, ce);
  free(str);
}

static void
push_back_guessed_candidate(struct seg_ent *seg)
{
  xchar xc;
  xstr *xs;
  struct cand_ent *ce;
  if (seg->str.len < 2) {
    return ;
  }
  /* �Ǹ��ʸ���Ͻ��줫�� */
  xc = seg->str.str[seg->str.len - 1];
  if (!(anthy_get_xchar_type(xc) & XCT_DEP)) {
    return ;
  }
  /* �Ǹ��ʸ���ʳ��򥫥����ʤˤ��Ƥߤ� */
  ce = alloc_cand_ent();
  xs = anthy_xstr_hira_to_kata(&seg->str);
  xs->str[xs->len-1] = xc;
  ce->str.str = anthy_xstr_dup_str(xs);
  ce->str.len = xs->len;
  ce->flag = CEF_GUESS;
  anthy_free_xstr(xs);
  push_back_candidate(seg, ce);
}

/** �Ƶ���1ñ�줺�ĸ��������ƤƤ��� */
static int
enum_candidates(struct seg_ent *seg,
		struct cand_ent *ce,
		int from, int n)
{
  int i, p;
  struct cand_ent *cand;
  int nr_cands = 0;

  if (n == ce->mw->nr_parts) {
    /* ������ */
    /* ʸ������β��Ϥ��ʤ��ä���ʬ�����ʸ������ɲ� */
    xstr tail;
    tail.len = seg->len - from;
    tail.str = &seg->str.str[from];
    anthy_xstrcat(&ce->str, &tail);
    push_back_candidate(seg, dup_candidate(ce));
    return 1;
  }

  p = anthy_get_nr_dic_ents(ce->elm[n].se, &ce->elm[n].str);
  /* �ʻ�����ξ��ˤ�̤�Ѵ��Ǽ���ñ��عԤ� */
  if (anthy_wtype_get_pos(ce->elm[n].wt) == POS_INVAL ||
      anthy_wtype_get_pos(ce->elm[n].wt) == POS_NONE ||
      p == 0) {
    xstr xs;
    xs.len = ce->elm[n].str.len;
    xs.str = &seg->str.str[from];
    cand = dup_candidate(ce);
    cand->elm[n].nth = -1;
    cand->elm[n].id = -1;
    anthy_xstrcat(&cand->str, &xs);
    nr_cands = enum_candidates(seg,cand,
			       from + xs.len,
			       n + 1);
    anthy_release_cand_ent(cand);
    return nr_cands;
  }

  /* �ʻ줬�����Ƥ��Ƥ���Τǡ������ʻ�˥ޥå������Τ�����Ƥ� */
  for (i = 0; i < p; i++) {
    wtype_t wt;
    anthy_get_nth_dic_ent_wtype(ce->elm[n].se, &ce->str, i, &wt);
    anthy_wtype_set_ct(&ce->elm[n].wt, CT_NONE);
    if (anthy_wtypecmp(ce->elm[n].wt, wt)) {
      xstr word, yomi;
      yomi.len = ce->elm[n].str.len;
      yomi.str = &seg->str.str[from];
      cand = dup_candidate(ce);
      anthy_get_nth_dic_ent_str(cand->elm[n].se,
				&yomi, i, &word);
      cand->elm[n].nth = i;
      cand->elm[n].id = anthy_get_nth_dic_ent_id(ce->elm[n].se, i);

      /* ñ������� */
      anthy_xstrcat(&cand->str, &word);
      free(word.str);
      /* ��ʬ��Ƶ��ƤӽФ�����³�����ꤢ�Ƥ� */
      nr_cands += enum_candidates(seg, cand, 
				  from + yomi.len,
				  n+1);
      anthy_release_cand_ent(cand);
    }
  }
  return nr_cands;
}

/**
 * ʸ�����Τ�ޤ��ñ��(ñ������ޤ�)�θ������������
 */
static void
push_back_singleword_candidate(struct seg_ent *seg)
{
  seq_ent_t se;
  struct cand_ent *ce;
  wtype_t wt;
  int i, n;
  xstr xs;

  se = anthy_get_seq_ent_from_xstr(&seg->str);
  n = anthy_get_nr_dic_ents(se, &seg->str);
  /* ����γƥ���ȥ���Ф��� */
  for (i = 0; i < n; i++) {
    int ct;
    /* �ʻ����Ф��� */
    anthy_get_nth_dic_ent_wtype(se, &seg->str, i, &wt);
    ct = anthy_wtype_get_ct(wt);
    /* ���߷������Ѥ��ʤ���Τθ����ʤ� */
    if (ct == CT_SYUSI || ct == CT_NONE) {
      ce = alloc_cand_ent();
      anthy_get_nth_dic_ent_str(se,&seg->str, i, &xs);
      ce->str.str = xs.str;
      ce->str.len = xs.len;
      ce->flag = CEF_SINGLEWORD;
      push_back_candidate(seg, ce);
    }
  }
}

static void
push_back_noconv_candidate(struct seg_ent *seg)
{
  /* ̵�Ѵ����Ҳ�̾�ˤʤ�����ʿ��̾�Τߤˤʤ������ɲ� */
  struct cand_ent *ce;
  xstr *xs;

  /* �Ҥ餬�ʤΤ� */
  ce = alloc_cand_ent();
  ce->str.str = anthy_xstr_dup_str(&seg->str);
  ce->str.len = seg->str.len;
  ce->flag = CEF_HIRAGANA;
  push_back_candidate(seg, ce);

  /* ���˥������� */
  ce = alloc_cand_ent();
  xs = anthy_xstr_hira_to_kata(&seg->str);
  ce->str.str = anthy_xstr_dup_str(xs);
  ce->str.len = xs->len;
  ce->flag = CEF_KATAKANA;
  anthy_free_xstr(xs);
  push_back_candidate(seg, ce);
}

static void
make_cand_elem_from_word_list(struct seg_ent *se,
			      struct cand_ent *ce,
			      struct word_list *wl,
			      int index)
{
  int i; 
  int from = wl->from - se->from;
  for (i = 0; i < NR_PARTS; ++i) {
    struct part_info *part = &wl->part[i];
    xstr core_xs;
    if (part->len == 0) {
      /* Ĺ����̵��part��̵�뤹�� */
      continue;
    }
    if (i == PART_CORE) {
      ce->core_elm_index = i + index;
    }
    core_xs.str = &se->str.str[from];
    core_xs.len = part->len;
    ce->elm[i + index].se = anthy_get_seq_ent_from_xstr(&core_xs);
    ce->elm[i + index].str.str = core_xs.str;
    ce->elm[i + index].str.len = core_xs.len;
    ce->elm[i + index].wt = part->wt;
    ce->elm[i + index].ratio = part->ratio * (wl->len - wl->weak_len);
    from += part->len;
  }
}


/** �ޤ�wordlist�����metaword����meta_word����Ф� */
static void
make_candidate_from_simple_metaword(struct seg_ent *se,
				    struct meta_word *mw,
				    struct meta_word *top_mw)
{
  /*
   * ��ñ����ʻ줬���ꤵ�줿���֤ǥ��ߥåȤ���롣
   */
  struct cand_ent *ce;
  struct word_list *wl = mw->wl;

  /* ʣ��(1��ޤ�)��ñ��ǹ��������ʸ���ñ�������ƤƤ��� */
  ce = alloc_cand_ent();
  ce->nr_words = mw->nr_parts;
  ce->str.str = NULL;
  ce->str.len = 0;
  ce->elm = calloc(sizeof(struct cand_elm),ce->nr_words);
  ce->mw = top_mw;
  ce->score = 0;

  /* ��Ƭ��, ��Ω����, ������, ��°�� */
  make_cand_elem_from_word_list(se, ce, wl, 0);

  ce->flag = (se->best_mw == mw) ? CEF_BEST : CEF_NONE;
  enum_candidates(se, ce, 0, 0);
  anthy_release_cand_ent(ce);
}

/** combined��metaword����Ĥθ����Τ��ư�Ĥθ�Ȥ��ƽФ� */
static void
make_candidate_from_combined_metaword(struct seg_ent *se,
				      struct meta_word *mw,
				      struct meta_word *top_mw)
{
  /*
   * ��ñ����ʻ줬���ꤵ�줿���֤ǥ��ߥåȤ���롣
   */
  struct cand_ent *ce;
  int score;

  /* ʣ��(1��ޤ�)��ñ��ǹ��������ʸ���ñ�������ƤƤ��� */
  ce = alloc_cand_ent();
  ce->nr_words = mw->nr_parts;
  ce->str.str = NULL;
  ce->str.len = 0;
  ce->elm = calloc(sizeof(struct cand_elm),ce->nr_words);
  ce->mw = top_mw;

  /* ��Ƭ��, ��Ω����, ������, ��°�� */
  make_cand_elem_from_word_list(se, ce, mw->mw1->wl, 0);
  if (mw->mw2) {
    make_cand_elem_from_word_list(se, ce, mw->mw2->mw1->wl, NR_PARTS);
  }
  /* ��¤��ɾ������ */
  score = mw->score;

  ce->score = score;
  ce->flag = (se->best_mw == mw) ? CEF_BEST : CEF_NONE;
  enum_candidates(se, ce, 0, 0);
  anthy_release_cand_ent(ce);
}


/** splitter�ξ�������Ѥ��Ƹ������������
 */
static void
proc_splitter_info(struct seg_ent *se,
		   struct meta_word *mw,
		   struct meta_word *top_mw)
{
  enum mw_status st;
  if (!mw) return;

  /* �ޤ�wordlist�����metaword�ξ�� */
  if (mw->wl && mw->wl->len) {
    make_candidate_from_simple_metaword(se, mw, top_mw);
    return;
  }
  
  st = anthy_metaword_type_tab[mw->type].status;
  switch (st) {
  case MW_STATUS_WRAPPED:
    /* wrap���줿��Τξ������Ф� */
    proc_splitter_info(se, mw->mw1, top_mw);
    break;
  case MW_STATUS_COMBINED:
    make_candidate_from_combined_metaword(se, mw, top_mw);
    break;
  case MW_STATUS_COMPOUND:
    /* Ϣʸ����� */
    {
      struct cand_ent *ce;
      ce = alloc_cand_ent();
      ce->str.str = anthy_xstr_dup_str(&mw->cand_hint);
      ce->str.len = mw->cand_hint.len;
      ce->flag = CEF_COMPOUND;
      ce->mw = top_mw;
      push_back_candidate(se, ce);
    }
    break;
  case MW_STATUS_COMPOUND_PART:
    /* Ϣʸ��θġ���ʸ����礷�ư�Ĥ�ʸ��Ȥ��Ƥߤ���� */
    /* BREAK THROUGH */
  case MW_STATUS_OCHAIRE:
    {
    /* metaword������ʤ� */
      /* ����ʸ���󤬥����쥯�Ȥ˻��ꤵ�줿 */
      {
	struct cand_ent *ce;
	ce = alloc_cand_ent();
	ce->str.str = anthy_xstr_dup_str(&mw->cand_hint);
	ce->str.len = mw->cand_hint.len;
	ce->mw = top_mw;
	ce->flag = (st == MW_STATUS_OCHAIRE) ? CEF_OCHAIRE : CEF_COMPOUND_PART;

	if (mw->len < se->len) {
	  /* metaword�ǥ��С�����Ƥʤ��ΰ��ʸ������դ��� */
	  xstr xs;
	  xs.str = &se->str.str[mw->len];
	  xs.len = se->len - mw->len;
	  anthy_xstrcat(&ce->str ,&xs);
	}
	push_back_candidate(se, ce);
      }
      break;
    }
  case MW_STATUS_NONE:
    break;
  default:
    break;
  }
}

/** context.c����ƽФ�����äȤ���ʪ
 * ��İʾ�θ����ɬ����������
 */
void
anthy_do_make_candidates(struct seg_ent *se)
{
  int i, limit = 0;

  /* 0���ܤ�metaword�Υ�������1/3����������β��¤ˤ��� */
  if (se->nr_metaword) {
    int best = se->mw_array[0]->score;
    if (best > RATIO_BASE) {
      best = RATIO_BASE;
    }
    limit = best / 3;
  }

  /* hmm�Ǥ�best��meta_word������������*/
  proc_splitter_info(se, se->best_mw, se->best_mw);
  /* limit����⤤score�����metaword���������������� */
  for (i = 0; i < se->nr_metaword; i++) {
    struct meta_word *mw = se->mw_array[i];
    if (mw->score > limit && mw != se->best_mw) {
      /**/
      proc_splitter_info(se, mw, mw);
    }
  }
  /* ñ�����ʤɤθ��� */
  push_back_singleword_candidate(se);
  /* ͹���ֹ� */
  push_back_zipcode_candidate(se);
  /* �Ҥ餬�ʡ��������ʤ�̵�Ѵ�����ȥ���� */
  push_back_noconv_candidate(se);

  /* ���䤬��Ĥ���̵���Ȥ��ϺǸ夬����ˤǻĤ꤬ʿ��̾�θ������뤫� */
  push_back_guessed_candidate(se);
}

