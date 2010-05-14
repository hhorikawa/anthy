/*
 * �ɤߤ���ñ��ξ�����������ǡ�����¤��ե��������
 * ���Ϥ��뤿��Υ�����
 *
 * �ǡ�����¤���ѹ����䤹�����뤿���mkdic.c����ʬΥ(2005/7/8)
 *
 * Copyright (C) 2000-2005 TABATA Yusuke
 */
#include <stdio.h>
#include <string.h>
#include <file_dic.h>
#include "mkdic.h"

extern FILE *page_out, *page_index_out;
extern FILE *yomi_entry_index_out, *yomi_entry_out;

/** ��Ĥ��ɤߤ��Ф���ñ������Ƥ���Ϥ��� */
static int
output_yomi_entry(struct yomi_entry *ye)
{
  int i;
  int count = 0;

  if (!ye) {
    return 0;
  }
  /* ��ñ�����Ϥ��� */
  for (i = 0; i < ye->nr_entries; i++) {
    struct word_entry *we = &ye->entries[i];
    /**/
    if (!we->freq) {
      continue;
    }
    if (i > 0) {
      /* ����ܰʹߤ϶��򤫤�Ϥޤ� */
      count += fprintf(yomi_entry_out, " ");
    }
    /* �ʻ�����٤���Ϥ��� */
    if (i == 0 ||
	(strcmp(ye->entries[i-1].word, we->word) ||
	 strcmp(ye->entries[i-1].wt, we->wt) ||
	 ye->entries[i-1].freq != we->freq)) {
      count += fprintf(yomi_entry_out, "%s", we->wt);
      if (we->freq > 1) {
	count += fprintf(yomi_entry_out, "*%d", we->freq);
      }
      count += fprintf(yomi_entry_out, " ");
    }
    /* ñ�����Ϥ����꤬����ñ���id */
    we->offset = count + ye->offset;
    /* ñ�����Ϥ��� */
    count += fprintf(yomi_entry_out, "%s", we->word);
  }

  fputc(0, yomi_entry_out);
  return count + 1;
}

/* 2�Ĥ�ʸ����ζ�����ʬ��Ĺ������� */
static int
common_len(xstr *s1, xstr *s2)
{
  int m,i;
  if (!s1 || !s2) {
    return 0;
  }
  if (s1->len < s2->len) {
    m = s1->len;
  }else{
    m = s2->len;
  }
  for (i = 0; i < m; i++) {
    if (s1->str[i] != s2->str[i]) {
      return i;
    }
  }
  return m;
}

/*
 * 2�Ĥ�ʸ����κ�ʬ����Ϥ���
 * AAA ABBB �Ȥ���2�Ĥ�ʸ����򸫤����ˤ�
 * ABBB��AAA�Τ�����2ʸ����ä���BBB���դ�����ΤȤ���
 * \0x2BBB�Ƚ��Ϥ���롣
 */
static int
output_diff(xstr *p, xstr *c)
{
  int i, m, len = 1;
  m = common_len(p, c);
  if (p && p->len > m) {
    fprintf(page_out, "%c", p->len - m + 1);
  } else {
    fprintf(page_out, "%c", 1);
  }
  for (i = m; i < c-> len; i++) {
    char buf[8];
    len += anthy_sputxchar(buf, c->str[i], 0);
    fputs(buf, page_out);
  }
  return len;
}

static void
begin_new_page(int i)
{
  fputc(0, page_out);
  write_nl(page_index_out, i);
}

static void
output_entry_index(int i)
{
  write_nl(yomi_entry_index_out, i);
}

/** ñ�켭�����Ϥ���
 * �ޤ������ΤȤ��˼�����Υ��ե��åȤ�׻����� */
void
output_word_dict(struct yomi_entry_list *yl)
{
  xstr *prev = NULL;
  int entry_index = 0;
  int page_index = 0;
  struct yomi_entry *ye = NULL;
  int i;

  /* �ޤ����ǽ���ɤߤ��Ф��륨��ȥ�Υ���ǥå�����񤭽Ф� */
  write_nl(page_index_out, page_index);

  /* ���ɤߤ��Ф���롼�� */
  for (i = 0; i < yl->nr_valid_entries; i++) {
    ye = yl->ye_array[i];
    /* �������ڡ����γ��� */
    if ((i % WORDS_PER_PAGE) == 0 && (i != 0)) {
      page_index ++;
      prev = NULL;
      begin_new_page(page_index);
    }

    /* �ɤߤ��б�����������Ϥ��� */
    page_index += output_diff(prev, ye->index_xstr);
    output_entry_index(entry_index);
    ye->offset = entry_index;
    entry_index += output_yomi_entry(ye);
    /***/
    prev = ye->index_xstr;
  }

  /* �Ǹ���ɤߤ�λ */
  entry_index += output_yomi_entry(ye);
  write_nl(yomi_entry_index_out, entry_index);
  write_nl(page_index_out, 0);

  /**/
  printf("Total %d indexes, %d words, (%d pages).\n",
	 yl->nr_valid_entries,
	 yl->nr_words,
	 yl->nr_valid_entries / WORDS_PER_PAGE + 1);
}
