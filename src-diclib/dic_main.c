/*
 * Anthy�μ���饤�֥����濴
 *
 * Copyright (C) 2000-2004 TABATA Yusuke
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

#include "dic_main.h"
#include "dic_personality.h"
#include "dic_ent.h"

/**/
static int dic_init_count;

/* ���� */
/* ��personality�Ƕ�ͭ�����ե����뼭�� */
static struct file_dic *master_dic_file;
/* �ƥѡ����ʥ�ƥ����Ȥμ��� */
struct mem_dic *anthy_current_personal_dic_cache;/* ����å��� */
static struct mem_dic *private_dic;/* �Ŀͼ��� */
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

/*
 * �Ŀͼ����ñ����ɤ߹���
 */
static void
add_word_to_private_dic(struct mem_dic * dd)
{
  int nr, i;
  xstr *word,*yomi,*tmp;
  wtype_t wt;
  char *cstr;
  int freq;
  struct seq_ent *se;

  nr = anthy_get_nr_values();
  if (nr < 3) {
    return ;
  }
  yomi = anthy_get_index_xstr();
  for (i = 0; i + 2 < nr; i += 3) {
    const char *wt_name;
    word = anthy_get_nth_xstr(i);/* ñ�� */
    tmp = anthy_get_nth_xstr(i+1);/* �ʻ�̾ */
    cstr = anthy_xstr_to_cstr(tmp, 0);
    wt_name = anthy_type_to_wtype(cstr, &wt);
    free(cstr);
    freq = anthy_get_nth_value(i+2);/* ���� */

    se = anthy_mem_dic_alloc_seq_ent_by_xstr(dd, yomi);
    /* �Ŀͼ�������ʻ줬anthy�ˤȤä�̤�Τʤ�wt_name��NULL*/
    if (wt_name) {
      anthy_mem_dic_push_back_dic_ent(se, word, wt, wt_name, freq, 0);
    }
  }
}

/** �Ŀͼ�����ɤ�
 * record����Ŀͼ���˥ǡ����������
 */
void
anthy_dic_reload_private_dic(void)
{
  if (private_dic) {
    anthy_release_mem_dic(private_dic);
  }
  private_dic = anthy_create_mem_dic();
  if (anthy_select_section("PRIVATEDIC", 0) == -1) {
    return ;
  }
  if (anthy_select_first_column() == -1) {
    return ;
  }
  do {
    add_word_to_private_dic(private_dic);
  } while (anthy_select_next_column() != -1);
}

static int
init_file_dic(void)
{
  const char *fn;
  fn = anthy_conf_get_str("DIC_FILE");
  if (!fn) {
    anthy_log(0, "dic file not specified.\n");
    return -1;
  }
  master_dic_file = anthy_create_file_dic(fn);
  if (!master_dic_file) {
    anthy_log(0, "Failed to create file dic.\n");
    return -1;
  }
  return 0;
}

static struct seq_ent *
cache_get_seq_ent_to_mem_dic(struct mem_dic *dd, xstr *xs)
{
  struct seq_ent *seq;
  /* ����å������̵���Τǳ��� */
  seq = anthy_mem_dic_alloc_seq_ent_by_xstr(dd, xs);

  /* file_dic����μ��� */
  anthy_file_dic_fill_seq_ent_by_xstr(master_dic_file, xs, seq);

  /*���٤Ƥ�dic_ent���Ф���flag��׻�����*/
  calc_seq_flags(seq);

  return seq;
}

struct seq_ent *
anthy_cache_get_seq_ent(xstr *xs)
{
  struct seq_ent *seq, *t;

  /* ����å�����˴��ˤ���Ф�����֤� */
  seq = anthy_mem_dic_find_seq_ent_by_xstr(anthy_current_personal_dic_cache, xs);
  if (seq) {
    return seq;
  }

  /* ����å���ˤ�̵���ΤǶ��̼��񤫤�μ��� */
  seq = cache_get_seq_ent_to_mem_dic(anthy_current_personal_dic_cache, xs);

  /* �Ŀͼ��񤫤�μ��� */
  t = 0;
  if (private_dic) {
    t = anthy_mem_dic_find_seq_ent_by_xstr(private_dic, xs);
  }
  if (t) {
    int i;
    /* �Ŀͼ�����ˤ⤢�ä��Τǡ���������ƥ��ԡ� */
    for (i = 0; i < t->nr_dic_ents; i++) {
      anthy_mem_dic_push_back_dic_ent(seq,
				      &t->dic_ents[i]->str,
				      t->dic_ents[i]->type,
				      t->dic_ents[i]->wt_name,
				      t->dic_ents[i]->freq, 0);
    }
  }

  if (seq->nr_dic_ents == 0 && seq->nr_compound_ents == 0) {
    /* ̵���ʥ���ȥ����������Τ�cache������ */
    anthy_mem_dic_release_seq_ent(anthy_current_personal_dic_cache, xs);
    return 0;
  }

  return seq;
}

int anthy_dic_check_word_relation(int from, int to)
{
  return anthy_file_dic_check_word_relation(master_dic_file, from, to);
}

seq_ent_t
anthy_get_seq_ent_from_xstr(xstr *xs)
{
  struct seq_ent *s;
  if (!xs) {
    return 0;
  }
  s = anthy_cache_get_seq_ent(xs);
  if (!s || (s->nr_dic_ents == 0 &&
	     s->nr_compound_ents == 0)) {
    return anthy_get_ext_seq_ent_from_xstr(xs);
  }
  return s;
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
anthy_get_seq_ent_id(seq_ent_t se)
{
  if (!se) {
    return -1;
  }
  return se->id;
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
anthy_get_nth_dic_ent_freq(seq_ent_t se, int n)
{
  struct seq_ent *s = se;
  if (!s || s->nr_dic_ents <= n) {
    return 0;
  }
  return s->dic_ents[n]->freq;
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
anthy_get_nth_dic_ent_id(seq_ent_t se, int nth)
{
  if (!se) {
    return 0;
  }
  if (nth < se->nr_dic_ents) {
    return se->dic_ents[nth]->id;
  }
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
  int i,f;
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
    if (anthy_wtypecmp(wt, s->dic_ents[i]->type)) {
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
    if (anthy_wtypecmp(wt, s->compound_ents[i]->type)) {
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

/** ���񥵥֥����ƥ������
 */
int
anthy_init_dic(void)
{
  if (dic_init_count) {
    dic_init_count ++;
    return 0;
  }
  anthy_do_conf_init();

  anthy_init_wtypes();
  anthy_init_ext_ent();
  anthy_init_mem_dic();
  anthy_init_file_dic();
  anthy_init_use_dic();
  anthy_init_record();
  anthy_init_xchar_tab();
  anthy_init_xstr();

  if (init_file_dic() == -1) {
    anthy_log(0, "Failed to init dic cache.\n");
    return -1;
  }
  anthy_init_hashmap(master_dic_file);
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
    return ;
  }
  if (anthy_current_record) {
    anthy_release_record(anthy_current_record);
  }
  if (anthy_current_personal_dic_cache) {
    anthy_release_mem_dic(anthy_current_personal_dic_cache);
  }
  if (private_dic) {
    anthy_release_mem_dic(private_dic);
    private_dic = 0;
  }
  anthy_quit_hashmap();
  anthy_current_record = 0;
  anthy_quit_use_dic();
  anthy_quit_mem_dic();
  anthy_quit_allocator();
  anthy_conf_free();
  anthy_quit_xstr();
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
}
