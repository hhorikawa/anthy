/* splitter�⥸�塼��Υ��󥿡��ե����� */
#ifndef _splitter_h_included_
#define _splitter_h_included_

#include <dic.h>
#include <xstr.h>
#include <wtype.h>
#include <segclass.h>

/* �ѥ�᡼�� */
#define RATIO_BASE 256
#define OCHAIRE_SCORE 5000000

/** splitter�Υ���ƥ����ȡ�
 * �ǽ�ζ������꤫��anthy_context�β����ޤ�ͭ��
 */
struct splitter_context {
  /** splitter�����ǻ��Ѥ��빽¤�� */
  struct word_split_info_cache *word_split_info;
  int char_count;
  int is_reverse;
  struct char_ent {
    xchar *c;
    int seg_border;
    int initial_seg_len;/* �ǽ��ʸ��ʬ��κݤˤ�������Ϥޤä�ʸ�᤬
			   ����Ф���Ĺ�� */
    enum seg_class best_seg_class;
    struct meta_word* best_mw; /* ����ͥ�褷�ƻȤ�����metaword */
  }*ce;
};

/* ����Υ����å��ξ��� */
enum constraint_stat {
  unchecked, ok, ng
};

/* �Ȥꤢ������Ŭ�������䤷�Ƥߤ����꤬�Ф���ʬ�ह�� */
enum metaword_type {
  /* ���ߡ� : seginfo������ʤ� */
  MW_DUMMY,
  /* wordlist��0�� or ��Ĵޤ��� */
  MW_SINGLE,
  /* �̤�metaword��Ĥ�ޤ�: metaword + ������ �ʤ� :seginfo��mw1������ */
  MW_WRAP,
  /* ʣ�����Ƭ */
  MW_COMPOUND_HEAD,
  /* ʣ����� */
  MW_COMPOUND,
  /* ʣ���ΰ�ʸ�� */
  MW_COMPOUND_LEAF,
  /* ʣ������θġ���ʸ����礷�ư�Ĥ�ʸ��Ȥ��Ƥߤ���� */
  MW_COMPOUND_PART,
  /* �Ļ���̾���Υڥ� */
  MW_NAMEPAIR,
  /* ư���Ϣ�ѷ� + ���ƻ� */
  MW_V_RENYOU_A,
  /* ư���Ϣ�ѷ� + ̾�� */
  MW_V_RENYOU_NOUN,
  /* ���� */
  MW_NUMBER,
  /**/
  MW_NOUN_NOUN_PREFIX,
  MW_OCHAIRE,
  /* ���Ҹ�δط� */
  MW_SENTENCE,
  /* �������佤���δط� */
  MW_MODIFIED,
  /**/
  MW_END
};

/*
 * meta_word: �����θ������оݤȤʤ���
 * ñ���word_list��ޤ��Τ�¾�ˤ����Ĥ��μ��ब���롥
 * 
 */
struct meta_word {
  int from, len;
  int score;
  int dep_score;
  enum seg_class seg_class;
  int mw_count;/* metaword�ο� */
  enum constraint_stat can_use; /* �������ȶ����˸٤��äƤ��ʤ� */
  enum metaword_type type;
  struct word_list *wl;
  struct meta_word *mw1, *mw2;
  xstr cand_hint;

  int nr_parts;

  /* list�Υ�� */
  struct meta_word *next;
  struct meta_word *composed;

  /* �ʲ��Ϲ�¤�򥳥ߥåȤ����Ȥ��˻Ȥ����� */
  struct meta_word *parent;
};

int anthy_init_splitter(void);
void anthy_quit_splitter(void);

void anthy_init_split_context(xstr *xs, struct splitter_context *, int is_reverse);
/*
 * mark_border(context, l1, l2, r1);
 * l1��r1�δ֤�ʸ��򸡽Ф��롢������l1��l2�δ֤϶����ˤ��ʤ���
 */
void anthy_mark_border(struct splitter_context *, int from, int from2, int to);
void anthy_commit_border(struct splitter_context *, int nr,
		   struct meta_word **mw, int *len);
void anthy_release_split_context(struct splitter_context *c);

/* ���Ф���ʸ��ξ����������� */
int anthy_get_nr_metaword(struct splitter_context *, int from, int len);
struct meta_word *anthy_get_nth_metaword(struct splitter_context *,
				 int from, int len, int nth);



#endif
