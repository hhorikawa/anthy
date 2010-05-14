/*
 * �ؽ�������ʤɤ�������뤿��Υǡ����١���
 * ʸ����(xstr)�򥭡��ˤ��ƹ�®�˹�(column)�򸡺����뤳�Ȥ��Ǥ��롥
 * ʣ���Υ����������Ĥ��Ȥ��Ǥ����ؽ��ΰ㤦�ե������ʤɤ��б�����
 *  (��������� * ʸ���� -> ��)
 * �ƹԤ�ʸ���󤫿����������ˤʤäƤ���
 *
 * �֥ѥȥꥷ�����ȥ饤�פȤ����ǡ�����¤����Ѥ��Ƥ��롣
 * ��������θ����ʤɤ򰷤äƤ��붵�ʽ�򻲾ȤΤ���
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
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2002 1/18
 * Copyright (C) 2000-2004 TABATA Yusuke
 * Copyright (C) 2000-2003 UGAWA Tomoharu
 * Copyright (C) 2001-2002 TAKAI Kosuke
 */
/*
 * �ѡ����ʥ�ƥ�""��ƿ̾�ѡ����ʥ�ƥ��Ǥ��ꡤ
 * �ե�����ؤ��ɤ߽񤭤ϹԤ�ʤ���
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include <alloc.h>
#include <conf.h>
#include <ruleparser.h>
#include <record.h>
#include <logger.h>

#include "dic_personality.h"

/* �Ŀͼ���򥻡��֤���ե�����̾��suffix */
#ifdef USE_UCS4
#define ENCODING_SUFFIX ".utf8"
#else
#define ENCODING_SUFFIX ""
#endif


enum val_type {
  RT_EMPTY, RT_VAL, RT_XSTR, RT_XSTRP
};

/* �� */
struct record_val {
  enum val_type type;
  union {
    xstr str;
    int val;
    xstr* strp;
  } u;
};

/* �� */
struct record_column {
  xstr key;
  int nr_vals;
  struct record_val *vals;
};

/* trie node������ */
struct trie_node {
  struct trie_node *l;
  struct trie_node *r;
  int bit;
  struct record_column column;
  struct trie_node *lru_prev, *lru_next; /* ξü�롼�� */
  int dirty; /* LRU �Τ���� used, sused �ӥå� */
};

/* trie tree��root */
struct trie_root {
  struct trie_node root;
  allocator node_ator;
};

#define LRU_USED  0x01
#define LRU_SUSED 0x02
#define PROTECT   0x04 /* ��ʬ�񤭽Ф����˻Ȥ�(LRU�Ȥϴط��ʤ�)
			*   ��ʬ�񤭽Ф��Ǥϡ��ե�����˽񤭽Ф�����
			*   �ե�������¾�Υץ�������Ͽ����������
			*   �ɤ߹��ࡣ����ˤ�äơ����줫���ɲä���
			*   ���Ȥ���Ρ��ɤ��ä����Τ��ɤ�
			*/
/*
 * LRU:
 *   USED:  �����ǻȤ�줿
 *   SUSED: ��¸���줿 used �ӥå�
 *
 * LRU�ꥹ�Ⱦ�Ǥϡ� USED ��ɬ���ꥹ����Ƭ���¤�Ǥ��뤬�� SUSED ��
 * �ե饰�ʤ��ΥΡ��ɤȺ��ߤ��Ƥ����ǽ�������롣
 *
 * n�Ĥ�Ĥ��褦�˻��ꤵ�줿����ư��
 *    1. used > n
 *        LRU �ꥹ�Ȥ���Ƭ���� n ���ܰʹߤ�ä�
 *    2. used + sused > n
 *        used -> �Ĥ�
 *        sused -> sused �ե饰���
 *        ����ʳ� -> �ä�
 *    3. ����ʳ�
 *        ���ƻĤ�
 * �ե�����˽񤭽Ф����ˡ� used || sused -> sused �Ȥ��ƽ񤭽Ф�
 */

/** ��������� */
struct record_section {
  const char *name;
  struct trie_root cols;
  struct record_section *next;
  int lru_nr_used, lru_nr_sused; /* LRU �� */
};

/** �ǡ����١��� */
struct record_stat {
  struct record_section section_list; /* section�Υꥹ��*/
  struct record_section *cur_section;
  struct trie_root xstrs; /* xstr �� intern ���뤿��� trie */
  struct trie_node *cur_column;
  int column_dirty; /* cur_column ����¸��ɬ�פ����뤫 */
  /**/
  int is_anon;
  int initial_read; /* �ǽ���ɤ߹��ߤ�λ������ */
  const char *id;         /* �ѡ����ʥ�ƥ���id */
  char *base_fn; /* ���ܥե����� ���Хѥ� */
  char *journal_fn; /* ��ʬ�ե����� ���Хѥ� */
  char *lock_fn; /* ��å��ե����� ���Хѥ� */
  /**/
  time_t base_timestamp; /* ���ܥե�����Υ����ॹ����� */
  int lock_fd;      /* ��ʬ�ե������å��ѤΥե�����ǥ�������ץ� */
  int last_update;  /* ��ʬ�ե�����κǸ���ɤ������ */
  time_t journal_timestamp; /* ��ʬ�ե�����Υ����ॹ����� */
};

/**/
#define ANON_ID ""
/* ��ʬ��100KB�ۤ�������ܥե�����إޡ��� */
#define FILE2_LIMIT 102400


/*
 * xstr �� intern:
 *  �Ŀͤ���( record_stat ����)��ʸ����� intern ���롣����ϡ�
 *  ����������¾�ˡ��ǡ����١����� flush ���˥ǡ����١�����
 *  ͳ�褹�� xstr ��̵���ˤʤ�Τ��ɤ���Ū�����롣
 *  �������äơ��ǡ����١����� flush ���Ǥ� xstr �� intern ��
 *  �Υǡ����١��� xstrs �Ϥ��Τޤ���¸���롣
 *  
 *  xstrs: xstr �� intern �ѤΥǡ����١���
 *         column �� key �� intern ���줿 xstr �Ȥ��ƻȤ���
 *         column �� value �ϻ����ʤ���
 *                    (����Ū�ˤϻ��ȥ����󥿤�Ĥ��Ƥ⤤������)
 *  ����: intern_xstr()
 */

/*
 * ��ʬ�񤭽Ф�:
 *  �ǡ����١�������¸��ʣ���� anthy �饤�֥����󥯤���
 *  �ץ����γؽ������Ʊ���Τ���ˡ��ؽ�����ι��������
 *  ���ե�����˽񤭽Ф���
 *
 * �����ܥե�����  �Ť� anthy �γؽ������Ʊ��������
 *                 ��ʬ�����Ŭ�Ѥ��븵�Ȥʤ�ե����롣
 *                 ����Ū�ˤϵ�ư���������ɤ߹��ࡣ
 *                 ���Υץ������ǥե�����1��base�ȸƤ֤��Ȥ����롣
 * ����ʬ�ե�����  ���ܥե�������Ф��빹������
 *                 �ǡ����١������Ф��빹�������ߥåȤ���뤿�Ӥ�
 *                 �ɤ߽񤭤���롣
 *                 ���Υץ������ǥե�����2��journal�ȸƤ֤��Ȥ����롣
 *  ��������:
 *     �ǡ����١������Ф��빹�������ߥåȤ����ȡ��ޤ���ʬ�ե�����
 *     ��¾�Υץ������ɲä�������������ɤ߹��ߡ����θ�˼�ʬ��
 *     ���ߥåȤ���������ʬ�ե�����˽񤭽Ф���
 *     �����ϥ�å��ե�������Ѥ��ƥ��ȥߥå��˹Ԥ��롣�ޤ���
 *     ���ܥե����롢��ʬ�ե�����Ȥ⡢��å����äƤ���֤���
 *     �����ץ󤷤Ƥ��ƤϤ����ʤ���
 *  �ɲäȺ��:
 *     �ɲäϤ��Ǥ˥����ǹ������줿 column �򥳥ߥåȤˤ�ä�
 *     ����˽񤭽Ф����ᡢ
 *       1. ���ߥå��о� column �ʳ���ʬ�ե�����ξ����
 *       2. ���ߥå��о� column ��ʬ�ե�����˽񤭽Ф�
 *     �Ȥ��롣����Ϥޤ������� column ���ĤäƤ�����֤ǥ��ߥå�
 *     ���Ԥ���(����׵�򥳥ߥåȤȤ��ư���)���ᡢ
 *       1. ����ξ����ʬ�ե�����˽񤭽Ф�
 *       2. ��ʬ�ե�������ɤ߹��ߤˤ�����׵��¹Ԥ���
 *     �Ȥ��롣
 *  ���ܥե�����ι���:
 *     ��ʬ�ե����뤬�����������粽����ȡ���ʬ�ե�����ξ����
 *     ���ܥե������ȿ�Ǥ��ƺ�ʬ�ե��������ˤ��롣
 *     ��������ץ���:
 *       ��ʬ�ե�����˽񤭽Ф���Ԥä��塢��ʬ�ե�������礭����Ĵ�١�
 *       ���粽���Ƥ���С����ΤȤ��Υ����Υǡ����١���(����ˤ�
 *       ���Ƥκ�ʬ�ե�����ι�����Ŭ�Ѥ���Ƥ���)����ܥե������
 *       �񤭽Ф���
 *     ����ʳ��Υץ���:
 *       ��ʬ�ե�������ɤ����ˡ����ܥե����뤬��������Ƥ��뤫��
 *       �ե�����Υ����ॹ����פ�Ĵ�١���������Ƥ���С����ߥå�
 *       ���줿���������ľ���˹����ե�������ɲä���������
 *       �ǡ����١����� flush ��������ܥե����롢��ʬ�ե������
 *       �ɤ߹���ľ����
 *       �ǡ����١����� flush �ˤ�ꡢ 
 *           ��cur_column ��̵���ˤʤ� (NULL �ˤʤ�)
 *           ��cur_section ��ͭ��������¸�����(section�ϲ������ʤ�)
 *           ��xstr �� intern ���Ƥ������¸�����
 *                              (���٤Ƥ� xstr �� intern ����Ƥ���Ϥ�)
 *   ��ɡ������ͤˤʤ�:
 *     if (���ܥե����뤬��������Ƥ���) {
 *             ��ʬ�ե�����إ��ߥåȤ��줿������񤭽Ф�;
 *             �ǡ����١����Υե�å���;
 *             ���ܥե�������ɹ��Ⱥ�ʬ�ե�����κǽ��ɹ����֥��ꥢ;
 *             ��ʬ�ե�������ɹ��Ⱥ�ʬ�ե�����κǽ��ɹ����ֹ���;
 *     } else {
 *             if (�ɲ�) {
 *                     ��ʬ�ե�������ɹ��Ⱥ�ʬ�ե�����κǽ��ɹ����ֹ���;
 *                     ��ʬ�ե�����ؤν񤭽Ф�;
 *             } else {
 *                     ��ʬ�ե�����ؤν񤭽Ф�;
 *                     ��ʬ�ե�������ɹ��Ⱥ�ʬ�ե�����κǽ��ɹ����ֹ���;
 *             }
 *     }
 *     if (��ʬ�ե����뤬�礭��) {
 *             ���ܥե�����ؤν񤭽Ф�;
 *             ��ʬ�ե�����Υ��ꥢ;
 *     }
 */

static allocator record_ator;

/* trie����� */
static void init_trie_root(struct trie_root *n);
static int trie_key_nth_bit(xstr* key, int n);
static int trie_key_first_diff_bit_1byte(xchar c1, xchar c2);
static int trie_key_first_diff_bit(xstr *k1, xstr *k2);
static int trie_key_cmp(xstr *k1, xstr *k2);
static void trie_key_dup(xstr *dst, xstr *src);
static void trie_column_init(struct record_column *rc);
static void trie_column_free(struct record_column *rc);
static struct trie_node *trie_find(struct trie_root *root, xstr *key);
static struct trie_node *trie_insert(struct trie_root *root, xstr *key,
				     int dirty, int *nr_used, int *nr_sused);
static void trie_remove(struct trie_root *root, xstr *key,
			int *nr_used, int *nr_sused);
static struct trie_node *trie_first(struct trie_root *root);
static struct trie_node *trie_next(struct trie_root *root,
				   struct trie_node *cur);
static void trie_remove_all(struct trie_root *root,
			    int *nr_used, int *nr_sused);
static void trie_remove_old(struct trie_root *root, int count,
			    int* nr_used, int* nr_sused);
static void trie_mark_used(struct trie_root *root, struct trie_node *n,
			   int *nr_used, int *nr_sused);


/* 
 * �ȥ饤�μ���
 * struct trie_node�Τ���column�ʳ�����ʬ��column.key�����
 * ����λ���trie_column_free��Ȥä�column�����Ƥ����
 */

#define PUTNODE(x) ((x) == &root->root ? printf("root\n") : anthy_putxstrln(&(x)->column.key))
static int
debug_trie_dump(FILE* fp, struct trie_node* n)
{
  int cnt = 0;
  char buf[1024];

  if (n->l->bit > n->bit) {
    cnt = debug_trie_dump(fp, n->l);
  } else {
    if (n->l->column.key.len == -1) {
      if (fp) {
	fprintf(fp, "root\n");
      }
    } else {
      if (fp) {
	anthy_sputxstr(buf, &n->l->column.key);
	fprintf(fp, "%s\n", buf);
      }
      cnt = 1;
    }
  }

  if (n->r->bit > n->bit) {
    return cnt + debug_trie_dump(fp, n->r);
  } else {
    if (n->r->column.key.len == -1) {
      if(fp) {
	fprintf(fp, "root\n");
      }
    } else {
      if(fp) {
	anthy_sputxstr(buf, &n->r->column.key);
	fprintf(fp, "%s\n", buf);
      }
      return cnt + 1;
    }
  }

  return cnt;
}

static void
init_trie_root(struct trie_root *root)
{
  struct trie_node* n;
  root->node_ator = anthy_create_allocator(sizeof(struct trie_node), NULL);
  n = &root->root;
  n->l = n;
  n->r = n;
  n->bit = 0;
  n->lru_next = n;
  n->lru_prev = n;
  n->dirty = 0;
  trie_column_init(&n->column);
  n->column.key.len = -1;
}

/*
 * bit0: 0
 * bit1: head�Υ�������0
 * bit2: ʸ����Υӥå�0
 * bit3: ʸ����Υӥå�1
 *   ...
 * ʸ����Ĺ��ۤ����0
 */
static int
trie_key_nth_bit(xstr* key, int n)
{
  switch (n) {
  case 0:
    return 0;
  case 1:
    return key->len + 1; /* key->len == -1 ? 0 : non-zero */
  default:
    {
      int pos;
      n -= 2;
      pos = n / (sizeof(xchar) << 3);
      if (pos >= key->len) {
	return 0;
      }
      return key->str[pos] & (1 << (n % (sizeof(xchar) << 3)));
    }
  }
}

/* c1 == c2 �ǤϸƤ�ǤϤ����ʤ� */
static int
trie_key_first_diff_bit_1byte(xchar c1, xchar c2)
{
  int i;
  int ptn;
  for (i = 0, ptn = c1 ^ c2; !(ptn & 1); i++, ptn >>= 1 )
    ;
  return i;
}

/*
 * k1 == k2 �ǤϸƤ�ǤϤ����ʤ�
 * ki->str[0 .. (ki->len - 1)]��0�Ϥʤ��Ȳ���
 */
#define MIN(a,b) ((a)<(b)?(a):(b))
static int
trie_key_first_diff_bit(xstr *k1, xstr *k2)
{
  int len;
  int i;

  len = MIN(k1->len, k2->len);
  if (len == -1) {
    return 1;
  }
  for ( i = 0 ; i < len ; i++ ){
    if (k1->str[i] != k2->str[i]) {
      return (2 + (i * (sizeof(xchar) << 3)) + 
	      trie_key_first_diff_bit_1byte(k1->str[i], k2->str[i]));
    }
  }
  if (k1->len < k2->len) {
    return (2 + (i * (sizeof(xchar) << 3)) +
	    trie_key_first_diff_bit_1byte(0, k2->str[i]));
  } else {
    return (2 + (i * (sizeof(xchar) << 3)) +
	    trie_key_first_diff_bit_1byte(k1->str[i], 0));
  }
}
#undef MIN

static int
trie_key_cmp(xstr *k1, xstr *k2)
{
  if (k1->len == -1 || k2->len == -1) {
    return k1->len - k2->len;
  }
  return anthy_xstrcmp(k1, k2);
}

static void
trie_key_dup(xstr *dst, xstr *src)
{
  dst->str = anthy_xstr_dup_str(src);
  dst->len = src->len;
}

/*
 * ���Ĥ���ʤ���� 0 
 */
static struct trie_node *
trie_find(struct trie_root *root, xstr *key)
{
  struct trie_node *p;
  struct trie_node *q;

  p = &root->root;
  q = p->l;
  while (p->bit < q->bit) {
    p = q;
    q = trie_key_nth_bit(key, p->bit) ? p->r : p->l;
  }
  return trie_key_cmp(&q->column.key,key) ? 0 : q; 
}

/*
 * ��Ĺ�ޥå��Τ��������ؿ�
 *  key ��õ�����ơ��Ϥ�ư��פ��ʤ��ʤä��Ρ��ɤ��֤���
 */
static struct trie_node *
trie_find_longest (struct trie_root* root, xstr *key)
{
  struct trie_node *p;
  struct trie_node *q;

  p = &root->root;
  q = p->l;
  while (p->bit < q->bit) {
    p = q;
    q = trie_key_nth_bit(key, p->bit) ? p->r : p->l;
  }

  return q;
}

/* 
 * �ɲä����Ρ��ɤ��֤�
 * ���Ǥ�Ʊ���������ĥΡ��ɤ�����Ȥ��ϡ��ɲä�����0���֤�
 */
static struct trie_node *
trie_insert(struct trie_root *root, xstr *key,
	    int dirty, int *nr_used, int *nr_sused)
{
  struct trie_node *n;
  struct trie_node *p;
  struct trie_node *q;
  int i;

  p = &root->root;
  q = p->l;
  while (p->bit < q->bit) {
    p = q;
    q = trie_key_nth_bit(key, p->bit) ? p->r : p->l;
  }
  if (trie_key_cmp(&q->column.key,key) == 0) {
    /* USED > SUSED > 0 �Ƕ�������Ĥ� */
    if (dirty == LRU_USED) {
      trie_mark_used(root, q, nr_used, nr_sused);
    } else if (q->dirty == 0) {
      q->dirty = dirty;
    }
    return 0;
  }
  i = trie_key_first_diff_bit(&q->column.key, key);
  p = &root->root;
  q = p->l;
  while (p->bit < q->bit && i > q->bit) {
    p = q;
    q = trie_key_nth_bit(key, p->bit) ? p->r : p->l;
  }
  n = anthy_smalloc(root->node_ator);
  trie_column_init(&n->column);
  trie_key_dup(&n->column.key, key);
  n->bit = i;
  if (trie_key_nth_bit(key, i)) {
    n->l = q;
    n->r = n;
  } else {
    n->l = n;
    n->r = q;
  }
  if (p->l == q) {
    p->l = n;
  } else {
    p->r = n;
  }

  /* LRU �ν��� */
  if (dirty == LRU_USED) {
    root->root.lru_next->lru_prev = n;
    n->lru_prev = &root->root;
    n->lru_next = root->root.lru_next;
    root->root.lru_next = n;
    (*nr_used)++;
  } else {
    root->root.lru_prev->lru_next = n;
    n->lru_next = &root->root;
    n->lru_prev = root->root.lru_prev;
    root->root.lru_prev = n;
    if (dirty == LRU_SUSED) {
      (*nr_sused)++;
    }
  }
  n->dirty = dirty;
  return n;
}

/* 
 * �Ρ��ɤ򸫤Ĥ���Ⱥ������
 * ������trie_column_free��Ƥӡ�������ޤ�ǡ�����ʬ��free����
 *
 * �ǡ����ȥΡ��ɤ������롣
 * ����оݤΥǡ����Ϻ���оݤΥΡ��ɤ˳�Ǽ����Ƥ���Ȥ�
 * �¤�ʤ����Ȥ���ա�
 * 1. ����оݤ��դ���ĥΡ��ɤ˺���оݤ��դ��ޤޤ�Ƥ���Ȥ�
 *  ����оݤΥΡ��ɤϡ��ҤؤλޤΤ����������Τ���ޤ�Ƥ��Ϥ��ƻ��
 * 2. ����оݤ��դ���ĥΡ��ɤ�����˺���оݤ��դ��ޤޤ�Ƥ���Ȥ�
 *  1. �˲ä��ơ�����оݤ��դ��ĥΡ��ɤ򻦤��ơ�����˺��
 *  �оݤΥΡ��ɤ����оݤ��դ��ĥΡ��ɤΰ��֤˰�ư����������
 */
static void
trie_remove(struct trie_root *root, xstr *key, 
	    int *nr_used, int *nr_sused)
{
  struct trie_node *p;
  struct trie_node *q;
  struct trie_node **pp = NULL; /* gcc �� warning ���� */
  struct trie_node **qq;
  p = &root->root;
  qq = &p->l;
  q = *qq;
  while (p->bit < q->bit) {
    pp = qq;
    p = q;
    qq = trie_key_nth_bit(key,p->bit) ? &p->r : &p->l;
    q = *qq;
  }
  if (trie_key_cmp(&q->column.key, key) != 0) {
    return ;
  }
  if (p != q) {
    /* case 2. */
    struct trie_node *r;
    struct trie_node *s;
    r = &root->root;
    s = r->l;
    while (s != q) {
      r = s;
      s = trie_key_nth_bit(key, r->bit) ? r->r : r->l;
    }
    *pp = (p->r == q) ? p->l : p->r;
    p->l = q->l;
    p->r = q->r;
    p->bit = q->bit;
    if (trie_key_nth_bit(key, r->bit)) {
      r->r = p;
    } else {
      r->l = p;
    }
    p = q;
  } else {
    *pp = (p->r == q) ? p->l : p->r;
  }
  p->lru_prev->lru_next = p->lru_next;
  p->lru_next->lru_prev = p->lru_prev;
  if (p->dirty == LRU_USED) {
    (*nr_used)--;
  } else if (p->dirty == LRU_SUSED) {
    (*nr_sused)--;
  }
  trie_column_free(&p->column);
  anthy_sfree(root->node_ator, p);
}

/* head�ʳ��ΥΡ��ɤ��ʤ���� 0 ���֤� */
static struct trie_node *
trie_first (struct trie_root *root)
{
  return root->root.lru_next == &root->root ?
    NULL : root->root.lru_next;
}

/* ���ΥΡ��ɤ��ʤ���� 0 ���֤� */
static struct trie_node *
trie_next (struct trie_root *root,
	   struct trie_node *cur)
{
  return cur->lru_next == &root->root ? 0 : cur->lru_next;
}

/*
 * head�ʳ����ƤΥΡ��ɤ�������
 * ������trie_column_free��Ƥӡ�������ޤ�ǡ�����ʬ��free����
 */
static void 
trie_remove_all (struct trie_root *root,
		 int *nr_used, int *nr_sused)
{
  struct trie_node* p;
  for (p = root->root.lru_next; p != &root->root; p = p->lru_next) {
    trie_column_free(&p->column);
  }
  anthy_free_allocator(root->node_ator);
  init_trie_root(root);
  *nr_used = 0;
  *nr_sused = 0;
}

/*
 * LRU �ꥹ�Ȥ���Ƭ���� count ���ܤޤǤ�Ĥ��ƻĤ���������
 */
static void
trie_remove_old (struct trie_root *root, int count, 
		 int *nr_used, int *nr_sused)
{
  struct trie_node *p;
  struct trie_node *q;

  if (*nr_used > count) {
    for (p = root->root.lru_next; count; count--, p = p->lru_next)
      ;
    /* p ���� head �ޤǤ�ä� */
    for ( ; p != &root->root; p = q) {
      q = p->lru_next;
      trie_remove(root, &p->column.key, nr_used, nr_sused);
    }
  } else if (*nr_used + *nr_sused > count) {
    for (p = root->root.lru_next; p->dirty == LRU_USED; p = p->lru_next)
      ; 
    /*
     * p ���� root �ޤ�  sused    -> dirty := 0
     *                   ����ʳ� -> �ä�
     */
    for ( ; p != &root->root; p = q) {
      q = p->lru_next;
      if (p->dirty == LRU_SUSED) {
	p->dirty = 0;
      } else {
	trie_remove(root, &p->column.key, nr_used, nr_sused);
      }
    }
    *nr_sused = 0;
  }
}      

static void
trie_mark_used (struct trie_root *root, struct trie_node *n,
		int *nr_used, int *nr_sused)
{
  switch(n->dirty) {
  case LRU_USED:
    break;
  case LRU_SUSED:
    (*nr_sused)--;
    /* fall through */
  default:
    n->dirty = LRU_USED;
    (*nr_used)++;
    break;
  }
  n->lru_prev->lru_next = n->lru_next;
  n->lru_next->lru_prev = n->lru_prev;
  root->root.lru_next->lru_prev = n;
  n->lru_next = root->root.lru_next;
  root->root.lru_next = n;
  n->lru_prev = &root->root;
}

/*
 * �ȥ饤�μ����Ϥ����ޤ�
 */

static xstr *
do_get_index_xstr(struct record_stat *rec)
{
  if (!rec->cur_column) {
    return 0;
  }
  return &rec->cur_column->column.key;
}

static struct record_section*
do_select_section(struct record_stat *rst, const char *name, int flag)
{
  struct record_section *rsc;

  for (rsc = rst->section_list.next; rsc; rsc = rsc->next) {
    if (!strcmp(name, rsc->name)) {
      return rsc;
    }
  }

  if (flag) {
    rsc = malloc(sizeof(struct record_section));
    rsc->name = strdup(name);
    rsc->next = rst->section_list.next;
    rst->section_list.next = rsc;
    rsc->lru_nr_used = 0;
    rsc->lru_nr_sused = 0;
    init_trie_root(&rsc->cols);
    return rsc;
  }

  return NULL;
}

static struct trie_node* 
do_select_longest_column(struct record_section *rsc, xstr *name)
{
  struct trie_node *mark, *found;
  xstr xs;
  int i;

  mark = trie_find_longest(&rsc->cols, name);
  xs.str = name->str;
  for (i = mark->column.key.len; i > 1; i--) {
    /* �롼�ȥΡ��ɤ� i == 1 �ǥޥå�����Τǽ���
     * trie_key_nth_bit ����
     */
    xs.len = i;
    found = trie_find(&rsc->cols, &xs);
    if (found) {
      return found;
    }
  }
  return NULL;
}

static struct trie_node* 
do_select_column(struct record_section* rsc, xstr *name,
		 int flag, int dirty)
{
  struct trie_node *node;

  if (flag) {
    node = trie_insert(&rsc->cols, name, dirty,
		       &rsc->lru_nr_used, &rsc->lru_nr_sused);
    if (node) {
      node->column.nr_vals = 0;
      node->column.vals = 0;
    } else {
      node = trie_find(&rsc->cols, name);
    }
  } else {
    node = trie_find(&rsc->cols, name);
  }
  return node;
}

static void 
do_mark_column_used(struct record_section* rsc, struct trie_node* node)
{
  trie_mark_used(&rsc->cols, node, &rsc->lru_nr_used, &rsc->lru_nr_sused);
}

static void
do_truncate_section(struct record_stat *s, int count)
{
  if (!s->cur_section) {
    return;
  }

  trie_remove_old(&s->cur_section->cols, count,
		  &s->cur_section->lru_nr_used,
		  &s->cur_section->lru_nr_sused);
}


static struct trie_node*
do_select_first_column(struct record_section *rsc)
{
  return trie_first(&rsc->cols);
}

static struct trie_node*
do_select_next_column(struct record_section *rsc, 
  struct trie_node* node)
{
  return trie_next(&rsc->cols, node);
}


static int
do_get_nr_values(struct trie_node *node)
{
  if (!node)
    return 0;
  return node->column.nr_vals;
}

static struct record_val *
get_nth_val_ent(struct trie_node *node, int n, int f)
{
  struct record_column *col;
  col = &node->column;
  if (n < 0) {
    return NULL;
  }
  if (n < do_get_nr_values(node)) {
    return &col->vals[n];
  }
  if (f) {
    int i;
    col->vals = realloc(col->vals, sizeof(struct record_val)*(n + 1));
    for (i = col->nr_vals; i < n+1; i++) {
      col->vals[i].type = RT_EMPTY;
    }
    col->nr_vals = n + 1;
    return &col->vals[n];
  }
  return NULL;
}

static void
free_val_contents(struct record_val* v)
{
  switch (v->type) {
  case RT_XSTR:
    anthy_free_xstr_str(&v->u.str);
    break;
  case RT_XSTRP:
  case RT_VAL:
  case RT_EMPTY:
  default:
    break;
  }
}

static void
do_set_nth_value(struct trie_node *node, int nth, int val)
{
  struct record_val *v = get_nth_val_ent(node, nth, 1);
  if (!v) {
    return ;
  }
  free_val_contents(v);
  v->type = RT_VAL;
  v->u.val = val;
}

static int
do_get_nth_value(struct trie_node *node, int n)
{
  struct record_val *v = get_nth_val_ent(node, n, 0);
  if (v && v->type == RT_VAL) {
    return v->u.val;
  }
  return 0;
}

static xstr*
intern_xstr (struct trie_root* xstrs, xstr* xs)
{
  struct trie_node* node;
  int dummy;

  node = trie_find(xstrs, xs);
  if (!node) 
    node = trie_insert(xstrs, xs, 0, &dummy, &dummy);
  return &node->column.key;
}

static void
do_set_nth_xstr (struct trie_node *node, int nth, xstr *xs,
		 struct trie_root* xstrs)
{
  struct record_val *v = get_nth_val_ent(node, nth, 1);
  if (!v){
    return ;
  }
  free_val_contents(v);
  v->type = RT_XSTRP;
  v->u.strp = intern_xstr(xstrs, xs);
}

static void
do_truncate_column (struct trie_node* node, int n)
{
  int i;
  if (n < node->column.nr_vals) {
    for (i = n; i < node->column.nr_vals; i++) {
      free_val_contents(node->column.vals + i);
    }
    node->column.vals = realloc(node->column.vals, 
				sizeof(struct record_val)* n);
    node->column.nr_vals = n;
  }
}

static void
do_remove_column (struct record_section* rsc,
		  struct trie_node* node)
{
  xstr* xs;
  xs = anthy_xstr_dup(&node->column.key);
  trie_remove(&rsc->cols, &node->column.key, 
	      &rsc->lru_nr_used, &rsc->lru_nr_sused);

  anthy_free_xstr(xs);
}

static xstr *
do_get_nth_xstr(struct trie_node *node, int n)
{
  struct record_val *v = get_nth_val_ent(node, n, 0);
  if (v) {
    if (v->type == RT_XSTR) {
      return &v->u.str;
    } else if (v->type == RT_XSTRP) {
      return v->u.strp;
    }
  }
  return 0;
}

static void
lock_record (struct record_stat* rs)
{
  struct flock lck;

  if (rs->is_anon) {
    return ;
  }

  /* �ե������å�����ˡ��¿�����뤬��������ˡ��cygwin�Ǥ�ư���ΤǺ��Ѥ��� */
  rs->lock_fd = open(rs->lock_fn, O_CREAT|O_RDWR, S_IREAD|S_IWRITE);
  if (rs->lock_fd == -1) {
    return ;
  }

  lck.l_type = F_WRLCK;
  lck.l_whence = (short) 0;
  lck.l_start = (off_t) 0;
  lck.l_len = (off_t) 1;
  if (fcntl(rs->lock_fd, F_SETLKW, &lck) == -1) {
    close(rs->lock_fd);
    rs->lock_fd = -1;
    return;
  }
}

static void
unlock_record (struct record_stat* rs)
{
  if (rs->lock_fd != -1) {
    close(rs->lock_fd);
    rs->lock_fd = -1;
  }
}

static void
check_anthy_dir(void)
{
  const char *hd;
  char *dn;
  struct stat st;
  hd = anthy_conf_get_str("HOME");
  dn = alloca(strlen(hd) + 10);
  sprintf(dn, "%s/.anthy", hd);
  if (stat(dn, &st) || !S_ISDIR(st.st_mode)) {
    int r;
    /*fprintf(stderr, "Anthy: Failed to open anthy directory(%s).\n", dn);*/
    r = mkdir(dn, S_IRWXU);
    if (r == -1){
      anthy_log(0, "Failed to create profile directory\n");
      return ;
    }
    /*fprintf(stderr, "Anthy: Created\n");*/
    r = chmod(dn, S_IRUSR | S_IWUSR | S_IXUSR);
    if (r == -1) {
      anthy_log(0, "But failed to change permission.\n");
    }
  }
}

/* ���ɤ߹��ߤ�ɬ�פ����뤫������å�����
 * ɬ�פ�������֤��ͤ�1�ˤʤ� */
static int
check_base_record_uptodate(struct record_stat *rst)
{
  struct stat st;
  if (rst->is_anon) {
    return 1;
  }
  check_anthy_dir();
  if (stat(rst->base_fn, &st) < 0) {
    return 0;
  } else if (st.st_mtime != rst->base_timestamp) {
    return 0;
  }
  return 1;
}


/*
 * column format:
 *  COLUMN := OPERATION SECTION KEY VALUE*
 *  OPERATION := "ADD"    (�ɲäޤ���LRU����)
 *               "DEL"    (���)
 *  SECTION := (ʸ����)
 *  KEY     := TD
 *  VALUE   := TD
 *  TD      := TYPE DATA  (����򤢤����˽�)
 *  TYPE    := "S"        (xstr)
 *             "N"        (number)
 *  DATA    := (�����Ȥ˥��ꥢ�饤���������)
 */

static char*
read_1_token (FILE* fp, int* eol)
{
  int c;
  char* s;
  int in_quote;
  int len;

  in_quote = 0;
  s = NULL;
  len = 0;
  while (1) {
    c = fgetc(fp);
    switch (c) {
    case EOF: case '\n':
      goto out;
    case '\\':
      c = fgetc(fp);
      if (c == EOF || c == '\n') {
	goto out;
      }
      break;
    case '\"':
      in_quote = !in_quote;
      continue;
    case ' ': case '\t': case '\r':
      if (in_quote) {
	break;
      }
      if (s != NULL) {
	goto out;
      }
      break;
    default:
      break;
    }

    s = (char*) realloc(s, len + 2);
    s[len] = c;
    len ++;
  }
out:
  if (s) {
    s[len] = '\0';
  }
  *eol = (c == '\n');
  return s;
}

/* journal����ADD�ιԤ��ɤ� */
static void
read_add_column(FILE *fp, struct record_stat* rst,
		struct record_section* rsc)
{
  int n;
  xstr* xs;
  char *token;
  int eol;
  struct trie_node* node;

  token = read_1_token(fp, &eol);
  if (!token) {
    return ;
  }

  xs = anthy_cstr_to_xstr(token + 1, 0); /* xstr ����ɽ�� S ���ɤ߼ΤƤ� */
  node = do_select_column(rsc, xs, 1, LRU_USED);
  anthy_free_xstr(xs);
  free(token);

  if (node->dirty & PROTECT) {
    /* ��¸���٤� column �ʤΤǡ���ʬ�ե�������ɤ߼ΤƤ� */
    while (!eol) {
      free(read_1_token(fp, &eol));
    }
    return ;
  }

  n = 0;
  while (!eol) {
    token = read_1_token(fp, &eol);
    if (token) {
      switch(*token) {
	/* String ʸ���� */
      case 'S':
	{
	  xstr* xs;
	  xs = anthy_cstr_to_xstr(token + 1, 0);
	  do_set_nth_xstr(node, n, xs, &rst->xstrs);
	  anthy_free_xstr(xs);
	}
	break;
	/* Number ���� */
      case 'N':
	do_set_nth_value(node, n, atoi(token + 1));
	break;
      }
      free(token);
      n++;
    }
  }
  do_truncate_column(node, n);
}

/* journal����DEL�ιԤ��ɤ� */
static void
read_del_column(FILE *fp, struct record_section* rsc)
{
  struct trie_node* node;
  char* token;
  xstr* xs;
  int eol;

  token = read_1_token(fp, &eol);
  if (!token) {
    return ;
  }

  xs = anthy_cstr_to_xstr(token + 1, 0); /* xstr ����ɽ�� S ���ɤ����Ф� */
  if ((node = do_select_column(rsc, xs, 0, 0)) != NULL) {
    do_remove_column(rsc, node);
  }
  anthy_free_xstr(xs);
  free(token);
}

/** ��ʬ�ե����뤫��1���ɤ߹��� */
static void
read_1_column(struct record_stat* rst, FILE* fp, char *op)
{
  char* sec_name;
  struct record_section* rsc;
  int eol;

  sec_name = read_1_token(fp, &eol);
  if (!sec_name || eol) {
    free(sec_name);
    return ;
  }
  rsc = do_select_section(rst, sec_name, 1);
  free(sec_name);
  if (!rsc) {
    return ;
  }

  if (strcmp(op, "ADD") == 0) {
    read_add_column(fp, rst, rsc);
  } else if (strcmp(op, "DEL") == 0) {
    read_del_column(fp, rsc);
  }
}

/*
 * journal(��ʬ)�ե�������ɤ�
 */
static void
read_journal_record(struct record_stat* rs)
{
  FILE* fp;
  struct stat st;

  if (rs->is_anon) {
    return ;
  }
  if (stat(rs->journal_fn, &st) == -1) {
    return ;
  }
  fp = fopen(rs->journal_fn, "r");
  if (fp == NULL) {
    return;
  }
  if (st.st_size < rs->last_update) {
    /* �ǽ餫���ɤ߹��� */
    fseek(fp, 0, SEEK_SET);
  } else {
    fseek(fp, rs->last_update, SEEK_SET);
  }
  rs->journal_timestamp = st.st_mtime;
  while (!feof(fp)) {
    char *op;
    int eol;
    op = read_1_token(fp, &eol);
    if (op && !eol) {
      read_1_column(rs, fp, op);
    }
    free(op);
  }
  rs->last_update = ftell(fp);
  fclose(fp);
}

static void
write_string(FILE* fp, const char* str)
{
  fprintf(fp, "%s", str);
}

/* ���֥륯�����Ȥ⤷���ϥХå�����å���˥Хå�����å�����դ��� */
static void
write_quote_string(FILE* fp, const char* str)
{
  const char* p;

  for (p = str; *p; p++) {
    if (*p == '\"' || *p == '\\') {
      fputc('\\', fp);
    }
    fputc(*p, fp);
  }
}

static void
write_quote_xstr(FILE* fp, xstr* xs)
{
  char* buf;

  buf = (char*) alloca(xs->len * 6 + 2); /* EUC �ޤ���UTF8���� */
  anthy_sputxstr(buf, xs);
  write_quote_string(fp, buf);
}

static void
write_number(FILE* fp, int x)
{
  fprintf(fp, "%d", x);
}

/* journal��1���ɵ����� */
static void
commit_add_column(struct record_stat* rst,
		  const char* sname, struct trie_node* node)
{
  FILE* fp;
  int i;

  fp = fopen(rst->journal_fn, "a");
  if (fp == NULL) {
    return;
  }

  write_string(fp, "ADD \"");
  write_quote_string(fp, sname);
  write_string(fp, "\" S\"");
  write_quote_xstr(fp, &node->column.key);
  write_string(fp, "\"");

  for (i = 0; i < node->column.nr_vals; i++) {
    switch (node->column.vals[i].type) {
    case RT_EMPTY:
      write_string(fp, " E");
      break;
    case RT_VAL:
      write_string(fp, " N");
      write_number(fp, node->column.vals[i].u.val);
      break;
    case RT_XSTR:
      write_string(fp, " S\"");
      write_quote_xstr(fp, &node->column.vals[i].u.str);
      write_string(fp, "\"");
      break;
    case RT_XSTRP:
      write_string(fp, " S\"");
      write_quote_xstr(fp, node->column.vals[i].u.strp);
      write_string(fp, "\"");
      break;
    }
  }
  write_string(fp, "\n");
  rst->last_update = ftell(fp);
  fclose(fp);
}

/* ���Ƥ� column ��������� */
static void
clear_record(struct record_stat* rst)
{
  struct record_section *rsc;
  for (rsc = rst->section_list.next; rsc; rsc = rsc->next) {
    trie_remove_all(&rsc->cols, &rsc->lru_nr_used, &rsc->lru_nr_sused); 
  }
  rst->cur_column = NULL;
}

/* ���ܥե�������ɤ� */
static void
read_session(struct record_stat *rst)
{
  char **tokens;
  int nr;
  int in_section = 0;
  while (!anthy_read_line(&tokens, &nr)) {
    xstr *xs;
    int i;
    int dirty = 0;
    struct trie_node* node;

    if (!strcmp(tokens[0], "---") && nr > 1) {
      /* �����������ڤ��� */
      in_section = 1;
      rst->cur_section = do_select_section(rst, tokens[1], 1);
      goto end;
    }
    if (!in_section || nr < 2) {
      /* ��������󤬻ϤޤäƤʤ� or �Ԥ��Դ��� */
      goto end;
    }
    /* ��Ƭ��LRU�Υޡ������ɤ� */
    if (tokens[0][0] == '-') {
      dirty = 0;
    } else if (tokens[0][0] == '+') {
      dirty = LRU_SUSED;
    }
    /* ����index */
    xs = anthy_cstr_to_xstr(&tokens[0][1], 0);
    node = do_select_column(rst->cur_section, xs, 1, dirty);
    anthy_free_xstr(xs);
    if (!node) {
      goto end;
    }
    rst->cur_column = node;
    /**/
    for (i = 1; i < nr; i++) {
      if (tokens[i][0] == '"') {
	char *str;
	str = strdup(&tokens[i][1]);
	str[strlen(str) - 1] = 0;
	xs = anthy_cstr_to_xstr(str, 0);
	free(str);
	do_set_nth_xstr(rst->cur_column, i-1, xs, &rst->xstrs);
	anthy_free_xstr(xs);
      }else if (tokens[i][0] == '*') {
	/* EMPTY entry */
	get_nth_val_ent(rst->cur_column, i-1, 1);
      } else {
	do_set_nth_value(rst->cur_column, i-1, atoi(tokens[i]));
      }
    }
  end:
    anthy_free_line();
  }
}

/* ���ޤΥǡ����١��������������˥ե����뤫���ɤ߹��� */
static void
read_base_record(struct record_stat *rst)
{
  struct stat st;
  if (rst->is_anon) {
    clear_record(rst);
    return ;
  }
  check_anthy_dir();

  if (anthy_open_file(rst->base_fn) == -1) {
    return ;
  }

  clear_record(rst);
  read_session(rst);
  anthy_close_file();
  if (stat(rst->base_fn, &st) == 0) {
    rst->base_timestamp = st.st_mtime;
  }
  rst->last_update = 0;
}

static FILE *
open_tmp_in_recorddir(void)
{
  char *pn;
  const char *hd;
  const char *sid;
  sid = anthy_conf_get_str("SESSION-ID");
  hd = anthy_conf_get_str("HOME");
  pn = alloca(strlen(hd)+strlen(sid) + 10);
  sprintf(pn, "%s/.anthy/%s", hd, sid);
  return fopen(pn, "w");
}

/*
 * ����ե����뤫��base�ե������rename���� 
 */
static void
update_file(const char *fn)
{
  const char *hd;
  char *tmp_fn;
  const char *sid;
  hd = anthy_conf_get_str("HOME");
  sid = anthy_conf_get_str("SESSION-ID");
  tmp_fn = alloca(strlen(hd)+strlen(sid) + 10);

  sprintf(tmp_fn, "%s/.anthy/%s", hd, sid);
  if (rename(tmp_fn, fn)){
    anthy_log(0, "Failed to update record file %s -> %s.\n", tmp_fn, fn);
  }
}

/* ��������¸���� */
static void
save_a_column(FILE *fp, struct record_column *c, int dirty)
{
  int i;
  char *buf = alloca(c->key.len * 6 + 2);
  /* LRU�Υޡ�������� */
  if (dirty == 0) {
    fputc('-', fp);
  } else {
    fputc('+', fp);
  }
  anthy_sputxstr(buf, &c->key);
  /* index ����� */
  fprintf(fp, "%s ", buf);
  /**/
  for (i = 0; i < c->nr_vals; i++) {
    struct record_val *val = &c->vals[i];
    switch (val->type) {
    case RT_EMPTY:
      fprintf(fp, "* ");
      break;
    case RT_XSTR:
      /* should not happen */
      fprintf(fp, "\"");
      write_quote_xstr(fp, &val->u.str);
      fprintf(fp, "\" ");
      abort();
      break;
    case RT_XSTRP:
      fprintf(fp, "\"");
      write_quote_xstr(fp, val->u.strp);
      fprintf(fp, "\" ");
      break;
    case RT_VAL:
      fprintf(fp, "%d ", val->u.val);
      break;
    default:
      anthy_log(0, "Faild to save an unkonwn record. (in record.c)\n");
      break;
    }
  }
  fprintf(fp, "\n");
}

static void
update_base_record(struct record_stat* rst)
{
  struct record_section *sec;
  struct trie_node *col;
  FILE *fp;
  struct stat st;

  /* ����ե�������ä�record��񤭽Ф� */
  check_anthy_dir();
  fp = open_tmp_in_recorddir();
  if (!fp) {
    anthy_log(0, "Failed to open temporaly session file.\n");
    return ;
  }
  /* �ƥ����������Ф��� */
  for (sec = rst->section_list.next;
       sec; sec = sec->next) {
    if (!trie_first(&sec->cols)) {
      /*���Υ��������϶�*/
      continue;
    }
    /* ��������󶭳���ʸ���� */
    fprintf(fp, "--- %s\n", sec->name);
    /* �ƥ�������¸���� */
    for (col = trie_first(&sec->cols); col; 
	 col = trie_next(&sec->cols, col)) {
      save_a_column(fp, &col->column, col->dirty);
    }
  }
  fclose(fp);

  /* �����̾����rename���� */
  update_file(rst->base_fn);

  if (stat(rst->base_fn, &st) == 0) {
    rst->base_timestamp = st.st_mtime;
  }
  /* journal�ե������ä� */
  unlink(rst->journal_fn);
  rst->last_update = 0;
}

static void
commit_del_column(struct record_stat* rst,
		  const char* sname, struct trie_node* node)
{
  FILE* fp;

  fp = fopen(rst->journal_fn, "a");
  if (fp == NULL) {
    return;
  }
  write_string(fp, "DEL \"");
  write_quote_string(fp, sname);
  write_string(fp, "\" S\"");
  write_quote_xstr(fp, &node->column.key);
  write_string(fp, "\"");
  write_string(fp, "\n");
  fclose(fp);
}

/*
 * sync_add: ADD �ν񤭹���
 * sync_del_and_del: DEL �ν񤭹��ߤȺ��
 *   �ɤ����񤭹��ߤ����ˡ�¾�Υץ����ˤ�äƥǥ����������¸���줿
 *   ������������ɤ߹��ࡣ
 *   ���ΤȤ����ǡ����١�����ե�å��夹���ǽ���⤢�롣�ǡ����١�����
 *   �ե�å��夬����ȡ� cur_column �����Ƥ� xstr ��̵���ˤʤ롣
 *   �������� cur_section ��ͭ��������¸����롣
 */
static void
sync_add(struct record_stat* rst, struct record_section* rsc, 
	 struct trie_node* node)
{
  lock_record(rst);
  if (check_base_record_uptodate(rst)) {
    node->dirty |= PROTECT;
    /* ��ʬ�ե���������ɤ� */
    read_journal_record(rst);
    node->dirty &= ~PROTECT;
    commit_add_column(rst, rsc->name, node);
  } else {
    /* ���ɤ߹��� */
    commit_add_column(rst, rsc->name, node);
    read_base_record(rst);
    read_journal_record(rst);
  }
  if (rst->last_update > FILE2_LIMIT) {
    update_base_record(rst);
  }
  unlock_record(rst);
}

static void
sync_del_and_del(struct record_stat* rst, struct record_section* rsc, 
		 struct trie_node* node)
{
  lock_record(rst);
  commit_del_column(rst, rsc->name, node);
  if (!check_base_record_uptodate(rst)) {
    read_base_record(rst);
  }
  read_journal_record(rst);
  if (rst->last_update > FILE2_LIMIT) {
    update_base_record(rst);
  }
  unlock_record(rst);
}

/* Wrappers begin.. */
int 
anthy_select_section(const char *name, int flag)
{
  struct record_stat* rst;
  struct record_section* rsc;

  rst = anthy_current_record;
  if (rst->column_dirty && rst->cur_section && rst->cur_column) {
    sync_add(rst, rst->cur_section, rst->cur_column);
  }
  rst->cur_column = NULL;
  rst->column_dirty = 0;
  rsc = do_select_section(rst, name, flag);
  if (!rsc) {
    return -1;
  }
  rst->cur_section = rsc;
  return 0;
}

int
anthy_select_column(xstr *name, int flag)
{
  struct record_stat* rst;
  struct trie_node* node;

  rst = anthy_current_record;
  if (!rst->cur_section)
    return -1;
  
  if (rst->column_dirty && rst->cur_column) {
    sync_add(rst, rst->cur_section, rst->cur_column);
    rst->column_dirty = 0;
  }
  node = do_select_column(rst->cur_section, name, flag, LRU_USED);
  if (!node) {
    return -1;
  }
  rst->cur_column = node;
  rst->column_dirty = flag;
  return 0;
}

int
anthy_select_longest_column(xstr *name)
{
  struct record_stat* rst;
  struct trie_node* node;

  rst = anthy_current_record;
  if (!rst->cur_section)
    return -1;

  if (rst->column_dirty && rst->cur_column) {
    sync_add(rst, rst->cur_section, rst->cur_column);
    rst->column_dirty = 0;
  }
  node = do_select_longest_column(rst->cur_section, name);
  if (!node) {
    return -1;
  }

  rst->cur_column = node;
  rst->column_dirty = 0;
  return 0;
}

void
anthy_truncate_section(int count)
{
  do_truncate_section(anthy_current_record, count);
}

void
anthy_truncate_column(int nth)
{
  struct trie_node *cur_column = anthy_current_record->cur_column;  
  if (!cur_column) {
    return ;
  }
  do_truncate_column(cur_column, nth);

}

int
anthy_mark_column_used(void)
{
  struct record_stat* rst = anthy_current_record;
  if (!rst->cur_column) {
    return -1;
  }

  do_mark_column_used(rst->cur_section, rst->cur_column);
  sync_add(rst, rst->cur_section, rst->cur_column);
  rst->column_dirty = 0;
  return 0;
}

void
anthy_set_nth_value(int nth, int val)
{
  struct record_stat* rst;

  rst = anthy_current_record;
  if (!rst->cur_column) {
    return;
  }
  do_set_nth_value(rst->cur_column, nth, val);
  rst->column_dirty = 1;
}

void
anthy_set_nth_xstr(int nth, xstr *xs)
{
  struct record_stat* rst = anthy_current_record;
  if (!rst->cur_column) {
    return;
  }
  do_set_nth_xstr(rst->cur_column, nth, xs, &rst->xstrs);
  rst->column_dirty = 1;
}

int
anthy_get_nr_values(void)
{
  return do_get_nr_values(anthy_current_record->cur_column);
}

int
anthy_get_nth_value(int n)
{
  return do_get_nth_value(anthy_current_record->cur_column, n);
}

xstr *
anthy_get_nth_xstr(int n)
{
  return do_get_nth_xstr(anthy_current_record->cur_column, n);
}

int
anthy_select_first_column(void)
{
  struct record_stat* rst;
  struct trie_node* node;

  rst = anthy_current_record;
  if (!rst->cur_section)
    return -1;
  
  if (rst->column_dirty && rst->cur_column) {
    sync_add(rst, rst->cur_section, rst->cur_column);
    rst->column_dirty = 0;
  }
  node = do_select_first_column(rst->cur_section);
  if (!node) {
    return -1;
  }
  rst->cur_column = node;
  rst->column_dirty = 0;
  return 0;
}

int
anthy_select_next_column(void)
{
  struct record_stat* rst;
  struct trie_node* node;

  rst = anthy_current_record;
  if (!rst->cur_section || !rst->cur_column)
    return -1;
  
  /* sync_add() �� cur_column ��̵���ˤʤ뤳�Ȥ�����Τǡ�
   * ���Ȥ� column_dirty �Ǥ� sync_add() ���ʤ�
   */
  rst->column_dirty = 0;
  node = do_select_next_column(rst->cur_section, rst->cur_column);
  if (!node)
    return -1;
  rst->cur_column = node;
  rst->column_dirty = 0;
  return 0;
}

xstr *
anthy_get_index_xstr(void)
{
  return do_get_index_xstr(anthy_current_record);
}
/*..Wrappers end*/

/*
 * trie_column_init �ϲ�����Ǥ⤤��
 */
static void
trie_column_init(struct record_column* rc)
{
  rc->nr_vals = 0;
  rc->vals = NULL;
}

static void
trie_column_free(struct record_column *rc)
{
  int i;
  for (i = 0; i < rc->nr_vals; i++)
    free_val_contents(rc->vals + i);
  free(rc->vals);
  free(rc->key.str);
}  

/* ���륻�������Υǡ��������Ʋ������� */
static void
free_section(struct record_stat *r, struct record_section *rs)
{
  struct record_section *s;
  trie_remove_all(&rs->cols, &rs->lru_nr_used, &rs->lru_nr_sused);
  if (r->cur_section == rs) {
    r->cur_column = 0;
    r->cur_section = 0;
  }
  for (s = &r->section_list; s && s->next; s = s->next) {
    if (s->next == rs) {
      s->next = s->next->next;
    }
  }
  if (rs->name){
    free((void *)rs->name);
  }
  free(rs);
}

/* ���٤ƤΥǡ������������ */
static void
free_record(struct record_stat *rst)
{
  struct record_section *rsc;
  for (rsc = rst->section_list.next; rsc; ){
    struct record_section *tmp;
    tmp = rsc;
    rsc = rsc->next;
    free_section(rst, tmp);
  }
  rst->section_list.next = NULL;
}

void
anthy_release_section(void)
{
  struct record_stat* rst;

  rst = anthy_current_record;
  if (!rst->cur_section) {
    return ;
  }
  free_section(rst, rst->cur_section);
  rst->cur_section = 0;
}

void
anthy_release_column(void)
{
  struct record_stat* rst;

  rst = anthy_current_record;
  if (!rst->cur_section || !rst->cur_column) {
    return;
  }

  rst->column_dirty = 0;
  /* sync_del_and_del �Ǻ���⤹�� */
  sync_del_and_del(rst, rst->cur_section, rst->cur_column);
  rst->cur_column = NULL;
}

static void
record_dtor(void *p)
{
  int dummy;
  struct record_stat *rst = (struct record_stat*) p;
  free_record(rst);
  if (rst->id) {
    free(rst->base_fn);
    free(rst->journal_fn);
    free(rst->lock_fn);
  }
  trie_remove_all(&rst->xstrs, &dummy, &dummy);
}

void
anthy_reload_record(void)
{
  struct stat st;
  struct record_stat *rst = anthy_current_record;
  
  if (stat(rst->journal_fn, &st) == 0 &&
      rst->journal_timestamp == st.st_mtime) {
    return ;
  }

  lock_record(rst);
  read_base_record(rst);
  read_journal_record(rst);
  unlock_record(rst);
}

void
anthy_init_record(void)
{
  record_ator = anthy_create_allocator(sizeof(struct record_stat),
				       record_dtor);
}

static void
setup_filenames(const char *id, struct record_stat *rst)
{
  const char *home = anthy_conf_get_str("HOME");
  int base_len = strlen(home) + strlen(id) + 10;

  /* ���ܥե����� */
  rst->base_fn = (char*) malloc(base_len +
				strlen("/.anthy/last-record1_"));
  sprintf(rst->base_fn, "%s/.anthy/last-record1_%s%s",
	  home, id, ENCODING_SUFFIX);
  /* ��ʬ�ե����� */
  rst->journal_fn = (char*) malloc(base_len +
				   strlen("/.anthy/last-record2_"));
  sprintf(rst->journal_fn, "%s/.anthy/last-record2_%s%s",
	  home, id, ENCODING_SUFFIX);
  /* ��å��ե����� */
  rst->lock_fn = (char*) malloc(base_len +
				strlen("/.anthy/lock-record_"));
  sprintf(rst->lock_fn, "%s/.anthy/lock-record_%s%s",
	  home, id, ENCODING_SUFFIX);
}

struct record_stat *
anthy_create_record(const char *id)
{
  struct record_stat *rst;

  if (!id) {
    return NULL;
  }

  rst = anthy_smalloc(record_ator);
  rst->id = id;
  rst->section_list.next = 0;
  init_trie_root(&rst->xstrs);
  rst->cur_section = 0;
  rst->cur_column = 0;
  rst->column_dirty = 0;
  rst->initial_read = 0;

  /* �ե�����̾��ʸ������� */
  setup_filenames(id, rst);

  rst->lock_fd = -1;
  rst->last_update = 0;

  if (!strcmp(id, ANON_ID)) {
    rst->is_anon = 1;
  } else {
    rst->is_anon = 0;
    check_anthy_dir();
  }

  /* �ե����뤫���ɤ߹��� */
  lock_record(rst);
  read_base_record(rst);
  read_journal_record(rst);
  unlock_record(rst);
  rst->initial_read = 1;

  return rst;
}

void
anthy_release_record(struct record_stat *rs)
{
  anthy_sfree(record_ator, rs);
}
