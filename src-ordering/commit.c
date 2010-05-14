/*
 * ����(���ߥå�)��ν����򤹤롣
 * �Ƽ�γؽ�������ƤӽФ�
 *
 * anthy_proc_commit() ����������ƤФ��
 */
#include <stdlib.h>

#include <ordering.h>
#include <record.h>
#include "splitter.h"
#include "segment.h"
#include "sorter.h"

#define MAX_OCHAIRE_ENTRY_COUNT 100
#define MAX_OCHAIRE_LEN 32

/* �򴹤��줿�����õ�� */
static void
learn_swapped_candidates(struct segment_list *sl)
{
  int i;
  struct seg_ent *seg;
  for (i = 0; i < sl->nr_segments; i++) {
    seg = anthy_get_nth_segment(sl, i);
    if (seg->committed != 0) {
      /* �ǽ�θ���(0����)�Ǥʤ�����(seg->commited����)�����ߥåȤ��줿 */
      anthy_swap_cand_ent(seg->cands[0],
			  seg->cands[seg->committed]);
    }
  }
  anthy_cand_swap_ageup();
}

/* Ĺ�����Ѥ�ä�ʸ����Ф��� */
static void
learn_resized_segment(struct splitter_context *sc,
		      struct segment_list *sl)
		      
{
  int i;
  struct meta_word **mw
    = alloca(sizeof(struct meta_word*) * sl->nr_segments);
  int *len_array
    = alloca(sizeof(int) * sl->nr_segments);

  /* ��ʸ���Ĺ���������meta_word��������Ѱդ��� */
  for (i = 0; i < sl->nr_segments; i++) {
    struct seg_ent *se = anthy_get_nth_segment(sl, i);
    mw[i] = se->cands[se->committed]->mw;
    len_array[i] = se->str.len;
  }

  anthy_commit_border(sc, sl->nr_segments, mw, len_array);
}

/* record�ˤ�������ؽ��η�̤�񤭹��� */
static void
commit_ochaire(struct seg_ent *seg, int count, xstr* xs)
{
  int i;
  if (xs->len >= MAX_OCHAIRE_LEN) {
    return ;
  }
  if (anthy_select_column(xs, 1)) {
    return ;
  }
  anthy_set_nth_value(0, count);
  for (i = 0; i < count; i++, seg = seg->next) {
    anthy_set_nth_value(i * 2 + 1, seg->len);
    anthy_set_nth_xstr(i * 2 + 2, &seg->cands[seg->committed]->str);
  }
}

/* record���ΰ�����󤹤뤿��ˡ���������ؽ��Υͥ��ƥ��֤�
   ����ȥ��ä� */
static void
release_negative_ochaire(struct splitter_context *sc,
			 struct segment_list *sl)
{
  int start, len;
  xstr xs;
  (void)sl;
  /* �Ѵ����ΤҤ餬��ʸ���� */
  xs.len = sc->char_count;
  xs.str = sc->ce[0].c;

  /* xs����ʬʸ������Ф��� */
  for (start = 0; start < xs.len; start ++) {
    for (len = 1; len <= xs.len - start && len < MAX_OCHAIRE_LEN; len ++) {
      xstr part;
      part.str = &xs.str[start];
      part.len = len;
      if (anthy_select_column(&part, 0) == 0) {
	anthy_release_column();
      }
    }
  }
}

/* ��������ؽ���Ԥ� */
static void
learn_ochaire(struct splitter_context *sc,
	      struct segment_list *sl)
{
  int i;
  int count;

  if (anthy_select_section("OCHAIRE", 1)) {
    return ;
  }

  /* ��������ؽ��Υͥ��ƥ��֤ʥ���ȥ��ä� */
  release_negative_ochaire(sc, sl);

  /* ��������ؽ��򤹤� */
  for (count = 2; count <= sl->nr_segments && count < 5; count++) {
    /* 2ʸ��ʾ��Ĺ����ʸ������Ф��� */

    for (i = 0; i <= sl->nr_segments - count; i++) {
      struct seg_ent *head = anthy_get_nth_segment(sl, i);
      struct seg_ent *s;
      xstr xs;
      int j;
      xs = head->str;
      if (xs.len < 2 && count < 3) {
	/* ���ڤ��ʸ���ؽ����뤳�Ȥ��򤱤롢
	 * �����ø���heuristics */
	continue;
      }
      /* ʸ�����������ʸ������� */
      for (j = 1, s = head->next; j < count; j++, s = s->next) {
	xs.len += s->str.len;
      }
      /**/
      commit_ochaire(head, count, &xs);
    }
  }
  if (anthy_select_section("OCHAIRE", 1)) {
    return ;
  }
  anthy_truncate_section(MAX_OCHAIRE_ENTRY_COUNT);
}

static int
check_segment_relation(struct seg_ent *cur, struct seg_ent *target)
{
  /* ��Ƭ�θ���ǳ��ꤵ�줿�Τǡ��ؽ����ʤ� */
  if (cur->committed == 0) {
    return 0;
  }
  /* ñ��ʷ�����ʸ�ᤷ���ؽ����ʤ� */
  if (cur->cands[0]->nr_words != 1 ||
      cur->cands[cur->committed]->nr_words != 1 ||
      target->cands[target->committed]->nr_words != 1) {
    return 0;
  }
  /* ���ꤵ�줿ʸ����ʻ�ȡ��ǽ�˽Ф���������ʻ줬Ʊ���Ǥ��뤳�Ȥ��ǧ */
  if (anthy_wtype_get_pos(cur->cands[0]->elm[0].wt) !=
      anthy_wtype_get_pos(cur->cands[cur->committed]->elm[0].wt)) {
    return 0;
  }
  if (cur->cands[cur->committed]->elm[0].id == -1 ||
      target->cands[target->committed]->elm[0].id == -1) {
    return 0;
  }
  /* ������Ф�����Ͽ�򤹤� */
  anthy_dic_register_relation(target->cands[target->committed]->elm[0].id,
			      cur->cands[cur->committed]->elm[0].id);
  return 1;
			      
}

static void
learn_word_relation(struct segment_list *sl)
{
  int i, j;
  int nr_learned = 0;
  for (i = 0; i < sl->nr_segments; i++) {
    struct seg_ent *cur = anthy_get_nth_segment(sl, i);
    for (j = i - 2; j < i + 2 && j < sl->nr_segments; j++) {
      struct seg_ent *target;
      if (i == j || j < 0) {
	continue;
      }
      target = anthy_get_nth_segment(sl, j);
      /* i���ܤ�j���ܤ�ʸ��θ���δط���ؽ��Ǥ��뤫�����å����� */
      nr_learned += check_segment_relation(cur, target);
    }
  }
  /* �ؽ���ȯ�����Ƥ���Х��ߥåȤ��� */
  if (nr_learned > 0) {
    anthy_dic_commit_relation();
  }
}

void
anthy_proc_commit(struct segment_list *sl,
		  struct splitter_context *sc)
{
  /* �Ƽ�γؽ���Ԥ� */
  learn_swapped_candidates(sl);
  learn_resized_segment(sc, sl);
  learn_ochaire(sc, sl);
  learn_word_relation(sl);
  anthy_learn_cand_history(sl);
}
