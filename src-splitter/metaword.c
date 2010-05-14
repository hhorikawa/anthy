/*
 * ʸ��⤷����ñ����İʾ奻�åȤˤ���metaword�Ȥ��ư�����
 * �����ǤϳƼ��metaword����������
 *
 * init_metaword_tab() metaword�����Τ���ξ����������
 * anthy_make_metaword_all() context���metaword��������
 * anthy_print_metaword() ���ꤵ�줿metaword��ɽ������
 *
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2001 10/29
 * Copyright (C) 2000-2004 TABATA Yusuke
 * Copyright (C) 2004 YOSHIDA Yuichi
 * Copyright (C) 2000-2003 UGAWA Tomoharu
 */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <record.h>
#include <splitter.h>
#include <xstr.h>
#include <segment.h>
#include <segclass.h>
#include "wordborder.h"

/* �Ƽ�meta_word��ɤΤ褦�˽������뤫 */
struct metaword_type_tab_ anthy_metaword_type_tab[] = {
  {MW_DUMMY,0,MW_STATUS_NONE,MW_CHECK_SINGLE},
  {MW_SINGLE,0,MW_STATUS_NONE,MW_CHECK_SINGLE},
  {MW_WRAP,0,MW_STATUS_WRAPPED,MW_CHECK_WRAP},
  {MW_COMPOUND_HEAD,0,MW_STATUS_NONE,MW_CHECK_COMPOUND},
  {MW_COMPOUND,0,MW_STATUS_NONE,MW_CHECK_NONE},
  {MW_COMPOUND_LEAF,0,MW_STATUS_COMPOUND,MW_CHECK_NONE},
  {MW_COMPOUND_PART,0,MW_STATUS_COMPOUND_PART,MW_CHECK_SINGLE},
  {MW_NAMEPAIR,0,MW_STATUS_COMBINED,MW_CHECK_PAIR},
  {MW_V_RENYOU_A,100,MW_STATUS_COMBINED,MW_CHECK_BORDER},
  {MW_V_RENYOU_NOUN,100,MW_STATUS_COMBINED,MW_CHECK_BORDER},
  {MW_NUMBER,0,MW_STATUS_COMBINED,MW_CHECK_NUMBER},
  {MW_NOUN_NOUN_PREFIX,0,MW_STATUS_COMBINED,MW_CHECK_PAIR},
  {MW_OCHAIRE,0,MW_STATUS_OCHAIRE,MW_CHECK_OCHAIRE},
  /**/
  {MW_SENTENCE,0,MW_STATUS_NONE,MW_CHECK_PAIR},
  {MW_MODIFIED,0,MW_STATUS_NONE,MW_CHECK_PAIR},
  {MW_END,0,MW_STATUS_NONE,MW_CHECK_NONE}
};

static void
combine_metaword(struct splitter_context *sc, struct meta_word *mw);

/* ����ƥ��������metaword���ɲä��� */
void
anthy_commit_meta_word(struct splitter_context *sc,
		       struct meta_word *mw)
{
  struct word_split_info_cache *info = sc->word_split_info;
  mw->score += anthy_metaword_type_tab[mw->type].score;
  /* Ʊ������������ĥΡ��ɤΥꥹ�� */
  mw->next = info->cnode[mw->from].mw;
  info->cnode[mw->from].mw = mw;
}

static void
anthy_do_print_metaword(struct splitter_context *sc,
			struct meta_word *mw,
			int indent)
{
  int i;
  for (i = 0; i < indent; i++) {
    printf(" ");
  }
  printf("*meta word type=%d(%d-%d)%d:score=%d:seg_class=%d*\n",
	 mw->type, mw->from, mw->len, mw->mw_count, mw->score, mw->seg_class);
  if (mw->wl) {
    anthy_print_word_list(sc, mw->wl);
  }
  if (mw->cand_hint.str) {
    anthy_putxstr(&mw->cand_hint);
    puts("");
  }
  if (mw->mw1) {
    anthy_do_print_metaword(sc, mw->mw1, indent + 1);
  }    
  if (mw->mw2) {
    anthy_do_print_metaword(sc, mw->mw2, indent + 1);
  }
}

void
anthy_print_metaword(struct splitter_context *sc,
		     struct meta_word *mw)
{
  anthy_do_print_metaword(sc, mw, 0);
}

static struct meta_word *
alloc_metaword(struct splitter_context *sc)
{
  struct meta_word *mw;
  mw = anthy_smalloc(sc->word_split_info->MwAllocator);
  mw->weak_len = 0;
  mw->type = MW_SINGLE;
  mw->mw_count = 1;
  mw->score = 0;
  mw->wl = NULL;
  mw->mw1 = NULL;
  mw->mw2 = NULL;
  mw->parent = NULL;
  mw->cand_hint.str = NULL;
  mw->cand_hint.len = 0;
  mw->seg_class = SEG_HEAD;
  mw->can_use = ok;
  return mw;
}


/*
 * wl����Ƭ����ʬ����������ʬ��ʸ����Ȥ��Ƽ��Ф�
 */
static void
get_surrounding_text(struct splitter_context* sc,
		     struct word_list* wl,
		     xstr* xs_pre, xstr* xs_post)
{
    int post_len = wl->part[PART_DEPWORD].len + wl->part[PART_POSTFIX].len;
    int pre_len = wl->part[PART_PREFIX].len;

    xs_pre->str = sc->ce[wl->from].c;
    xs_pre->len = pre_len;
    xs_post->str = sc->ce[wl->from + wl->len - post_len].c;
    xs_post->len = post_len;
}

/*
 * ʣ���Ǥ���wl����n�֤����ʬ����Ф���mw�ˤ���
 */
static struct meta_word*
make_compound_nth_metaword(struct splitter_context* sc, 
			   compound_ent_t ce, int nth,
			   struct word_list* wl,
			   enum metaword_type type)
{
  int i;
  int len = 0;
  int from = wl->from;
  int seg_num = anthy_compound_get_nr_segments(ce);
  struct meta_word* mw;
  xstr xs_pre, xs_core, xs_post;

  get_surrounding_text(sc, wl, &xs_pre, &xs_post);

  for (i = 0; i <= nth; ++i) {
    from += len;
    len = anthy_compound_get_nth_segment_len(ce, i);
    if (i == 0) {
      len += xs_pre.len;
    }
    if (i == seg_num - 1) {
      len += xs_post.len;
    }
  }
  
  mw = alloc_metaword(sc);
  mw->from = from;
  mw->len = len;
  mw->type = type;
  mw->score = wl->score;
  mw->seg_class = wl->seg_class;

  anthy_compound_get_nth_segment_xstr(ce, nth, &xs_core);
  if (nth == 0) {
    anthy_xstrcat(&mw->cand_hint, &xs_pre);
  }
  anthy_xstrcat(&mw->cand_hint, &xs_core);
  if (nth == seg_num - 1) {
    anthy_xstrcat(&mw->cand_hint, &xs_post);
  }
  return mw;
}

/*
 * ʣ����Ѥ�meta_word��������롣
 */
static void
make_compound_metaword(struct splitter_context* sc, struct word_list* wl)
{
  int i, j;
  seq_ent_t se = wl->part[PART_CORE].seq;
  int ent_num = anthy_get_nr_compound_ents(se);

  for (i = 0; i < ent_num; ++i) {
    compound_ent_t ce = anthy_get_nth_compound_ent(se, i);
    int seg_num = anthy_compound_get_nr_segments(ce);
    struct meta_word *mw = NULL;
    struct meta_word *mw2 = NULL;

    for (j = seg_num - 1; j >= 0; --j) {
      enum metaword_type type;
      mw = make_compound_nth_metaword(sc, ce, j, wl, MW_COMPOUND_LEAF);
      anthy_commit_meta_word(sc, mw);

      type = j == 0 ? MW_COMPOUND_HEAD : MW_COMPOUND;
      mw2 = anthy_do_cons_metaword(sc, type, mw, mw2, 0);
    }
  }
}

/*
 * ʣ������θġ���ʸ����礷��meta_word��������롣
 */
static void
make_compound_part_metaword(struct splitter_context* sc, struct word_list* wl)
{
  int i, j, k;
  seq_ent_t se = wl->part[PART_CORE].seq;
  int ent_num = anthy_get_nr_compound_ents(se);

  for (i = 0; i < ent_num; ++i) {
    compound_ent_t ce = anthy_get_nth_compound_ent(se, i);
    int seg_num = anthy_compound_get_nr_segments(ce);
    struct meta_word *mw = NULL;
    struct meta_word *mw2 = NULL;

    /* ����� */
    for (j = seg_num - 1; j >= 0; --j) {
      mw = make_compound_nth_metaword(sc, ce, j, wl, MW_COMPOUND_PART);
      for (k = j - 1; k >= 0; --k) {
	mw2 = make_compound_nth_metaword(sc, ce, k, wl, MW_COMPOUND_PART);
	mw2->len += mw->len;
	mw2->score += mw->score;
	anthy_xstrcat(&mw2->cand_hint, &mw->cand_hint);

	anthy_commit_meta_word(sc, mw2);	
	mw = mw2;
      }
    } 
  }
}

/*
 * ñʸ��ñ��
 */
static void
make_simple_metaword(struct splitter_context *sc, struct word_list* wl)
{
  struct meta_word *mw = alloc_metaword(sc);
  mw->wl = wl;
  mw->from = wl->from;
  mw->len = wl->len;
  mw->weak_len = wl->weak_len;
  mw->score = wl->score;
  mw->type = MW_SINGLE;
  mw->seg_class = wl->seg_class;
  mw->nr_parts = NR_PARTS;
  anthy_commit_meta_word(sc, mw);
}

/*
 * wordlist��Ĥ���ʤ롢metaword�����
 */
static void
make_metaword_from_word_list(struct splitter_context *sc)
{
  int i;
  for (i = 0; i < sc->char_count; i++) {
    struct word_list *wl;
    for (wl = sc->word_split_info->cnode[i].wl;
	 wl; wl = wl->next) {
      if (wl->is_compound) {
	make_compound_part_metaword(sc, wl);
	make_compound_metaword(sc, wl);
      } else {
	make_simple_metaword(sc, wl);
      }
    }
  }
}

/*
 * metaword��ꥹ�����˷�礹��
 */
struct meta_word *
anthy_do_list_metaword(struct splitter_context *sc,
		       enum metaword_type type,
		       struct meta_word *mw, struct meta_word *mw2,
		       int weak)
{
  struct meta_word *wrapped_mw = anthy_do_cons_metaword(sc, type, mw2, NULL, weak);
  struct meta_word *n = anthy_do_cons_metaword(sc, type, mw, wrapped_mw, weak);

  return n;
}

/*
 * metaword��ºݤ˷�礹��
 */
struct meta_word *
anthy_do_cons_metaword(struct splitter_context *sc,
		       enum metaword_type type,
		       struct meta_word *mw, struct meta_word *mw2,
		       int weak)
{
  struct meta_word *n;
 
  n = alloc_metaword(sc);
  n->from = mw->from;
  n->len = mw->len + (mw2 ? mw2->len : 0);
  if (weak) {
    n->weak_len = mw->weak_len + (mw2 ? mw2->len : 0);
  } else {
    n->weak_len = mw->weak_len + (mw2 ? mw2->weak_len : 0);
  }

  if (mw2) {
    n->score = sqrt(mw->score) * sqrt(mw2->score);
  } else {
    n->score = mw->score;    
  }
  n->type = type;
  n->mw1 = mw;
  n->mw2 = mw2;
  n->seg_class = mw2 ? mw2->seg_class : mw->seg_class;
  n->nr_parts = mw->nr_parts + (mw2 ? mw2->nr_parts : 0);
  anthy_commit_meta_word(sc, n);
  return n;
}

/*
 * ư��Ϣ�ѷ� + ���ƻ첽������ �֡����䤹���פʤ�
 */
static void
try_combine_v_renyou_a(struct splitter_context *sc,
		       struct meta_word *mw, struct meta_word *mw2)
{
  wtype_t w2;
  if (!mw->wl || !mw2->wl) return;

  w2 = mw2->wl->part[PART_CORE].wt;

  if (mw->wl->head_pos == POS_V &&
      mw->wl->tail_ct == CT_RENYOU &&
      anthy_wtype_get_pos(w2) == POS_A) {
    /* ���ƻ�ǤϤ���ΤǼ��Υ����å� */
    if (anthy_get_seq_ent_wtype_freq(mw2->wl->part[PART_CORE].seq, 
				     anthy_wtype_a_tail_of_v_renyou)) {
      anthy_do_list_metaword(sc, MW_V_RENYOU_A, mw, mw2, 0);
    }
  }
}

/*
 * ư��Ϣ�ѷ� + ̾�첽������(#D2T35) ������ ����(�Τ���)�פʤ�
 */
static void
try_combine_v_renyou_noun(struct splitter_context *sc,
			  struct meta_word *mw, struct meta_word *mw2)
{
  wtype_t w2;
  if (!mw->wl || !mw2->wl) return;

  w2 = mw2->wl->part[PART_CORE].wt;
  if (mw->wl->head_pos == POS_V &&
      mw->wl->tail_ct == CT_RENYOU &&
      anthy_wtype_get_pos(w2) == POS_NOUN &&
      anthy_wtype_get_scos(w2) == SCOS_T40) {
    anthy_do_list_metaword(sc, MW_V_RENYOU_NOUN, mw, mw2, 0);
  }
}


/*
 * ̾�� + ̾��������(#N2T35, #N2T30) �֥����� �ѡפʤ�
 */
static void
try_combine_noun_noun_postfix(struct splitter_context *sc,
			      struct meta_word *mw, struct meta_word *mw2)
{
  wtype_t w1;
  if (!mw->wl || !mw2->wl) return;

  w1 = mw->wl->part[PART_CORE].wt;
  if (anthy_wtype_get_pos(w1) == POS_NOUN &&
      mw->wl->part[PART_CORE].len > 1 &&
      mw->wl->part[PART_POSTFIX].len == 0 &&
      mw->wl->part[PART_DEPWORD].len == 0 &&
      mw2->wl->part[PART_CORE].len > 1 &&
      anthy_wtype_get_pos(mw2->wl->part[PART_CORE].wt) == POS_N2T &&
      anthy_get_seq_ent_wtype_freq(mw2->wl->part[PART_CORE].seq, 
				   anthy_wtype_noun_and_postfix)) {
    anthy_do_list_metaword(sc, MW_NOUN_NOUN_PREFIX, mw, mw2, 1);
  }
}

/*
 * �� + ̾���礹��
 */
static void
try_combine_name(struct splitter_context *sc,
		 struct meta_word *mw, struct meta_word *mw2)
{
  int f, f2;
  if (!mw->wl || !mw2->wl) return;

  f = anthy_get_seq_flag(mw->wl->part[PART_CORE].seq);
  f2 = anthy_get_seq_flag(mw2->wl->part[PART_CORE].seq);

  if ((f & NF_FAMNAME) && (f2 & NF_FSTNAME)) {
    if (anthy_wtype_get_scos(mw->wl->part[PART_CORE].wt) == SCOS_FAMNAME &&
	anthy_wtype_get_scos(mw2->wl->part[PART_CORE].wt) == SCOS_FSTNAME) {
      anthy_do_list_metaword(sc, MW_NAMEPAIR, mw, mw2, 0);
    }
  }
}

/*
 * �������礹��
 */
static void
try_combine_number(struct splitter_context *sc,
		 struct meta_word *mw1, struct meta_word *mw2)
{
  struct word_list *wl1 = mw1->wl;
  struct word_list *wl2 = mw2->wl;
  struct meta_word *combined_mw;
  int recursive = wl2 ? 0 : 1; /* combined��mw���礹����1 */

  /* ��mw�Ͽ��� */
  if (anthy_wtype_get_pos(wl1->part[PART_CORE].wt) != POS_NUMBER) return;  
  if (recursive) {
    /* ��mw�Ͽ������礷��mw */
    if (mw2->type != MW_NUMBER) return;
    wl2 = mw2->mw1->wl;
  } else {
    /* ��mw�Ͽ��� */
    if (anthy_wtype_get_pos(wl2->part[PART_CORE].wt) != POS_NUMBER) return;    
  }

  /* ��mw�θ���ʸ�����դ��Ƥ��ʤ���� */
  if (wl1->part[PART_POSTFIX].len == 0 &&
      wl1->part[PART_DEPWORD].len == 0) {
    int scos1 = anthy_wtype_get_scos(wl1->part[PART_CORE].wt);
    int scos2 = anthy_wtype_get_scos(wl2->part[PART_CORE].wt);

    /* #NN���оݳ� */
    if (scos2 == SCOS_NONE) return;
    /* 
       ��mw�μ���ˤ�äơ����ˤĤ����Ȥ��Ǥ��뱦mw�μ��ब�Ѥ��
       �㤨�а����θ��ˤ����������������岯�����Ĥ����Ȥ��Ǥ��ʤ�����
       �����彽�θ��ˤϡ����碌�ư����ʤɤ�Ĥ����Ȥ��Ǥ���
     */
    switch (scos1) {
    case SCOS_N1: 
      if (scos2 == SCOS_N1) return; /* ���˰���夬�Ĥ��ƤϤ����ʤ� */
    case SCOS_N10:
      if (scos2 == SCOS_N10) return; /* ���˽����彽���Ĥ��ƤϤ����ʤ� */
    case SCOS_N100:
      if (scos2 == SCOS_N100) return; /* ����ɴ����ɴ���Ĥ��ƤϤ����ʤ� */
    case SCOS_N1000:
      if (scos2 == SCOS_N1000) return; /* ����������餬�Ĥ��ƤϤ����ʤ� */
    case SCOS_N10000:
      /* ���������������岯�Ĥʤɤϡ�
	 ���ĤǤ���ˤĤ����Ȥ��Ǥ��� */
      break;
    default:
      return;
    }

    if (recursive) {
      combined_mw = anthy_do_cons_metaword(sc, MW_NUMBER, mw1, mw2, 0);
    } else {
      /* ���Ʒ�礹����ϸ���null��Ĥ���list�ˤ��� */
      combined_mw = anthy_do_list_metaword(sc, MW_NUMBER, mw1, mw2, 0);
    }
    combine_metaword(sc, combined_mw);
  }
}

/* ���٤�metaword�ȷ��Ǥ��뤫�����å� */
static void
try_combine_metaword(struct splitter_context *sc,
		     struct meta_word *mw1, struct meta_word *mw2)
{
  if (!mw1->wl) return;

  /* metaword�η���Ԥ�����ˤϡ���³��
     metaword����Ƭ�����ʤ����Ȥ�ɬ�� */
  if (mw2->wl && mw2->wl->part[PART_PREFIX].len > 0) {
    return;
  }
  
  try_combine_name(sc, mw1, mw2);
  try_combine_v_renyou_a(sc, mw1, mw2);
  try_combine_v_renyou_noun(sc, mw1, mw2);
  try_combine_noun_noun_postfix(sc, mw1, mw2);
  try_combine_number(sc, mw1, mw2);
}

static void
combine_metaword(struct splitter_context *sc, struct meta_word *mw)
{
  struct meta_word *mw_left;
  struct word_split_info_cache *info = sc->word_split_info;
  int i;

  /* ��°�������ʸ��ȤϷ�礷�ʤ� */  
  if (anthy_seg_class_is_depword(mw->seg_class)) return;

  for (i = mw->from - 1; i >= 0; i--) {
    for (mw_left = info->cnode[i].mw; mw_left; mw_left = mw_left->next) {
      if (mw_left->from + mw_left->len == mw->from) {
	/* ���Ǥ��뤫�����å� */
	try_combine_metaword(sc, mw_left, mw);
      }
    }
  }
}

static void
combine_metaword_all(struct splitter_context *sc)
{
  int i;

  struct word_split_info_cache *info = sc->word_split_info;
  /* metaword�κ�ü�ˤ��롼�� */
  for (i = sc->char_count - 1; i >= 0; i--){
    struct meta_word *mw;
    /* ��metaword�Υ롼�� */
    for (mw = info->cnode[i].mw;
	 mw; mw = mw->next) {
      combine_metaword(sc, mw);
    }
  }
}

static void
make_dummy_metaword(struct splitter_context *sc, int from,
		    int len, int orig_len)
{
  int score = 0;
  struct meta_word *mw, *n;

  for (mw = sc->word_split_info->cnode[from].mw; mw; mw = mw->next) {
   if (mw->len != orig_len) continue;
    if (mw->score > score) {
      score = mw->score;
    }
  }

  n = alloc_metaword(sc);
  n->type = MW_DUMMY;
  n->from = from;
  n->len = len;
  n->score = 3 * score * len / orig_len;
  if (mw) {
    mw->nr_parts = 0;
  }
  anthy_commit_meta_word(sc, n);
}

/*
 * ʸ��򿭤Ф����餽���Ф��Ƥ���
 */
static void
make_expanded_metaword_all(struct splitter_context *sc)
{
  int i, j;
  if (anthy_select_section("EXPANDPAIR", 0) == -1) {
    return ;
  }
  for (i = 0; i < sc->char_count; i++) {
    for (j = 1; j < sc->char_count - i; j++) {
      /* ���Ƥ���ʬʸ������Ф��� */
      xstr xs;
      xs.len = j;
      xs.str = sc->ce[i].c;
      if (anthy_select_column(&xs, 0) == 0) {
	/* ������ʬʸ����ϲ��˳�����оݤȤʤä� */
        int k;
        int nr = anthy_get_nr_values();
        for (k = 0; k < nr; k++) {
          xstr *exs;
          exs = anthy_get_nth_xstr(k);
          if (exs && exs->len <= sc->char_count - i) {
            xstr txs;
            txs.str = sc->ce[i].c;
            txs.len = exs->len;
            if (!anthy_xstrcmp(&txs, exs)) {
              make_dummy_metaword(sc, i, txs.len, j);
            }
          }
        }
      }
    }
  }
}

/* ��������ؽ���metaword���� */
static void
make_ochaire_metaword(struct splitter_context *sc,
		      int from, int len)
{
  struct meta_word *mw;
  int count;
  int s;
  int j;
  int seg_len;
  int mw_len = 0;
  xstr* xs;

  (void)len;

  /* ʸ�������� */
  count = anthy_get_nth_value(0);
  /* ���ֱ���ʸ���Τ�����ʸ�����ι�פ�׻� */
  for (s = 0, j = 0; j < count - 1; j++) {
    s += anthy_get_nth_value(j * 2 + 1);
  }
  /* ���ֱ���ʸ���metaword���� */
  seg_len = anthy_get_nth_value((count - 1) * 2 + 1);
  mw = alloc_metaword(sc);
  mw->type = MW_OCHAIRE;
  mw->from = from + s;
  mw->len = seg_len;
  mw->score = OCHAIRE_SCORE;
  xs = anthy_get_nth_xstr((count - 1) * 2 + 2);
  mw->cand_hint.str = malloc(sizeof(xchar)*xs->len);
  anthy_xstrcpy(&mw->cand_hint, xs);
  anthy_commit_meta_word(sc, mw);
  mw_len += seg_len;
  /* ����ʳ���ʸ���metaword���� */
  for (j-- ; j >= 0; j--) {
    struct meta_word *n;
    seg_len = anthy_get_nth_value(j * 2 + 1);
    s -= seg_len;
    n = alloc_metaword(sc);
    n->type = MW_OCHAIRE;
    /* ����metaword��Ĥʤ� */
    n->mw1 = mw;
    n->from = from + s;
    n->len = seg_len;
    n->score = OCHAIRE_SCORE;
    xs = anthy_get_nth_xstr(j * 2 + 2);
    n->cand_hint.str = malloc(sizeof(xchar)*xs->len);
    anthy_xstrcpy(&n->cand_hint, xs);
    anthy_commit_meta_word(sc, n);
    mw = n;
    mw_len += seg_len;
  } 
}

/*
 * ʣ����ʸ����Ȥ����򤫤鸡������
 */
static void
make_ochaire_metaword_all(struct splitter_context *sc)
{
  int i;
  if (anthy_select_section("OCHAIRE", 0) == -1) {
    return ;
  }

  for (i = 0; i < sc->char_count; i++) {
    xstr xs;
    xs.len = sc->char_count - i;
    xs.str = sc->ce[i].c;
    if (anthy_select_longest_column(&xs) == 0) {
      xstr* key;
      int len;
      anthy_mark_column_used();
      key = anthy_get_index_xstr();
      len = key->len;

      make_ochaire_metaword(sc, i, len);
      /* ���󸫤Ĥ��ä� meta_word �μ���ʸ������Ϥ�� */
      i += len - 1;
      break;
    }
  }
}

/*
 * metaword�θ��λ�¿��ʸ���򤯤äĤ���metaword��������
 */
static void
make_metaword_with_depchar(struct splitter_context *sc,
			   struct meta_word *mw)
{
  int j;
  int destroy_seg_class = 0;
  int from = mw ? mw->from : 0;
  int len = mw ? mw->len : 0;

  /* metaword��ľ���ʸ���μ����Ĵ�٤� */
  int type = anthy_get_xchar_type(*sc->ce[from + len].c);
  if (!(type & XCT_SYMBOL) &&
      !(type & XCT_PART)) {
    return;
  }

  /* Ʊ�������ʸ���Ǥʤ���Ф��äĤ���Τ򤦤����� */
  for (j = 0; from + len + j < sc->char_count; j++) {
    int p = from + len + j;
    if ((anthy_get_xchar_type(*sc->ce[p].c) != type)) {
      break;
    }
    if (0 < p && p + 1 < sc->char_count && *sc->ce[p].c != *sc->ce[p + 1].c) {
      destroy_seg_class = 1;
    }
  }

  /* ��Υ롼�פ�ȴ��������j�ˤ���Ω�Ǥ��ʤ�ʸ���ο������äƤ��� */

  /* ��Ω�Ǥ��ʤ�ʸ��������Τǡ�������դ���metaword���� */
  if (j > 0) {
    struct meta_word *n;
    n = alloc_metaword(sc);
    n->from = from;
    n->len = len + j;
    if (mw) {
      n->type = MW_WRAP;
      n->mw1 = mw;
      n->score = mw->score;
      n->nr_parts = mw->nr_parts;
      n->weak_len = mw->weak_len + j;
      if (destroy_seg_class) {
	n->seg_class = SEG_DOKURITSUGO;
	n->score /= 10;
      } else {
	n->seg_class = mw->seg_class;
      }
    } else {
      n->type = MW_SINGLE;
      n->score = 1;
      n->seg_class = SEG_DOKURITSUGO;
    }
    anthy_commit_meta_word(sc, n);
  }
}

static void 
make_metaword_with_depchar_all(struct splitter_context *sc)
{
  int i;
  struct word_split_info_cache *info = sc->word_split_info;

  /* ��metaword���Ф��� */
  for (i = 0; i < sc->char_count; i++) {
    struct meta_word *mw;
    for (mw = info->cnode[i].mw;
	 mw; mw = mw->next) {
      make_metaword_with_depchar(sc, mw);
    }
  }
  make_metaword_with_depchar(sc, NULL);
}

static int
is_single(xstr* xs)
{
  int i;
  int xct;
  for (i = xs->len - 1; i >= 1; --i) {
    xct = anthy_get_xchar_type(xs->str[i]);
    if (!(xct & XCT_PART)) {
      return 0;
    }
  }
  return 1;
}

static void 
bias_to_single_char_metaword(struct splitter_context *sc)
{
  int i;

  for (i = sc->char_count - 1; i >= 0; --i) {
    struct meta_word *mw;
    xstr xs;
    int xct;

    struct char_node *cnode = &sc->word_split_info->cnode[i];

    /* ���å��ξ��ϰ�ʸ����ʸ������Ǥ��� */
    xct = anthy_get_xchar_type(*sc->ce[i].c);
    if (xct & (XCT_OPEN|XCT_CLOSE)) {
      continue;
    }

    xs.str = sc->ce[i].c;
    for (mw = cnode->mw; mw; mw = mw->next) {
      /* ��°��Τߤ�ʸ��ϸ������ʤ� */
      if (anthy_seg_class_is_depword(mw->seg_class)) {
	continue;
      } 
      /* ��ʸ��(+ľ���ˤĤʤ���ʸ���η����֤�)�Υ������򲼤��� */
      xs.len = mw->len;
      if (is_single(&xs)) {
	mw->score /= 100;
      }
    }
  }
}

void
anthy_mark_border_by_metaword(struct splitter_context* sc,
			      struct meta_word* mw)
{
  struct word_split_info_cache* info = sc->word_split_info;
  if (!mw) return;

  switch (mw->type) {
  case MW_DUMMY:
    /* BREAK THROUGH */
  case MW_SINGLE:
    /* BREAK THROUGH */
  case MW_COMPOUND_PART: 
    info->seg_border[mw->from] = 1;    
    break;
  case MW_COMPOUND_LEAF:
    info->seg_border[mw->from] = 1;    
    info->best_mw[mw->from] = mw;
    break;
  case MW_COMPOUND_HEAD:
    /* BREAK THROUGH */
  case MW_COMPOUND:
    /* BREAK THROUGH */
  case MW_NAMEPAIR:
    /* BREAK THROUGH */
  case MW_NUMBER:
    info->best_mw[mw->mw1->from] = mw->mw1;
    anthy_mark_border_by_metaword(sc, mw->mw1);
    anthy_mark_border_by_metaword(sc, mw->mw2);
    break;
  case MW_V_RENYOU_A:
    /* BREAK THROUGH */
  case MW_V_RENYOU_NOUN:
    /* BREAK THROUGH */
  case MW_NOUN_NOUN_PREFIX:
    info->seg_border[mw->from] = 1;    
    break;
  case MW_WRAP:
    anthy_mark_border_by_metaword(sc, mw->mw1);
    break;
  case MW_OCHAIRE:
    info->seg_border[mw->from] = 1;
    anthy_mark_border_by_metaword(sc, mw->mw1);
    break;
  default:
    break;
  }
}

void
anthy_make_metaword_all(struct splitter_context *sc)
{
  /* �ޤ���word_list���ä���metaword���� */
  make_metaword_from_word_list(sc);

  /* metaword���礹�� */
  combine_metaword_all(sc);

  /* ���礵�줿ʸ���������� */
  make_expanded_metaword_all(sc);

  /* ������Ĺ���ʤɤε��桢����¾�ε������� */
  make_metaword_with_depchar_all(sc);

  /* ������򤤤�� */
  make_ochaire_metaword_all(sc);

  /* ��ʸ����ʸ��ϸ��� */
  bias_to_single_char_metaword(sc);
}

/* 
 * ���ꤵ�줿�ΰ�򥫥С�����metaword������� 
 */
int
anthy_get_nr_metaword(struct splitter_context *sc,
		     int from, int len)
{
  struct meta_word *mw;
  int n;

  for (n = 0, mw = sc->word_split_info->cnode[from].mw;
       mw; mw = mw->next) {
    if (mw->len == len && mw->can_use == ok) {
      n++;
    }
  }
  return n;
}

struct meta_word *
anthy_get_nth_metaword(struct splitter_context *sc,
		      int from, int len, int nth)
{
  struct meta_word *mw;
  int n;
  for (n = 0, mw = sc->word_split_info->cnode[from].mw;
       mw; mw = mw->next) {
    if (mw->len == len && mw->can_use == ok) {
      if (n == nth) {
	return mw;
      }
      n++;
    }
  }
  return NULL;
}
