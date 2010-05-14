#ifndef _dic_ent_h_included_
#define _dic_ent_h_included_

#include <wtype.h>
#include "dic.h"

/** ����ñ�� */
struct dic_ent {
  wtype_t type; /** �ʻ� */
  const char *wt_name;
  int freq; /** ���� */
  int id; /** id */
  xstr str; /** �Ѵ���̤�ʸ���� */
  struct seq_ent *se;/** °����seq_ent */
  struct dic_ent *next;
};

/** ʣ���Υ���ȥ� */
struct compound_ent {
  /* �ʻ� */
  wtype_t type;
  /* ʸ����(compound_ent��Ʊ���˳��ݤ��졢Ʊ���˲��������) */
  xstr *str;
  /* ���� */
  int freq;
};

/**����ʸ�����Ʊ���۵��������
 * seq_ent_t �Ȥ��ƻ��Ȥ����
 */
struct seq_ent {
  xstr str;/* �ɤ� */
  int mask;/* �ɤ�dic_session�ˤ�äƻ��Ѥ���Ƥ��뤫�Υޥ��� */

  int seq_type; /** ST_(type) */
  int flags; /** ?F_* */

  /** dic_ent������ */
  int nr_dic_ents;
  struct dic_ent **dic_ents;
  /** compound_ent������ */
  int nr_compound_ents;
  struct compound_ent **compound_ents;

  /* °������꼭�� */
  struct mem_dic *md;
  /* ���꼭�����hash chain */
  struct seq_ent *next;
};

/* ext_ent.c */
void anthy_init_ext_ent(void);
/**/
int anthy_get_nr_dic_ents_of_ext_ent(struct seq_ent *se,xstr *xs);
int anthy_get_nth_dic_ent_str_of_ext_ent(seq_ent_t ,xstr *,int ,xstr *);
int anthy_get_nth_dic_ent_wtype_of_ext_ent(xstr *,int ,wtype_t *);
int anthy_get_ext_seq_ent_wtype(struct seq_ent *, wtype_t );
seq_ent_t anthy_get_ext_seq_ent_from_xstr(xstr *x);
int anthy_get_ext_seq_ent_pos(struct seq_ent *, int);
int anthy_get_ext_seq_ent_indep(struct seq_ent *);
int anthy_get_ext_seq_ent_ct(struct seq_ent *, int, int);
int anthy_get_ext_seq_ent_wtype(struct seq_ent *se, wtype_t w);

#endif
