/*
 * �Ŀͼ�������Ѥδؿ���
 *
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2001 10/24
 *
 * Copyright (C) 2001-2006 TABATA Yusuke
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
#include <record.h>
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
  gIsInit = 0;
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

static char *
find_next_key(void)
{
  char *v;
  v = anthy_trie_find_next_key(anthy_private_tt_dic,
			       key_buf, MAX_KEY_LEN+32);

  if (v && v[0] == ' ' && v[1] == ' ') {
    return v;
  }
  /**/
  sprintf(key_buf, "  ");
  return NULL;
}

/** �Ŀͼ���������ä� */
void
anthy_priv_dic_delete(void)
{
  sprintf(key_buf, "  ");
  anthy_priv_dic_lock();
  /* key_buf��"  "�Ǥ���С�find_next_key()��
     �ǽ��ñ����֤� */
  while (find_next_key()) {
    anthy_trie_delete(anthy_private_tt_dic, key_buf);
    sprintf(key_buf, "  ");
  }
  anthy_priv_dic_unlock();
  /* ������μ���������� */
  if (!anthy_select_section("PRIVATEDIC", 0)) {
    anthy_release_section();
    return ;
  }
}

/** �ǽ��ñ������򤹤� */
int
anthy_priv_dic_select_first_entry(void)
{
  if (!anthy_private_tt_dic) {
    return ANTHY_DIC_UTIL_INVALID;
  }
  sprintf(key_buf, "  ");
  /* "  "�μ��Υ���ȥ꤬�ǽ�Υ���ȥ� */
  if (find_next_key()) {
    return 0;
  }
  return ANTHY_DIC_UTIL_ERROR;
}

/** �������򤵤�Ƥ���ñ��μ���ñ������򤹤� */
int
anthy_priv_dic_select_next_entry(void)
{
  if (find_next_key()) {
    return 0;
  }
  return ANTHY_DIC_UTIL_ERROR;
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
  char *src_buf = &key_buf[2];
  /* �ǽ�ζ���\0�ޤǤ򥳥ԡ����� */
  for (i = 0; src_buf[i] && src_buf[i] != ' '; i++) {
    if (i >= len - 1) {
      return NULL;
    }
    buf[i] = src_buf[i];
  }
  buf[i] = 0;
  return buf;
}

/** �������򤵤�Ƥ���ñ������٤�������� */
int
anthy_priv_dic_get_freq(void)
{
  struct word_line res;
  char *v = anthy_trie_find(anthy_private_tt_dic, key_buf);
  anthy_parse_word_line(v, &res);
  free(v);
  return res.freq;
}

/** �������򤵤�Ƥ���ñ����ʻ��������� */
char *
anthy_priv_dic_get_wtype(char *buf, int len)
{
  struct word_line res;
  char *v = anthy_trie_find(anthy_private_tt_dic, key_buf);
  anthy_parse_word_line(v, &res);
  free(v);
  if (len - 1 < (int)strlen(res.wt)) {
    return NULL;
  }
  sprintf(buf, "%s", res.wt);
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
  s++;
  snprintf(buf, len, "%s", s);
  free(v);
  return buf;
}

static int
dup_word_check(const char *v, const char *word, const char *wt)
{
  struct word_line res;

  anthy_parse_word_line(v, &res);

  /* �ɤߤ�ñ�����Ӥ��� */
  if (!strcmp(res.wt, wt) &&
      !strcmp(res.word, word)) {
    return 1;
  }
  return 0;
}

static void
do_add_entry(const char *yomi, const char *word,
	     const char *wt_name, int freq)
{
  int i = 0, ok = 0;
  char *cur;
  char word_buf[256];
  char *idx_buf = malloc(strlen(yomi) + 12);
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
  free(idx_buf);
}

static int
find_same_word(char *idx_buf, const char *yomi,
	       const char *word, const char *wt_name, int yomi_len)
{
  int found = 0;
  sprintf(idx_buf, "  %s ", yomi);
  anthy_trie_find_next_key(anthy_private_tt_dic,
			   idx_buf, yomi_len + 12);

  /* trie�Υ���ǥå�����õ�� */
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
      found = dup_word_check(v, word, wt_name);
      free(v);
      if (found) {
	break;
      }
    }
  } while (anthy_trie_find_next_key(anthy_private_tt_dic,
				    idx_buf, yomi_len + 12));

  return found;
}

/** ñ�����Ͽ����
 * ���٤�0�ξ��Ϻ��
 */
int
anthy_priv_dic_add_entry(const char *yomi, const char *word,
			 const char *wt_name, int freq)
{
  int yomi_len = strlen(yomi);
  char *idx_buf;
  int found = 0;
  int rv = ANTHY_DIC_UTIL_OK;

  if (!anthy_private_tt_dic) {
    return ANTHY_DIC_UTIL_ERROR;
  }

  if (yomi_len > MAX_KEY_LEN) {
    return ANTHY_DIC_UTIL_ERROR;
  }

  if (wt_name[0] != '#') {
    return ANTHY_DIC_UTIL_ERROR;
  }

  /* Ʊ�����Ф��θ줬���뤫�����å����� */
  idx_buf = malloc(yomi_len + 12);
  found = find_same_word(idx_buf, yomi, word, wt_name, yomi_len);

  if (freq > 0) {
    /* ��Ͽ */
    if (found) {
      /* ��¸�Τ�Τ��� */
      anthy_trie_delete(anthy_private_tt_dic, idx_buf);
      rv = ANTHY_DIC_UTIL_DUPLICATE;
    }
    do_add_entry(yomi, word, wt_name, freq);
  } else if (found) {
    /* ���٤����ǤϤʤ��Τǡ���� */
    anthy_trie_delete(anthy_private_tt_dic, idx_buf);
  }
  free(idx_buf);
  return rv;
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
