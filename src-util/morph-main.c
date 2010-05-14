/* �����ѥ��������ܹ�����뤿��Υ����� 
 *
 * �Ѵ����󥸥�����������Ȥ����ᡢ�տ�Ū��
 * layer violation�����֤��Ƥ��롣
 *
 * Copyright (C) 2005-2006 TABATA Yusuke
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "anthy.h"
#include "segment.h"
/**/
#include "../src-main/main.h"
#include "../src-splitter/wordborder.h"
#include "../src-worddic/dic_ent.h"

/* ��Ω��������°������ */
#define WORD_INDEP 0
#define WORD_DEP 1

/* ñ��(��Ω��or��°��) */
struct word {
  /* WORD_* */
  int type;
  /* id�ϼ�Ω��λ��Τ�ͭ�� */
  int id;
  /* ��°���hash(WORD_INDEP)�⤷�����Ѵ����ʸ�����hash(WORD_DEP) */
  int hash;
  /* �ɤߤ�ʸ�����hash */
  int yomi_hash;
  /* �Ѵ�����ʸ���� */
  xstr *raw_xs;
  /* �Ѵ����ʸ���� */
  xstr *conv_xs;
  /* �Ѵ�����ʻ� */
  const char *wt;
  /* ʸ�᥯�饹 */
  int seg_class;
};

struct test_context {
  anthy_context_t ac;
};

static void read_file(struct test_context *tc, const char *fn);
extern void anthy_reload_record(void);

int verbose;

/**/
static void
init_test_context(struct test_context *tc)
{
  tc->ac = anthy_create_context();
}

static void
fill_conv_info(struct word *w, struct cand_elm *elm)
{
  /*w->conv_xs, w->wt*/
  struct dic_ent *de;
  if (elm->nth == -1 ||
      elm->nth >= elm->se->nr_dic_ents) {
    w->conv_xs = NULL;
    w->wt = NULL;
    return ;
  }
  if (!elm->se->dic_ents) {
    w->conv_xs = NULL;
    w->wt = NULL;
    return ;
  }
  de = elm->se->dic_ents[elm->nth];
  w->conv_xs = anthy_xstr_dup(&de->str);
  w->wt = de->wt_name;
  w->hash = anthy_xstr_hash(w->conv_xs);
}

static void
init_word(struct word *w, int type)
{
  w->type = type;
  w->raw_xs = NULL;
  w->conv_xs = NULL;
  w->wt = NULL;
  w->seg_class = -1;
}

static void
free_word(struct word *w)
{
  anthy_free_xstr(w->raw_xs);
  anthy_free_xstr(w->conv_xs);
}

/* ��Ω����� */
static void
fill_indep_word(struct word *w, struct cand_elm *elm)
{
  init_word(w, WORD_INDEP);
  /**/
  w->id = elm->id;
  /* �Ѵ������ɤߤ�������� */
  w->raw_xs = anthy_xstr_dup(&elm->str);
  w->yomi_hash = anthy_xstr_hash(w->raw_xs);
  w->hash = 0;
  /**/
  fill_conv_info(w, elm);
}

/* ��°����� */
static void
fill_dep_word(struct word *w, struct cand_elm *elm)
{
  init_word(w, WORD_DEP);
  /**/
  w->id = 0;
  w->hash = anthy_xstr_hash(&elm->str);
  w->yomi_hash = w->hash;
  w->raw_xs = anthy_xstr_dup(&elm->str);
}

static void
print_word(struct word *w)
{
  if (w->type == WORD_DEP) {
    /* ��°�� */
    printf("dep_word hash=%d ", w->hash);
    anthy_putxstrln(w->raw_xs);
    return ;
  }
  /* ��Ω�� */
  printf("indep_word id=%d hash=%d yomi_hash=%d ",
	 w->id, w->hash, w->yomi_hash);
  if (w->seg_class > -1) {
    printf("seg_class=%d ", w->seg_class);
  }
  if (w->wt) {
    printf("%s ", w->wt);
  } else {
    printf("null ");
  }
  if (w->conv_xs) {
    anthy_putxstr(w->conv_xs);
  } else {
    printf("null");
  }
  printf(" ");
  anthy_putxstrln(w->raw_xs);
}

static void
set_string(struct test_context *tc, const char *str)
{
  xstr *xs;
  int retval;
  struct anthy_context *ac = tc->ac;

  anthy_dic_activate_session(ac->dic_session);
  /* �Ѵ��򳫻Ϥ������˸Ŀͼ����reload���� */
  anthy_reload_record();

  xs = anthy_cstr_to_xstr(str, ac->encoding);  
  retval = anthy_do_context_set_str(ac, xs, 1);
  anthy_free_xstr(xs);
}

static void
print_element(int seg_class, struct cand_elm *elm)
{
  struct word w;
  if (elm->str.len == 0) {
    return ;
  }
  if (elm->id != -1) {
    /* ��Ω�� */
    fill_indep_word(&w, elm);
    w.seg_class = seg_class;
  } else {
    /* ��°�� */
    fill_dep_word(&w, elm);
  }
  print_word(&w);
  free_word(&w);
}

static void
conv_sentence(struct test_context *tc, const char *str)
{
  int i, j;

  set_string(tc, str);
  if (verbose) {
    anthy_print_context(tc->ac);
  }
  /**/
  printf("segments: %d\n", tc->ac->seg_list.nr_segments);
  /* ��ʸ����Ф��� */
  for (i = 0; i < tc->ac->seg_list.nr_segments; i++) {
    struct seg_ent *se = anthy_get_nth_segment(&tc->ac->seg_list, i);
    struct cand_ent *ce = se->cands[0];
    int seg_class = -1;
    if (ce->mw) {
      seg_class = ce->mw->seg_class;
    }

    /* �����Ǥ��Ф��� */
    for (j = 0; j < ce->nr_words; j++) {
      print_element(seg_class, &ce->elm[j]);
    }
  }
  printf("\n");
}

/* �����β��Ԥ��� */
static void
chomp(char *buf)
{
  int len = strlen(buf);
  while (len > 0) {
    char c = buf[len - 1];
    if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
      buf[len - 1] = 0;
      len --;
    } else {
      return ;
    }
  }
}

static void
read_fp(struct test_context *tc, FILE *fp)
{
  char buf[1024];
  while (fgets(buf, 1024, fp)) {
    if (buf[0] == '#') {
      continue;
    }

    if (!strncmp(buf, "\\include ", 9)) {
      read_file(tc, &buf[9]);
      continue;
    }
    chomp(buf);
    conv_sentence(tc, buf);
  }
}

static void
read_file(struct test_context *tc, const char *fn)
{
  FILE *fp;
  fp = fopen(fn, "r");
  if (!fp) {
    printf("failed to open (%s)\n", fn);
    return ;
  }
  read_fp(tc, fp);
  fclose(fp);
}

static void
print_usage(void)
{
  printf("morphological analyzer\n");
  printf(" $ ./morphological analyzer < [text-file]\n or");
  printf(" $ ./morphological analyzer [text-file]\n");
  exit(0);
}

void
parse_args(int argc, char **argv)
{
  int i;
  for (i = 1; i < argc; i++) {
    char *arg = argv[i];
    if (arg[i] == '-') {
      print_usage();
    }
  }
}

int
main(int argc, char **argv)
{
  struct test_context tc;
  int i, nr;
  anthy_init();
  anthy_set_personality("");
  init_test_context(&tc);

  /*  conv(&tc, "���̾������Ȫ�Ǥ�");*/
  /*conv(&tc, "�Ĥ���");*/

  /*read_file(&tc, "index.txt");*/
  parse_args(argc, argv);

  nr = 0;
  for (i = 1; i < argc; i++) {
    read_file(&tc, argv[i]);
    nr ++;
  }
  if (nr == 0) {
    read_fp(&tc, stdin);
  }

  return 0;
}
