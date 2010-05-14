/*
 * �Ŀͼ�������Ѥδؿ���
 *
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2001 10/24
 *
 * Copyright (C) 2001-2004 TABATA Yusuke
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
#include <stdio.h>
#include <string.h>

#include <anthy.h>
#include <conf.h>
#include <dic.h>
#include <texttrie.h>
#include <dicutil.h>

#include "dic_main.h"
#include "dic_personality.h"

/*
 * �Ŀͼ����texttrie��˳�Ǽ�����Ȥ�
 * ��  ���Ф� ������ -> ��#�ʻ�*���� ñ��פȤ���������Ȥ�
 * �ǽ��2ʸ���ζ����ñ�����Υ��������Ǥ��뤳�Ȥ��̣����
 * ��������ʬ��Ʊ�������̤��뤿����Ѥ����롣
 *
 */

/* UTF8��32ʸ�� x 3bytes */
#define MAX_KEY_LEN 96

static int gIsInit;
static int dic_util_encoding;

/* record���Ʊ��index����β����ܤ����ͤ���Ф��� */
extern struct text_trie *anthy_private_tt_dic;
/* �������򤵤�Ƥ����ɤ� */
static char key_buf[MAX_KEY_LEN+32];

/** �Ŀͼ���饤�֥����������� */
void
anthy_dic_util_init(void)
{
  if (gIsInit) {
    return ;
  }
  if (anthy_init_dic() == -1) {
    return ;
  }
  anthy_dic_set_personality("default");
  gIsInit = 1;
  dic_util_encoding = ANTHY_EUC_JP_ENCODING;
  /**/
  key_buf[0] = 0;
}

/** ����饤�֥���������� */
void
anthy_dic_util_quit(void)
{
  if (gIsInit) {
    anthy_quit_dic();
  }
}

/** ����桼�ƥ���ƥ�API�Υ��󥳡��ǥ��󥰤����ꤹ�� */
int
anthy_dic_util_set_encoding(int enc)
{
#ifdef USE_UCS4
  if (enc == ANTHY_EUC_JP_ENCODING ||
      enc == ANTHY_UTF8_ENCODING) {
    dic_util_encoding = enc;
  }
  return dic_util_encoding;
#else
  (void)enc;
  return ANTHY_EUC_JP_ENCODING;
#endif
}

void
anthy_dic_util_set_personality(const char *id)
{
  anthy_dic_set_personality(id);
}

/** �Ŀͼ���������ä� */
void
anthy_priv_dic_delete(void)
{
  key_buf[0] = 0;
  anthy_priv_dic_lock();
  while (anthy_trie_find_next_key(anthy_private_tt_dic, key_buf, 128) &&
	 key_buf[0]) {
    anthy_trie_delete(anthy_private_tt_dic, key_buf);
    key_buf[0] = 0;
  }
  anthy_priv_dic_unlock();
}

/** �ǽ��ñ������򤹤� */
int
anthy_priv_dic_select_first_entry(void)
{
  char *v;
  sprintf(key_buf, "  ");
  v = anthy_trie_find_next_key(anthy_private_tt_dic, key_buf, 128);
  if (v) {
    return 0;
  }
  return -1;
}

/** �������򤵤�Ƥ���ñ��μ���ñ������򤹤� */
int
anthy_priv_dic_select_next_entry(void)
{
  char *v;
  v = anthy_trie_find_next_key(anthy_private_tt_dic, key_buf, 128);
  if (v) {
    return 0;
  }
  return -1;
}

/** ̤���� */
int
anthy_priv_dic_select_entry(const char *index)
{
  (void)index;
  return 0;
}

/** �������򤵤�Ƥ���ñ����ɤߤ��������� */
char *
anthy_priv_dic_get_index(char *buf, int len)
{
  int i;
  for (i = 0; key_buf[i+2] && key_buf[i+2] != ' '; i++) {
    if (i >= len - 1) {
      return NULL;
    }
    buf[i] = key_buf[i+2];
  }
  buf[i] = 0;
  return buf;
}

/** �������򤵤�Ƥ���ñ������٤�������� */
int
anthy_priv_dic_get_freq(void)
{
  char *v = anthy_trie_find(anthy_private_tt_dic, key_buf);
  char *f, *e;
  int freq = 0;
  if (!v) {
    return 0;
  }
  /* ��#�ʻ�*���� ñ��פη�����ѡ������� */
  f = strchr(v, '*');
  if (!f) {
    goto out;
  }
  f ++;
  e = strchr(f, ' ');
  if (!e) {
    goto out;
  }
  *e = 0;
  freq = atoi(f);

 out:
  free(v);
  return freq;
}

/** �������򤵤�Ƥ���ñ����ʻ��������� */
char *
anthy_priv_dic_get_wtype(char *buf, int len)
{
  char *v = anthy_trie_find(anthy_private_tt_dic, key_buf);
  char *s;
  if (!v) {
    return NULL;
  }
  s = strchr(v, '*');
  if (!s) {
    free(v);
    return NULL;
  }
  *s = 0;
  snprintf(buf, len, "%s", v);
  free(v);
  return buf;
}

/** �������򤵤�Ƥ���ñ���������� */
char *
anthy_priv_dic_get_word(char *buf, int len)
{
  char *v = anthy_trie_find(anthy_private_tt_dic, key_buf);
  char *s;
  if (!v) {
    return NULL;
  }
  s = strchr(v, ' ');
  snprintf(buf, len, "%s", s);
  free(v);
  return buf;
}

static int
dup_check(const char *v, const char *word, const char *wt)
{
  int wt_len = strlen(wt);
  const char *p;
  if (strncmp(wt, v, wt_len) ||
      v[wt_len] != '*') {
    return 0;
  }
  p = v;
  while (*p && *p != ' ') {
    p++;
  }
  p++;
  if (!strcmp(p, word)) {
    return 1;
  }
  return 0;
}

/** ñ�����Ͽ���� */
int
anthy_priv_dic_add_entry(const char *yomi, const char *word,
			 const char *wt_name, int freq)
{
  int yomi_len = strlen(yomi);
  char *idx_buf;
  int found = 0;
  char word_buf[256];

  if (!anthy_private_tt_dic) {
    return ANTHY_DIC_UTIL_ERROR;
  }

  if (yomi_len > MAX_KEY_LEN) {
    return ANTHY_DIC_UTIL_ERROR;
  }

  idx_buf = malloc(yomi_len + 12);
  sprintf(idx_buf, "  %s 0", yomi);
  do {
    char *v;
    if (strncmp(&idx_buf[2], yomi, yomi_len) ||
	idx_buf[yomi_len+2] != ' ') {
      /* ���и줬�ۤʤ�Τǥ롼�׽�λ */
      break;
    }
    /* texttrie�˥����������ơ����и�ʳ�����פ��Ƥ��뤫������å� */
    v = anthy_trie_find(anthy_private_tt_dic, idx_buf);
    if (v) {
      found = dup_check(v, word, wt_name);
      free(v);
      if (found) {
	break;
      }
    }
  } while (anthy_trie_find_next_key(anthy_private_tt_dic, idx_buf, yomi_len + 12));

  /* ̵���ä��Τ���Ͽ */
  if (!found) {
    int i = 0, ok = 0;
    char *cur;
    do {
      sprintf(idx_buf, "  %s %d", yomi, i);
      cur = anthy_trie_find(anthy_private_tt_dic, idx_buf);
      if (cur) {
	free(cur);
      } else {
	ok = 1;
      }
      i++;
    } while (!ok);
    sprintf(word_buf, "%s*%d %s", wt_name, freq, word);
    anthy_trie_add(anthy_private_tt_dic, idx_buf, word_buf);
  }
  free(idx_buf);
  return ANTHY_DIC_UTIL_OK;
}

const char *
anthy_dic_util_get_anthydir(void)
{
  return anthy_conf_get_str("ANTHYDIR");
}

/* look���ޥ�ɤμ���򸡺����뤿��δؿ� */
static char *
do_search(FILE *fp, const char *word)
{
  char buf[32];
  char *res = NULL;
  int word_len = strlen(word);
  while (fgets(buf, 32, fp)) {
    int len = strlen(buf);
    buf[len - 1] = 0;
    len --;
    if (len > word_len) {
      continue;
    }
    if (!strncasecmp(buf, word, len)) {
      if (res) {
	free(res);
      }
      res = strdup(buf);
    }
  }
  return res;
}

/* look���ޥ�ɤμ���򸡺�����API */
char *
anthy_dic_search_words_file(const char *word)
{
  FILE *fp;
  char *res;
  const char *words_dict_fn = anthy_conf_get_str("WORDS_FILE");
  if (!words_dict_fn) {
    return NULL;
  }
  fp = fopen(words_dict_fn, "r");
  if (!fp) {
    return NULL;
  }
  res = do_search(fp, word);
  fclose(fp);
  return res;
}
