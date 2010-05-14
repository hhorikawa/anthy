/* segment(ʸ��) ����� */
#ifndef _segment_h_included_
#define _segment_h_included_

#include <segclass.h>
#include <wtype.h>
#include <xstr.h>
#include <dic.h>

/** ����ι������� */
struct cand_elm {
  int nth; /* -1�ΤȤ��ϼ��񤫤�γ�����Ƥ��äƤ��ʤ� */
  wtype_t wt;
  seq_ent_t se;
  int ratio;/* ���٤�ɾ������ݤ˻��Ѥ�����Ψ */
  xstr str;/* �Ѵ��оݤ�ʸ���� */
  int id;
};

/** ��Ĥθ�����������롣
 * anthy_release_cand_ent()�ǲ�������
 */
struct cand_ent {
  /** ����Υ����� */
  int score;
  /** �Ѵ����ʸ���� */
  xstr str;
  /** ���Ǥο� */
  int nr_words;
  /** ��������������Ǥ����� */
  struct cand_elm *elm;
  /** ��Ω�����Υ���ǥå��� */
  int core_elm_index;
  /** ����Υե饰 CEF_? */
  unsigned int flag;
  struct meta_word *mw;
};

/* ����(cand_ent)�Υե饰 */
#define CEF_NONE           0
#define CEF_OCHAIRE        0x00000001
#define CEF_SINGLEWORD     0x00000002
#define CEF_HIRAGANA       0x00000004
#define CEF_KATAKANA       0x00000008
#define CEF_GUESS          0x00000010
#define CEF_USEDICT        0x00000020
#define CEF_COMPOUND       0x00000040
#define CEF_COMPOUND_PART  0x00000080
#define CEF_BEST           0x00000100

/** Context���¸�ߤ���ʸ�����
 * release_seg_ent�ǲ�������
 */
struct seg_ent {
  /* str�μ��Τ�context��ˤ��� */
  xstr str;
  /* commit���줿������ֹ桢��ο��ξ��Ϥޤ����ߥåȤ���Ƥ��ʤ� */
  int committed;
  
  /* ��������� */
  int nr_cands;/* ����ο� */
  struct cand_ent **cands;/* ���� */

  int from, len;/* len == str.len */
  /* �Ѵ��оݤȤʤ���ʬ�������դ���������̤��ζ������ʤɤ�̵�뤹�� */
  int core_from, core_len;

  /* ʸ��ι��� */
  int nr_metaword;
  struct meta_word **mw_array;

  /* hmm�ǰ������Ӥ��ɤ��ä����饹 */
  enum seg_class best_seg_class;
  
  struct meta_word *best_mw;

  struct seg_ent *prev, *next;
};

/** ʸ��Υꥹ�� */
struct segment_list {
  int nr_segments;
  struct seg_ent list_head;
};

/* ������������(̵�̤��������Ƥ��ޤä������) */
void anthy_release_cand_ent(struct cand_ent *s);

/**/
struct seg_ent *anthy_get_nth_segment(struct segment_list *c, int );
void anthy_print_candidate(struct cand_ent *ce);

/* compose.c */
/* �������Ф� */
void anthy_do_make_candidates(struct seg_ent *e);

#endif
