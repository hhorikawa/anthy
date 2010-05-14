/*
 * Anthy�μ���饤�֥����濴
 *
 * Copyright (C) 2000-2006 TABATA Yusuke
 * Copyright (C) 2005-2006 YOSHIDA Yuichi
 *
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
#include <stdlib.h>

#include <dic.h>
#include <conf.h>
#include <record.h>
#include <alloc.h>
#include <logger.h>
#include <xchar.h>

#include <diclib.h>

#include "dic_ent.h"
#include "dic_personality.h"
#include "dic_main.h"

/**/
static int dic_init_count;

/* ���� */
/* ��personality�Ƕ�ͭ�����ե����뼭�� */
static struct word_dic *master_dic_file;

/* �ƥѡ����ʥ�ƥ����Ȥμ��� */
struct mem_dic *anthy_current_personal_dic_cache;/* ����å��� */
/**/
struct record_stat *anthy_current_record;


/** Ʊ���ɤߤ��Ф��롢ñ�줫��ե饰��׻����� */
static void
calc_seq_flags(struct seq_ent *se)
{
  int i;

  /* ���Ƥμ��񥨥�ȥ���Ф��� */
  for (i = 0; i < se->nr_dic_ents; i++) {
    int p;
    p = anthy_wtype_get_pos(se->dic_ents[i]->type);
    switch (p) {
    case POS_NOUN:
      {
	int c;
	c = anthy_wtype_get_cos(se->dic_ents[i]->type);
	if (c == COS_NN) {

	}else if (c == COS_JN) {
	  int s;
	  s = anthy_wtype_get_scos(se->dic_ents[i]->type);
	  if (s == SCOS_FSTNAME) {
	    se->flags |= NF_FSTNAME;
	  } else if (s == SCOS_FAMNAME) {
	    se->flags |= NF_FAMNAME;
	  } else {
	    se->flags |= NF_UNSPECNAME;
	  }
	}
      }
      break;
    case POS_PRE:
    case POS_SUC:
      {
	int c;
	c = anthy_wtype_get_cos(se->dic_ents[i]->type);
	if (c == COS_JN) {
	  se->flags |= SF_JN;
	}else if (c == COS_NN) {
	  se->flags |= SF_NUM;
	}
      }
      break;
    }
  }
}

static struct seq_ent *
cache_get_seq_ent_to_mem_dic(struct mem_dic *dd, xstr *xs, int is_reverse)
{
  struct seq_ent *seq;
  /* ����å������̵���Τǳ��� */
  seq = anthy_mem_dic_alloc_seq_ent_by_xstr(dd, xs, is_reverse);

  /* word_dic����μ��� */
  anthy_word_dic_fill_seq_ent_by_xstr(master_dic_file, xs, seq, is_reverse);

  /*���٤Ƥ�dic_ent���Ф���flag��׻�����*/
  calc_seq_flags(seq);

  return seq;
}

struct seq_ent *
anthy_cache_get_seq_ent(xstr *xs, int is_reverse)
{
  struct seq_ent *seq;

  /* ����å�����˴��ˤ���Ф�����֤� */
  seq = anthy_mem_dic_find_seq_ent_by_xstr(anthy_current_personal_dic_cache,
					   xs, is_reverse);
  if (seq) {
    return seq;
  }

  /* ����å���ˤ�̵���ΤǶ��̼��񤫤�μ��� */
  seq = cache_get_seq_ent_to_mem_dic(anthy_current_personal_dic_cache,
				     xs, is_reverse);

  /* �Ŀͼ��񤫤�μ��� */
  anthy_copy_words_from_private_dic(seq, xs, is_reverse);

  if (seq->nr_dic_ents == 0 && seq->nr_compound_ents == 0) {
    /* ̵���ʥ���ȥ����������Τ�cache������ */
    anthy_mem_dic_release_seq_ent(anthy_current_personal_dic_cache,
				  xs, is_reverse);
    return NULL;
  }

  return seq;
}

int anthy_dic_check_word_relation(int from, int to)
{
  return anthy_word_dic_check_word_relation(master_dic_file, from, to);
}

static seq_ent_t
do_get_seq_ent_from_xstr(xstr *xs, int is_reverse)
{
  struct seq_ent *se;
  /* ����å��夫����Ф� */
  se = anthy_cache_get_seq_ent(xs, is_reverse);
  if (!se) {
    /* �����ʤɤμ����̵��ʸ����򸡺����� */
    return anthy_get_ext_seq_ent_from_xstr(xs, is_reverse);
  }
  return se;
}

seq_ent_t
anthy_get_seq_ent_from_xstr(xstr *xs, int is_reverse)
{
  struct seq_ent *se;

  if (!xs) {
    return NULL;
  }
  if (!is_reverse) {
    /* �֥��פκ����ä����Ѵ��ξ�硢�֤����פ�ľ���Ƹ�������
     *   ��̤Υ쥤�䡼�Ǥϥ桼����Ϳ����ʸ����򤽤Τޤ��ݻ����뤳�Ȥ�
     *   ���Ԥ����Τǡ��Ѵ��Ϥ����ǹԤʤ���
     */
    int i, v = 0;
    int j;

    /* �֥��פνи�������� */
    for (i = 0; i < xs->len; i++) {
      if (xs->str[i] == KK_VU) {
	v++;
      }
    }
    if (v > 0) {
      xstr nx;
      nx.len = xs->len + v;
      nx.str = malloc(sizeof(xchar)*nx.len);
      j = 0;
      /* �֥��פ�֤����פ��Ѵ����Ĥĥ��ԡ����� */
      for (i = 0; i < xs->len; i++) {
	if (xs->str[i] == KK_VU) {
	  nx.str[j] = HK_U;
	  j++;
	  nx.str[j] = HK_DDOT;
	  j++;
	} else {
	  nx.str[j] = xs->str[i];
	  j++;
	}
      }
      se = do_get_seq_ent_from_xstr(&nx, 0);
      free(nx.str);
      return se;
    }
  }
  /* �֥��פ��и����ʤ����⤷���ϵ��Ѵ��ξ�� */
  return do_get_seq_ent_from_xstr(xs, is_reverse);
}

/*
 * seq_ent�μ���
 ************************
 * seq_ent�γƼ����μ���
 */
int
anthy_get_seq_flag(seq_ent_t seq)
{
  if (!seq) {
    return 0;
  }

  return seq->flags;
}

int
anthy_get_nr_dic_ents(seq_ent_t se, xstr *xs)
{
  struct seq_ent *s = se;
  if (!s) {
    return 0;
  }
  return s->nr_dic_ents + anthy_get_nr_dic_ents_of_ext_ent(se, xs);
}

int
anthy_get_nth_dic_ent_str(seq_ent_t se, xstr *o,
			  int n, xstr *x)
{
  struct seq_ent *s = se;
  if (!s) {
    return -1;
  }
  if (n >= s->nr_dic_ents) {
    return anthy_get_nth_dic_ent_str_of_ext_ent(se, o, n - s->nr_dic_ents, x);
  }
  x->len = s->dic_ents[n]->str.len;
  x->str = anthy_xstr_dup_str(&s->dic_ents[n]->str);
  return 0;
}

int
anthy_get_nth_dic_ent_freq(seq_ent_t se, int nth)
{
  struct seq_ent *s = se;
  if (!s) {
    return 0;
  }
  if (!s->dic_ents) {
    return anthy_get_nth_dic_ent_freq_of_ext_ent(se, nth);
  }
  if (s->nr_dic_ents <= nth) {
    return anthy_get_nth_dic_ent_freq_of_ext_ent(se, nth - se->nr_dic_ents);
  }
  return s->dic_ents[nth]->freq;
}

int
anthy_get_nth_dic_ent_wtype(seq_ent_t se, xstr *xs,
			    int n, wtype_t *w)
{
  struct seq_ent *s = se;
  if (!s) {
    *w = anthy_wt_none;
    return -1;
  }
  if (s->nr_dic_ents <= n) {
    int r;
    r = anthy_get_nth_dic_ent_wtype_of_ext_ent(xs, n - s->nr_dic_ents, w);
    if ( r == -1) {
      *w = anthy_wt_none;
    }
    return r;
  }
  *w =  s->dic_ents[n]->type;
  return 0;
}

int
anthy_get_seq_ent_pos(seq_ent_t se, int pos)
{
  int i, v=0;
  struct seq_ent *s = se;
  if (!s) {
    return 0;
  }
  if (s->nr_dic_ents == 0) {
    return anthy_get_ext_seq_ent_pos(se, pos);
  }
  for (i = 0; i < s->nr_dic_ents; i++) {
    if (anthy_wtype_get_pos(s->dic_ents[i]->type) == pos) {
      v += s->dic_ents[i]->freq;
      if (v == 0) {
	v = 1;
      }
    }
  }
  return v;
}

int
anthy_get_seq_ent_ct(seq_ent_t se, int pos, int ct)
{
  int i, v=0;
  struct seq_ent *s = se;
  if (!s) {
    return 0;
  }
  if (s->nr_dic_ents == 0) {
    return anthy_get_ext_seq_ent_ct(s, pos, ct);
  }
  for (i = 0; i < s->nr_dic_ents; i++) {
    if (anthy_wtype_get_pos(s->dic_ents[i]->type)== pos &&
	anthy_wtype_get_ct(s->dic_ents[i]->type)==ct) {
      v += s->dic_ents[i]->freq;
      if (v == 0) {
	v = 1;
      }
    }
  }
  return v;
}

/*
 * wt���ʻ�����ñ�����Ǻ�������٤���Ĥ�Τ��֤�
 */
int
anthy_get_seq_ent_wtype_freq(seq_ent_t se, wtype_t wt)
{
  int i, f;

  struct seq_ent *s = se;
  if (!s) {
    return 0;
  }
  /**/
  if (s->nr_dic_ents == 0) {
    return anthy_get_ext_seq_ent_wtype(se, wt);
  }

  f = 0;
  /* ñ�� */
  for (i = 0; i < s->nr_dic_ents; i++) {
    if (s->dic_ents[i]->order == 0 &&
	anthy_wtype_include(wt, s->dic_ents[i]->type)) {
      if (f < s->dic_ents[i]->freq) {
	f = s->dic_ents[i]->freq;
      }
    }
  }
  return f;
}

/*
 * wt���ʻ�����ʣ������Ǻ�������٤���Ĥ�Τ��֤�
 */
int
anthy_get_seq_ent_wtype_compound_freq(seq_ent_t se, wtype_t wt)
{
  int i,f;
  struct seq_ent *s = se;
  if (!s) {
    return 0;
  }
  /**/
  f = 0;
  for (i = 0; i < s->nr_compound_ents; i++) {
    if (anthy_wtype_include(wt, s->compound_ents[i]->type)) {
      if (f < s->compound_ents[i]->freq) {
	f = s->compound_ents[i]->freq;
      }
    }
  }
  return f;
}

int
anthy_get_seq_ent_indep(seq_ent_t se)
{
  int i;
  struct seq_ent *s = se;
  if (!s) {
    return 0;
  }
  if (s->nr_dic_ents == 0) {
    return anthy_get_ext_seq_ent_indep(s);
  }
  for (i = 0; i < s->nr_dic_ents; i++) {
    if (anthy_wtype_get_indep(s->dic_ents[i]->type)) {
      return 1;
    }
  }
  return 0;
}

int
anthy_get_nr_compound_ents(seq_ent_t se)
{
  if (!se) {
    return 0;
  }
  return se->nr_compound_ents;
}

compound_ent_t
anthy_get_nth_compound_ent(seq_ent_t se, int nth)
{
  if (!se) {
    return NULL;
  }
  if (nth >= 0 && nth < se->nr_compound_ents) {
    return se->compound_ents[nth];
  }
  return NULL;
}

struct elm_compound {
  int len;
  xstr str;
};

/* ���Ǥ��б������ɤߤ�Ĺ�����֤� */
static int
get_element_len(xchar xc)
{
  if (xc > '0' && xc <= '9') {
    return xc - '0';
  }
  if (xc >= 'a' && xc <= 'z') {
    return xc - 'a' + 10;
  }
  return 0;
}

static struct elm_compound *
get_nth_elm_compound(compound_ent_t ce, struct elm_compound *elm, int nth)
{
  int off = 0;
  int i, j;
  for (i = 0; i <= nth; i++) {
    /* nth���ܤ����Ǥ���Ƭ�ذ�ư���� */
    while (!(ce->str->str[off] == '_' &&
	     get_element_len(ce->str->str[off+1]) > 0)) {
      off ++;
      if (off + 1 >= ce->str->len) {
	return NULL;
      }
    }
    /* ��¤�Τؾ��������� */
    elm->len = get_element_len(ce->str->str[off+1]);
    elm->str.str = &ce->str->str[off+2];
    elm->str.len = ce->str->len - off - 2;
    for (j = 0; j < elm->str.len; j++) {
      if (elm->str.str[j] == '_') {
	elm->str.len = j;
	break;
      }
    }
    off ++;
  }
  return elm;
}

int
anthy_compound_get_nr_segments(compound_ent_t ce)
{
  struct elm_compound elm;
  int i;
  if (!ce) {
    return 0;
  }
  for (i = 0; get_nth_elm_compound(ce, &elm, i); i++);
  return i;
}

int
anthy_compound_get_nth_segment_len(compound_ent_t ce, int nth)
{
  struct elm_compound elm;
  if (get_nth_elm_compound(ce, &elm, nth)) {
    return elm.len;
  }
  return 0;
}

int
anthy_compound_get_nth_segment_xstr(compound_ent_t ce, int nth, xstr *xs)
{
  struct elm_compound elm;
  if (get_nth_elm_compound(ce, &elm, nth)) {
    if (xs) {
      *xs = elm.str;
      return 0;
    }
  }
  return -1;
}

int
anthy_compound_get_wtype(compound_ent_t ce, wtype_t *w)
{
  *w = ce->type;
  return 0;
}

int
anthy_compound_get_freq(compound_ent_t ce)
{
  return ce->freq;
}


/* �ե��ȥ���ɤ���ƤФ�� */
void
anthy_lock_dic(void)
{
  anthy_priv_dic_lock();
  anthy_priv_dic_update();
}

/* �ե��ȥ���ɤ���ƤФ�� */
void
anthy_unlock_dic(void)
{
  anthy_priv_dic_unlock();
}


dic_session_t
anthy_dic_create_session(void)
{
  return anthy_create_session();
}

void
anthy_dic_activate_session(dic_session_t d)
{
  anthy_activate_session(d);
}

void
anthy_dic_release_session(dic_session_t d)
{
  anthy_release_session(d);
  anthy_shrink_mem_dic(anthy_current_personal_dic_cache);
}

void
anthy_dic_set_personality(const char *id)
{
  anthy_current_record = anthy_create_record(id);
  anthy_current_personal_dic_cache = anthy_create_mem_dic();
  anthy_init_private_dic(id);
}


/** ���񥵥֥����ƥ������
 */
int
anthy_init_dic(void)
{
  if (dic_init_count) {
    dic_init_count ++;
    return 0;
  }
  if (anthy_init_diclib() == -1) {
    return -1;
  }

  anthy_init_wtypes();
  anthy_init_mem_dic();
  anthy_init_record();
  anthy_init_ext_ent();

  anthy_init_word_dic();
  master_dic_file = anthy_create_word_dic();
  if (!master_dic_file) {
    anthy_log(0, "Failed to create file dic.\n");
    return -1;
  }
  dic_init_count ++;
  return 0;
}

/** ���񥵥֥����ƥ�򤹤٤Ʋ���
 */
void
anthy_quit_dic(void)
{
  dic_init_count --;
  if (dic_init_count) {
    return;
  }
  if (anthy_current_record) {
    anthy_release_record(anthy_current_record);
  }
  if (anthy_current_personal_dic_cache) {
    anthy_release_mem_dic(anthy_current_personal_dic_cache);
  }
  anthy_release_private_dic();
  anthy_current_record = NULL;
  anthy_quit_mem_dic();
  anthy_quit_diclib();
}

