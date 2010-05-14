#ifndef _mkdic_h_included_
#define _mkdic_h_included_

#include <xstr.h>
#include <stdio.h>

/** ñ�� */
struct word_entry {
  /** �ʻ�̾ */
  const char *wt_name;
  /** ���� */
  int freq;
  /**/
  int feature;
  /** ñ�� */
  char *word_utf8;
  /** ����ե�������Υ��ե��å� */
  int offset;
  /** °��yomi_entry*/
  struct yomi_entry* ye;
};

/** �����ɤ� */
struct yomi_entry {
  /* �ɤߤ�ʸ���� */
  xstr *index_xstr;
  char *index_str;
  /* ����ե�������Υڡ�����Υ��ե��å� */
  int offset;
  /* �ƥ���ȥ� */
  int nr_entries;
  struct word_entry *entries;
  /**/
  struct yomi_entry *next;
  struct yomi_entry *hash_next;
};

#define YOMI_HASH 16384

/* �������� */
struct yomi_entry_list {
  /* ���Ф��Υꥹ�� */
  struct yomi_entry *head;
  /* ����ե�������θ��Ф��ο� */
  int nr_entries;
  /* ���Ф������ñ�����Ĥ�Το� */
  int nr_valid_entries;
  /* ñ��ο� */
  int nr_words;
  /**/
  struct yomi_entry *hash[YOMI_HASH];
  struct yomi_entry **ye_array;
  /**/
  int index_encoding;
  int body_encoding;
};

#define ADJUST_FREQ_UP 1
#define ADJUST_FREQ_DOWN 2
#define ADJUST_FREQ_KILL 3

/* ���������ѥ��ޥ�� */
struct adjust_command {
  int type;
  xstr *yomi;
  const char *wt;
  char *word;
  struct adjust_command *next;
};

/**/
struct yomi_entry *find_yomi_entry(struct yomi_entry_list *yl,
				   xstr *index, int create);

/* ����񤭽Ф��Ѥ���� */
void write_nl(FILE *fp, int i);

/**/
const char *get_wt_name(const char *name);

/* mkudic.c
 * ���㼭����� */
struct uc_dict *create_uc_dict(void);
void read_uc_file(struct uc_dict *ud, const char *fn);
void make_ucdict(FILE *out, struct uc_dict *uc);
/**/

/* writewords.c */
void output_word_dict(struct yomi_entry_list *yl);

#endif