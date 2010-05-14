/*
 * ����⥸�塼��Υ��󥿡��ե�����
 */
#ifndef _dic_h_included_
#define _dic_h_included_

#include "xstr.h"
#include "wtype.h"

/* seq_ent_t�Υե饰 */
#define F_NONE 0
/* ̾��Υե饰 */
#define NF_FAMNAME 1
#define NF_FSTNAME 2
#define NF_UNSPECNAME 4
#define NF_NAME 7
#define NF_NUM 8
#define NF_CNAME 16
/* ��Ƭ�����������Υե饰 */
#define SF_JN 32
#define SF_NUM 64

/** ������ɤߤ��Ф���ϥ�ɥ�(sequence entry) */
typedef struct seq_ent *seq_ent_t;
/***/
typedef struct compound_ent *compound_ent_t;

/* ���Τν���������� */
int anthy_init_dic(void);
void anthy_quit_dic(void);

/* ¾�ץ������Ф�����¾���� */
void anthy_lock_dic(void);
void anthy_unlock_dic(void);

/* ʸ����γ��� */
seq_ent_t anthy_get_seq_ent_from_xstr(xstr *, int);
int anthy_get_seq_flag(seq_ent_t);
/* ����ȥ�ξ��� */
int anthy_get_nr_dic_ents(seq_ent_t, xstr *);
/* caller should free @res */
int anthy_get_nth_dic_ent_str(seq_ent_t, xstr *orig, int, xstr *res);
int anthy_get_nth_dic_ent_freq(seq_ent_t, int nth);
int anthy_get_nth_dic_ent_wtype(seq_ent_t, xstr *, int nth, wtype_t *w);
/* �ʻ� */
int anthy_get_seq_ent_pos(seq_ent_t, int pos);
int anthy_get_seq_ent_ct(seq_ent_t, int pos, int ct);
int anthy_get_seq_ent_wtype_freq(seq_ent_t, wtype_t);
int anthy_get_seq_ent_indep(seq_ent_t );
/* ʣ��� */
int anthy_get_nr_compound_ents(seq_ent_t se);
compound_ent_t anthy_get_nth_compound_ent(seq_ent_t se, int nth);
int anthy_get_seq_ent_wtype_compound_freq(seq_ent_t se, wtype_t wt);
/**/
int anthy_compound_get_wtype(compound_ent_t, wtype_t *w);
int anthy_compound_get_freq(compound_ent_t ce);
int anthy_compound_get_nr_segments(compound_ent_t ce);
int anthy_compound_get_nth_segment_len(compound_ent_t ce, int nth);
int anthy_compound_get_nth_segment_xstr(compound_ent_t ce, int nth, xstr *xs);



/** ���񥻥å����
 *
 */
typedef struct dic_session *dic_session_t;

dic_session_t anthy_dic_create_session(void);
void anthy_dic_activate_session(dic_session_t );
void anthy_dic_release_session(dic_session_t);

/* personality */
void anthy_dic_set_personality(const char *);
/**/
#define ANON_ID ""


/** ���㼭��
 */
int anthy_dic_check_word_relation(int from, int to);



#endif
