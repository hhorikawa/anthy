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

/* �����ܾ��� */
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

struct hmm_info {
  /* ���ܾ��֤Υꥹ�Ȥ����� */
  struct hmm_node** hmm_list;
  struct splitter_context *sc;
  /* HMM�Ρ��ɤΥ������� */
  allocator node_allocator;
};

/* ���ܳ�Ψ�ι��� */
static double g_transition[SEG_SIZE][SEG_SIZE] = {
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

static double
get_probability(struct hmm_node *node)
{
  double probability;
  if (anthy_seg_class_is_depword(node->seg_class)) {
    return probability = 1.0 / SEG_SIZE;
  } else if (node->seg_class == SEG_FUKUSHI) {
    probability = 1.0 / SEG_SIZE;
  } else if (node->before_node->seg_class == SEG_HEAD &&
	     (node->seg_class == SEG_SETSUZOKUGO)) {
    probability = 1.0 / SEG_SIZE;
  } else if (node->mw && node->mw->type == MW_NOUN_NOUN_PREFIX) {
    probability = g_transition[node->before_node->seg_class][node->seg_class] * g_transition[SEG_MEISHI][SEG_MEISHI];
  } else {
    probability = g_transition[node->before_node->seg_class][node->seg_class];
  }
  probability *= get_poisson(anthy_normal_length, node->mw->len - node->mw->weak_len);
  return probability;
}

static struct hmm_info*
alloc_hmm_info(struct splitter_context *sc, int size)
{
  struct hmm_info* info = (struct hmm_info*)malloc(sizeof(struct hmm_info));
  info->sc = sc;
  info->hmm_list = (struct hmm_node**)calloc(size + 1, sizeof(struct hmm_node*));
  info->node_allocator = anthy_create_allocator(sizeof(struct hmm_node), NULL);
  return info;
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
  
  /* �б�����metaword��̵������ʸƬ��Ƚ�Ǥ��� */
  node->seg_class = mw ? mw->seg_class : SEG_HEAD; 

  if (before_node) {
    node->nth = before_node->nth + 1;
    node->score_sum = before_node->score_sum + (mw ? mw->score : 0);
    node->real_probability = before_node->real_probability * get_probability(node);
    node->probability = node->real_probability * node->score_sum;
  } else {
    node->nth = 0;
    node->score_sum = 0;
    node->real_probability = 1.0;
    node->probability = node->real_probability;
  }

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
  free(info->hmm_list);
  free(info);
}

static int
cmp_node_by_type(struct hmm_node *lhs, struct hmm_node *rhs, enum metaword_type type)
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
cmp_node_by_type_to_type(struct hmm_node *lhs, struct hmm_node *rhs, enum metaword_type type1, enum metaword_type type2)
{
  if (lhs->mw->type == type1 && rhs->mw->type == type2) {
    return 1;
  } else if (lhs->mw->type == type2 && rhs->mw->type == type1) {
    return -1;
  } else {
    return 0;
  } 
}

/* �֤���
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
    if (lhs_before->mw && rhs_before->mw && lhs_before->mw->from + lhs_before->mw->len == rhs_before->mw->from + rhs_before->mw->len) {
      /* �ؽ�������줿�Ρ��ɤ��ɤ����򸫤� */
      ret = cmp_node_by_type(lhs_before, rhs_before, MW_OCHAIRE);
      if (ret != 0) return ret;
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

static void
push_node(struct hmm_info* info, struct hmm_node* new_node, int position)
{
  struct hmm_node* node = info->hmm_list[position];
  struct hmm_node* previous_node = NULL;

  if (anthy_splitter_debug_flags() & SPLITTER_DEBUG_HM) {
    print_hmm_node(info, new_node);
  }

  if (!node) {
    info->hmm_list[position] = new_node;
    return;
  }

  while (node->next) {
    /* �޴��� */
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
	  info->hmm_list[position] = new_node;
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
  node->next = new_node;
}

static int
list_length(struct hmm_info* info, int position)
{
  int length = 1;
  struct hmm_node* node = info->hmm_list[position];
  while(node) {
    ++length;
    node = node->next;
  }
  return length;
}

static void
remove_min_node(struct hmm_info* info, int position)
{
  struct hmm_node* node = info->hmm_list[position];
  struct hmm_node* previous_node = NULL;
  struct hmm_node* min_node = node;
  struct hmm_node* previous_min_node = NULL;
  int list_len = 0;

  /* ���ֳ�Ψ���㤤�Ρ��ɤ�õ��*/
  while (node) {
    if (cmp_node(node, min_node) < 0) {
      previous_min_node = previous_node;
      min_node = node;
    }
    previous_node = node;
    node = node->next;
    ++list_len;
  }
  if (previous_min_node) {
    previous_min_node->next = min_node->next;
  } else {
    info->hmm_list[position] = min_node->next;
  }
  release_hmm_node(info, min_node);
}

static void
hmm(int from, int to, struct hmm_info* info)
{
  int i;
  struct hmm_node* node;
  struct hmm_node* first_node;  

  first_node = alloc_hmm_node(info, NULL, NULL, from);
  push_node(info, first_node, from);

  /* info->hmm_list[index]�ˤ�index�ޤǤ����ܤ����äƤ���ΤǤ��äơ�
   * index��������ܤ����äƤ���ΤǤϤʤ� 
   */

  /* ���Ƥ����ܤ� */
  for (i = from; i < to; ++i) {
    for (node = info->hmm_list[i]; node; node = node->next) {
      struct meta_word *mw;
      for (mw = info->sc->word_split_info->cnode[i].mw; mw; mw = mw->next) {
	int position;
	struct hmm_node* new_node;
	if (mw->can_use != ok) continue; /* ����줿ʸ��ζ��ڤ��ޤ���metaword�ϻȤ�ʤ� */
	position = i + mw->len;
	new_node = alloc_hmm_node(info, node, mw, i);
	push_node(info, new_node, position);

	/* ��θ��䤬¿�������顢��Ψ���㤤�������� */
	if (list_length(info, position) >= NODE_MAX_SIZE) {
	  remove_min_node(info, position);
	}
      }
    }
  }

  /* ʸ������ */
  for (node = info->hmm_list[to]; node; node = node->next) {
    node->probability = node->probability * g_transition[node->seg_class][SEG_TAIL];
  }

  {
    /* �Ǹ�ޤ���ã�������ܤΤʤ��ǰ��ֳ�Ψ���礭����Τ����� */
    struct hmm_node* best_node = NULL;
    int last = to; 
    while (!info->hmm_list[last]) --last; /* �Ǹ�ޤ����ܤ��Ƥ��ʤ��ä������� */
    for (node = info->hmm_list[last]; node; node = node->next) {
      if (cmp_node(node, best_node) > 0) {
	best_node = node;
      }
    }
    if (!best_node) return;

    /* ���ܤ�դˤ��ɤ�Ĥ�ʸ����ڤ��ܤ�Ͽ */
    node = best_node;
    while (node->before_node) {
      info->sc->word_split_info->best_seg_class[node->border] =
	node->seg_class;
      anthy_mark_border_by_metaword(info->sc, node->mw);
      node = node->before_node;
    }
  }
}

void
anthy_hmm(struct splitter_context *sc, int from, int to)
{
  struct hmm_info* info = alloc_hmm_info(sc, to);
  hmm(from, to, info);
  release_hmm_info(info);
}

