/*
 * Comments in this program are written in Japanese,
 * because this program is a Japanese input method.
 * (many Japanese gramatical terms will appear.)
 *
 * Kana-Kanji conversion engine Anthy.
 * ��̾�����Ѵ����󥸥�Anthy(���󥷡�)
 *
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2001 9/22
 * Copyright (C) 2000-2005 TABATA Yusuke, UGAWA Tomoharu
 * Copyright (C) 2004-2005 YOSHIDA Yuichi
 * Copyright (C) 2000-2005 KMC(Kyoto University Micro Computer Club)
 * Copyright (C) 2001-2002 TAKAI Kosuke, Nobuoka Takahiro
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
/*
 * Anthy���Ѵ���ǽ�ϥ饤�֥��Ȥ��ƹ�������Ƥ��ꡢ����
 * �ե�����ˤϥ饤�֥����󶡤���ؿ�(API)�����Ҥ���Ƥ��ޤ���
 *
 * �饤�֥����󶡤���ؿ��ϲ����Τ褦�ʤ�Τ�����ޤ�
 * (1)�饤�֥�����Τν��������λ������
 * (2)�Ѵ�����ƥ����Ȥκ���������
 * (3)�Ѵ�����ƥ����Ȥ��Ф���ʸ��������ꡢʸ��Ĺ���ѹ�������μ�����
 *
 * ���󥿡��ե������˴ؤ��Ƥ� doc/LIB�򻲾Ȥ��Ƥ�������
 * Anthy�Υ����ɤ����򤷤褦�Ȥ������
 * doc/GLOSSARY ���Ѹ���İ����뤳�Ȥ򴫤�ޤ�
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <dic.h>
#include <splitter.h>
#include <conf.h>
#include <ordering.h>
#include <logger.h>
#include <record.h>
#include <anthy.h>
#include "main.h"
#include <config.h>

/** Anthy�ν��������λ�������ɤ����Υե饰 */
static int is_init_ok;

static int default_encoding;

/** (API) ���Τν���� */
int
anthy_init(void)
{
  if (is_init_ok) {
    /* 2�ٽ�������ʤ��褦�� */
    return 0;
  }

  /* �ƥ��֥����ƥ���˽�������� */
  if (anthy_init_dic()) {
    anthy_log(0, "Failed to open dictionary.\n");
    return -1;
  }

  if (anthy_init_splitter()) {
    anthy_log(0, "Failed to init splitter.\n");
    return -1;
  }
  anthy_init_contexts();
  anthy_init_personality();

  default_encoding = ANTHY_EUC_JP_ENCODING;
  is_init_ok = 1;
  return 0;
}

/** (API) ���ǡ����β��� */
void
anthy_quit(void)
{
  if (!is_init_ok) {
    return ;
  }
  anthy_quit_contexts();
  anthy_quit_personality();
  anthy_quit_splitter();
  /* ¿���Υǡ�����¤�Ϥ�����allocator�ˤ�äƲ�������� */
  anthy_quit_dic();
  is_init_ok = 0;
}

/** (API) ������ܤξ�� */
void
anthy_conf_override(const char *var, const char *val)
{
  anthy_do_conf_override(var, val);
}

/** (API) personality������ */
int
anthy_set_personality(const char *id)
{
  return anthy_do_set_personality(id);
}

/** (API) �Ѵ�context�κ��� */
struct anthy_context *
anthy_create_context(void)
{
  if (!is_init_ok) {
    return 0;
  }
  return anthy_do_create_context(default_encoding);
}

/** (API) �Ѵ�context�Υꥻ�å� */
void
anthy_reset_context(struct anthy_context *ac)
{
  anthy_do_reset_context(ac);
}

/** (API) �Ѵ�context�β��� */
void
anthy_release_context(struct anthy_context *ac)
{
  anthy_do_release_context(ac);
}

/** (API) �Ѵ�ʸ��������� */
int
anthy_set_string(struct anthy_context *ac, const char *s)
{
  xstr *xs;
  int retval;

  anthy_dic_activate_session(ac->dic_session);
  /* �Ѵ��򳫻Ϥ������˸Ŀͼ����reload���� */
  anthy_reload_record();
  anthy_dic_reload_use_dic();
  anthy_dic_reload_private_dic();

  xs = anthy_cstr_to_xstr(s, ac->encoding);
  retval = anthy_do_context_set_str(ac, xs);
  anthy_free_xstr(xs);
  return retval;
}

/** (API) ʸ��Ĺ���ѹ� */
void
anthy_resize_segment(struct anthy_context *ac, int nth, int resize)
{
  anthy_dic_activate_session(ac->dic_session);
  anthy_do_resize_segment(ac, nth, resize);
}

/** (API) �Ѵ��ξ��֤μ��� */
int
anthy_get_stat(struct anthy_context *ac, struct anthy_conv_stat *s)
{
  s->nr_segment = ac->seg_list.nr_segments;
  return 0;
}

/** (API) ʸ��ξ��֤μ��� */
int
anthy_get_segment_stat(struct anthy_context *ac, int n,
		       struct anthy_segment_stat *s)
{
  struct seg_ent *seg;
  seg = anthy_get_nth_segment(&ac->seg_list, n);
  if (seg) {
    s->nr_candidate = seg->nr_cands;
    s->seg_len = seg->str.len;
    return 0;
  }
  return -1;
}

/** (API) ʸ��μ��� */
int
anthy_get_segment(struct anthy_context *ac, int nth_seg,
		  int nth_cand, char *buf, int buflen)
{
  struct seg_ent *seg;
  char *p;
  int len;

  /* ʸ�����Ф� */
  if (nth_seg < 0 || nth_seg >= ac->seg_list.nr_segments) {
    return -1;
  }
  seg = anthy_get_nth_segment(&ac->seg_list, nth_seg);

  /* ʸ�ᤫ��������Ф� */
  if ((nth_cand < 0 || nth_cand >= seg->nr_cands) &&
      nth_cand != NTH_UNCONVERTED_CANDIDATE) {
    return -1;
  }
  if (nth_cand == NTH_UNCONVERTED_CANDIDATE) {
    /* �Ѵ�����ʸ������������ */
    p = anthy_xstr_to_cstr(&seg->str, ac->encoding);
  } else {
    p = anthy_xstr_to_cstr(&seg->cands[nth_cand]->str, ac->encoding);
  }

  /* �Хåե��˽񤭹��� */
  len = strlen(p);
  if (!buf) {
    free(p);
    return len;
  }
  if (len + 1 > buflen) {
    /* �Хåե���­��ޤ��� */
    free(p);
    return -1;
  }
  strcpy(buf, p);
  free(p);
  return len;
}

/* ���٤Ƥ�ʸ�᤬���ߥåȤ��줿��check���� */
static int
commit_all_segment_p(struct anthy_context *ac)
{
  int i;
  struct seg_ent *se;
  for (i = 0; i < ac->seg_list.nr_segments; i++) {
    se = anthy_get_nth_segment(&ac->seg_list, i);
    if (se->committed < 0) {
      return 0;
    }
  }
  return 1;
}

/** (API) ʸ��γ��� */
int
anthy_commit_segment(struct anthy_context *ac, int s, int c)
{
  struct seg_ent *seg;
  if (!ac->str.str) {
    return -1;
  }
  if (s < 0 || s >= ac->seg_list.nr_segments) {
    return -1;
  }
  if (commit_all_segment_p(ac)) {
    /* ���Ǥ����ƤΥ������Ȥ����ߥåȤ���Ƥ��� */
    return -1;
  }

  anthy_dic_activate_session(ac->dic_session);
  seg = anthy_get_nth_segment(&ac->seg_list, s);
  if (c == NTH_UNCONVERTED_CANDIDATE) {
    /*
     * �Ѵ�����ʸ���󤬥��ߥåȤ��줿�Τǡ�������б����������ֹ��õ��
     */
    int i;
    for (i = 0; i < seg->nr_cands; i++) {
      if (!anthy_xstrcmp(&seg->str, &seg->cands[i]->str)) {
	c = i;
      }
    }
  }
  if (c < 0 || c >= seg->nr_cands) {
    return -1;
  }
  seg->committed = c;

  if (commit_all_segment_p(ac)) {
    /* �������٤ƤΥ������Ȥ����ߥåȤ��줿 */
    anthy_proc_commit(&ac->seg_list, &ac->split_info);
  }
  return 0;
}

/** (API) ��ȯ�� */
void
anthy_print_context(struct anthy_context *ac)
{
  anthy_do_print_context(ac, default_encoding);
}

/** (API) Anthy �饤�֥��ΥС�������ɽ��ʸ������֤�
 * ��ͭ�饤�֥��Ǥϳ����ѿ��Υ������ݡ��ȤϹ��ޤ����ʤ��ΤǴؿ��ˤ��Ƥ���
 */
const char *
anthy_get_version_string (void)
{
#ifdef VERSION
  return VERSION;
#else  /* just in case */
  return "(unknown)";
#endif
}

int
anthy_context_set_encoding(struct anthy_context *ac, int encoding)
{
#ifdef USE_UCS4
  if (!ac) {
    default_encoding = encoding;
  } else {
    ac->encoding = encoding;
  }
  return encoding;
#else
  (void)ac;
  (void)encoding;
  return ANTHY_EUC_JP_ENCODING;
#endif
}
