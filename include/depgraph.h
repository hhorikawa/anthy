/* ��°�쥰��դΥǡ�����¤ */
#ifndef _depgraph_h_included_
#define _depgraph_h_included_

#include <segclass.h>
#include <wtype.h>

struct dep_transition {
  /** ������ΥΡ��ɤ��ֹ� 0�ξ��Ͻ�ü */
  int next_node;
  /** ���ܤΥ����� */
  int trans_ratio;
  /** �ʻ� */
  int pos;
  /** ���ѷ� */
  int ct;
  /* ��°�쥯�饹 */
  enum dep_class dc;

  int head_pos;
  int weak;
};

struct dep_branch {
  /* ���ܾ�����°������� */
  /** ����Ĺ */
  int nr_strs;
  /** ���ܾ������� */
  xstr **str;
  
  /** ������ΥΡ��� */
  int nr_transitions;
  struct dep_transition *transition;
};

struct dep_node {
  int nr_branch;
  struct dep_branch *branch;
};

/** ��Ω����ʻ�Ȥ��θ��³����°��Υ������ΥΡ��ɤ��б� */
struct wordseq_rule {
  wtype_t wt; /* ��Ω����ʻ� */
  int ratio; /* ����Υ��������Ф�����Ψ */
  int node_id; /* ���μ�Ω��θ���³����°�쥰�����ΥΡ��ɤ�id */
};

/** ��°�쥰��դΥե������Ǥη��� */
struct ondisk_wordseq_rule {
  char wt[8];
  /* ���Τդ��Ĥϥͥåȥ���Х��ȥ������� */
  int ratio;
  int node_id;
};

/* ��°�쥰��� */
struct file_dep {
  int file_size;
  char* file_ptr;

  int nrRules;
  int nrNodes;

  /* ��Ω�줫�����³�롼�� */
  struct ondisk_wordseq_rule *rules;
  /* ��°��֤���³�롼�� */
  struct dep_node* nodes;
};

#endif
