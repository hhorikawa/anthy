/*
 * HMM�ȥӥ��ӥ��르�ꥺ��(viterbi algoritgm)�ˤ�ä�
 * ʸ��ζ��ڤ����ꤷ�ƥޡ������롣
 *
 * ��������ƤӽФ����ؿ�
 *  anthy_hmm()
 *
 * Copyright (C) 2004-2006 YOSHIDA Yuichi
 * Copyright (C) 2006 TABATA Yusuke
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
/*
 * ����ƥ��������¸�ߤ���meta_word��Ĥʤ��ǥ���դ������ޤ���
 * (���Υ���դΤ��Ȥ��ƥ���(lattice/«)�⤷���ϥȥ�ꥹ(trellis)�ȸƤӤޤ�)
 * meta_word�ɤ�������³������դΥΡ��ɤȤʤꡢ��¤��hmm_node��
 * ��󥯤Ȥ��ƹ�������ޤ���
 *
 * �����Ǥν����ϼ�����Ĥ����Ǥǹ�������ޤ�
 * (1) ����դ������Ĥġ��ƥΡ��ɤؤ���ã��Ψ�����
 * (2) ����դ���(��)���餿�ɤäƺ�Ŭ�ʥѥ������
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <alloc.h>
#include <xstr.h>
#include <segclass.h>
#include <splitter.h>
#include <parameter.h>
#include "wordborder.h"

float anthy_normal_length = 20.0; /* ʸ��δ��Ԥ����Ĺ�� */

#define NODE_MAX_SIZE 50

struct feature_list {
  /* ���ޤΤȤ��������ϰ�� */
  int index;
};

/* ����դΥΡ���(���ܾ���) */
struct hmm_node {
  int border; /* ʸ������Τɤ�����Ϥޤ���֤� */
  int nth; /* ���ߤ����Ĥ��ʸ�ᤫ */
  enum seg_class seg_class; /* ���ξ��֤��ʻ� */


  double real_probability;  /* �����˻��ޤǤγ�Ψ(ʸ�������̵��) */
  double probability;  /* �����˻��ޤǤγ�Ψ(ʸ�������ͭ��) */
  int score_sum; /* ���Ѥ��Ƥ���metaword�Υ������ι��*/


  struct hmm_node* before_node; /* ����������ܾ��� */
  struct meta_word* mw; /* �������ܾ��֤��б�����meta_word */

  struct hmm_node* next; /* �ꥹ�ȹ�¤�Τ���Υݥ��� */
};

struct node_list_head {
  struct hmm_node *head;
  int nr_nodes;
};

struct hmm_info {
  /* ���ܾ��֤Υꥹ�Ȥ����� */
  struct node_list_head *hmm_node_list;
  struct splitter_context *sc;
  /* HMM�Ρ��ɤΥ������� */
  allocator node_allocator;
};

/* �����νŤߤι��� */
static double g_transition[SEG_SIZE*SEG_SIZE] = {
#include "transition.h"
};


static void
print_hmm_node(struct hmm_info *info, struct hmm_node *node)
{
  if (!node) {
    printf("**hmm_node (null)*\n");
    return ;
  }
  printf("**hmm_node score_sum=%d, nth=%d*\n", node->score_sum, node->nth);
  printf("probability=%.128f\n", node->real_probability);
  if (node->mw) {
    anthy_print_metaword(info->sc, node->mw);
  }
}

static double
get_poisson(double lambda, int r)
{
  int i;
  double result;

  /* �פ���˥ݥ諒��ʬ�� */
  result = pow(lambda, r) * exp(-lambda);
  for (i = 2; i <= r; ++i) {
    result /= i;
  }

  return result;
}

/* ʸ��η������饹������Ĵ������ */
static double
get_form_bias(struct hmm_node *node)
{
  struct meta_word *mw = node->mw;
  double bias;
  int wrap = 0;
  /* wrap����Ƥ�����������Τ�Ȥ� */
  while (mw->type == MW_WRAP) {
    mw = mw->mw1;
    wrap = 1;
  }
  /* ʸ��Ĺ�ˤ��Ĵ�� */
  bias = get_poisson(anthy_normal_length, mw->len);
  /* ��°������٤ˤ��Ĵ�� */
  bias *= (mw->dep_score + 1000);
  bias /= 2000;
  return bias;
}

static void
fill_bigram_feature(struct feature_list *features, int from, int to)
{
  features->index = from * SEG_SIZE + to;
}

static void
build_feature_list(struct hmm_node *node, struct feature_list *features)
{
  fill_bigram_feature(features, node->before_node->seg_class,
		      node->seg_class);
}

static double
calc_probability(struct feature_list *features)
{
  return g_transition[features->index];
}

static double
get_transition_probability(struct hmm_node *node)
{
  struct feature_list features;
  double probability;
  if (anthy_seg_class_is_depword(node->seg_class)) {
    /* ��°��Τߤξ�� */
    return probability = 1.0 / SEG_SIZE;
  } else if (node->seg_class == SEG_FUKUSHI || 
	     node->seg_class == SEG_RENTAISHI) {
    /* ���졢Ϣ�λ� */
    probability = 2.0 / SEG_SIZE;
  } else if (node->before_node->seg_class == SEG_HEAD &&
	     (node->seg_class == SEG_SETSUZOKUGO)) {
    /* ʸƬ -> ��³�� */
    probability = 1.0 / SEG_SIZE;
  } else if (node->mw && node->mw->type == MW_NOUN_NOUN_PREFIX) {
    /* ̾����Ƭ����̾����դ���meta_word */
    build_feature_list(node, &features);
    probability = calc_probability(&features);
    /**/
    fill_bigram_feature(&features, SEG_MEISHI, SEG_MEISHI);
    probability *= calc_probability(&features);
  } else {
    /* ����¾�ξ�� */
    build_feature_list(node, &features);
    probability = calc_probability(&features);
  }
  /* ʸ��η����Ф���ɾ�� */
  probability *= get_form_bias(node);
  return probability;
}

static struct hmm_info*
alloc_hmm_info(struct splitter_context *sc, int size)
{
  int i;
  struct hmm_info* info = (struct hmm_info*)malloc(sizeof(struct hmm_info));
  info->sc = sc;
  info->hmm_node_list = (struct node_list_head*)
    malloc((size + 1) * sizeof(struct node_list_head));
  for (i = 0; i < size + 1; i++) {
    info->hmm_node_list[i].head = NULL;
    info->hmm_node_list[i].nr_nodes = 0;
  }
  info->node_allocator = anthy_create_allocator(sizeof(struct hmm_node), NULL);
  return info;
}

static void
calc_node_parameters(struct hmm_node *node)
{
  
  /* �б�����metaword��̵������ʸƬ��Ƚ�Ǥ��� */
  node->seg_class = node->mw ? node->mw->seg_class : SEG_HEAD; 

  if (node->before_node) {
    /* �������ܤ���Ρ��ɤ������� */
    node->nth = node->before_node->nth + 1;
    node->score_sum = node->before_node->score_sum +
      (node->mw ? node->mw->score : 0);
    node->real_probability = node->before_node->real_probability *
      get_transition_probability(node);
    node->probability = node->real_probability * node->score_sum;
  } else {
    /* �������ܤ���Ρ��ɤ�̵����� */
    node->nth = 0;
    node->score_sum = 0;
    node->real_probability = 1.0;
    node->probability = node->real_probability;
  }
}

static struct hmm_node*
alloc_hmm_node(struct hmm_info *info, struct hmm_node* before_node,
	       struct meta_word* mw, int border)
{
  struct hmm_node* node;
  node = anthy_smalloc(info->node_allocator);
  node->before_node = before_node;
  node->border = border;
  node->next = NULL;
  node->mw = mw;

  calc_node_parameters(node);

  return node;
}

static void 
release_hmm_node(struct hmm_info *info, struct hmm_node* node)
{
  anthy_sfree(info->node_allocator, node);
}

static void
release_hmm_info(struct hmm_info* info)
{
  anthy_free_allocator(info->node_allocator);
  free(info->hmm_node_list);
  free(info);
}

static int
cmp_node_by_type(struct hmm_node *lhs, struct hmm_node *rhs,
		 enum metaword_type type)
{
  if (lhs->mw->type == type && rhs->mw->type != type) {
    return 1;
  } else if (lhs->mw->type != type && rhs->mw->type == type) {
    return -1;
  } else {
    return 0;
  }
}

static int
cmp_node_by_type_to_type(struct hmm_node *lhs, struct hmm_node *rhs,
			 enum metaword_type type1, enum metaword_type type2)
{
  if (lhs->mw->type == type1 && rhs->mw->type == type2) {
    return 1;
  } else if (lhs->mw->type == type2 && rhs->mw->type == type1) {
    return -1;
  } else {
    return 0;
  } 
}

/*
 * �Ρ��ɤ���Ӥ���
 *
 ** �֤���
 * 1: lhs��������Ψ���⤤
 * 0: Ʊ��
 * -1: rhs��������Ψ���⤤
 */
static int
cmp_node(struct hmm_node *lhs, struct hmm_node *rhs)
{
  struct hmm_node *lhs_before = lhs;
  struct hmm_node *rhs_before = rhs;
  int ret;

  if (lhs && !rhs) return 1;
  if (!lhs && rhs) return -1;
  if (!lhs && !rhs) return 0;

  while (lhs_before && rhs_before) {
    if (lhs_before->mw && rhs_before->mw &&
	lhs_before->mw->from + lhs_before->mw->len == rhs_before->mw->from + rhs_before->mw->len) {
      /* �ؽ�������줿�Ρ��ɤ��ɤ����򸫤� */
      ret = cmp_node_by_type(lhs_before, rhs_before, MW_OCHAIRE);
      if (ret != 0) return ret;
      /* ��åפ��줿��Τϳ�Ψ���㤤 */
      /*
	ret = cmp_node_by_type(lhs, rhs, MW_WRAP);
	if (ret != 0) return -ret;
      */

      /* COMPOUND_PART����COMPOUND_HEAD��ͥ�� */
      ret = cmp_node_by_type_to_type(lhs_before, rhs_before, MW_COMPOUND_HEAD, MW_COMPOUND_PART);
      if (ret != 0) return ret;
    } else {
      break;
    }
    lhs_before = lhs_before->before_node;
    rhs_before = rhs_before->before_node;
  }

  /* �Ǹ�����ܳ�Ψ�򸫤� */
  if (lhs->probability > rhs->probability) {
    return 1;
  } else if (lhs->probability < rhs->probability) {
    return -1;
  } else {
    return 0;
  }
}

/*
 * ������Υ�ƥ����˥Ρ��ɤ��ɲä���
 */
static void
push_node(struct hmm_info* info, struct hmm_node* new_node, int position)
{
  struct hmm_node* node;
  struct hmm_node* previous_node = NULL;

  if (anthy_splitter_debug_flags() & SPLITTER_DEBUG_HM) {
    print_hmm_node(info, new_node);
  }

  /* ��Ƭ��node��̵�����̵�����ɲ� */
  node = info->hmm_node_list[position].head;
  if (!node) {
    info->hmm_node_list[position].head = new_node;
    info->hmm_node_list[position].nr_nodes ++;
    return;
  }

  while (node->next) {
    /* ;�פʥΡ��ɤ��ɲä��ʤ�����λ޴��� */
    if (new_node->seg_class == node->seg_class &&
	new_node->border == node->border) {
      /* segclass��Ʊ���ǡ��Ϥޤ���֤�Ʊ���ʤ� */
      switch (cmp_node(new_node, node)) {
      case 0:
      case 1:
	/* ������������Ψ���礭�����ؽ��ˤ���Τʤ顢�Ť��Τ��֤�����*/
	if (previous_node) {
	  previous_node->next = new_node;
	} else {
	  info->hmm_node_list[position].head = new_node;
	}
	new_node->next = node->next;
	release_hmm_node(info, node);
	break;
      case -1:
	/* �����Ǥʤ��ʤ��� */
	release_hmm_node(info, new_node);
	break;
      }
      return;
    }
    previous_node = node;
    node = node->next;
  }
  /* �Ǹ�ΥΡ��ɤθ����ɲ� */
  node->next = new_node;
  info->hmm_node_list[position].nr_nodes ++;
}

/* ���ֳ�Ψ���㤤�Ρ��ɤ�õ��*/
static void
remove_min_node(struct hmm_info *info, struct node_list_head *node_list)
{
  struct hmm_node* node = node_list->head;
  struct hmm_node* previous_node = NULL;
  struct hmm_node* min_node = node;
  struct hmm_node* previous_min_node = NULL;

  /* ���ֳ�Ψ���㤤�Ρ��ɤ�õ�� */
  while (node) {
    if (cmp_node(node, min_node) < 0) {
      previous_min_node = previous_node;
      min_node = node;
    }
    previous_node = node;
    node = node->next;
  }

  /* ���ֳ�Ψ���㤤�Ρ��ɤ������� */
  if (previous_min_node) {
    previous_min_node->next = min_node->next;
  } else {
    node_list->head = min_node->next;
  }
  release_hmm_node(info, min_node);
  node_list->nr_nodes --;
}

/* ������ӥ��ӥ��르�ꥺ�����Ѥ��Ʒ�ϩ������ */
static void
choose_path(struct hmm_info* info, int to)
{
  /* �Ǹ�ޤ���ã�������ܤΤʤ��ǰ��ֳ�Ψ���礭����Τ����� */
  struct hmm_node* node;
  struct hmm_node* best_node = NULL;
  int last = to; 
  while (!info->hmm_node_list[last].head) {
    /* �Ǹ��ʸ���ޤ����ܤ��Ƥ��ʤ��ä������� */
    --last;
  }
  for (node = info->hmm_node_list[last].head; node; node = node->next) {
    if (cmp_node(node, best_node) > 0) {
      best_node = node;
    }
  }
  if (!best_node) {
    return;
  }

  /* ���ܤ�դˤ��ɤ�Ĥ�ʸ����ڤ��ܤ�Ͽ */
  node = best_node;
  while (node->before_node) {
    info->sc->word_split_info->best_seg_class[node->border] =
      node->seg_class;
    anthy_mark_border_by_metaword(info->sc, node->mw);
    node = node->before_node;
  }
}

static void
build_hmm_graph(struct hmm_info* info, int from, int to)
{
  int i;
  struct hmm_node* node;
  struct hmm_node* left_node;

  /* �����Ȥʤ�Ρ��ɤ��ɲ� */
  node = alloc_hmm_node(info, NULL, NULL, from);
  push_node(info, node, from);

  /* info->hmm_list[index]�ˤ�index�ޤǤ����ܤ����äƤ���ΤǤ��äơ�
   * index��������ܤ����äƤ���ΤǤϤʤ� 
   */

  /* ���Ƥ����ܤ򺸤��� */
  for (i = from; i < to; ++i) {
    for (left_node = info->hmm_node_list[i].head; left_node;
	 left_node = left_node->next) {
      struct meta_word *mw;
      /* iʸ���ܤ���ã����hmm_node�Υ롼�� */

      for (mw = info->sc->word_split_info->cnode[i].mw; mw; mw = mw->next) {
	int position;
	struct hmm_node* new_node;
	/* iʸ���ܤ����meta_word�Υ롼�� */

	if (mw->can_use != ok) {
	  continue; /* ����줿ʸ��ζ��ڤ��ޤ���metaword�ϻȤ�ʤ� */
	}
	position = i + mw->len;
	new_node = alloc_hmm_node(info, left_node, mw, i);
	push_node(info, new_node, position);

	/* ��θ��䤬¿�������顢��Ψ���㤤�������� */
	if (info->hmm_node_list[position].nr_nodes >= NODE_MAX_SIZE) {
	  remove_min_node(info, &info->hmm_node_list[position]);
	}
      }
    }
  }

  /* ʸ������ */
  for (node = info->hmm_node_list[to].head; node; node = node->next) {
    struct feature_list features;
    fill_bigram_feature(&features, node->seg_class, SEG_TAIL);
    node->probability = node->probability *
      calc_probability(&features);
  }
}

void
anthy_hmm(struct splitter_context *sc, int from, int to)
{
  struct hmm_info* info = alloc_hmm_info(sc, to);
  build_hmm_graph(info, from, to);
  choose_path(info, to);
  release_hmm_info(info);
}

