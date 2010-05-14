/*
 * ʸ��κǾ�ñ�̤Ǥ���wordlist��������
 *
 * init_word_seq_tab()
 *   ��°��ơ��֥���ΥΡ��ɤؤΥݥ��󥿤ν����
 * release_word_seq_tab()
 *   ��°��ơ��֥�β���
 * anthy_make_word_list_all() 
 * ʸ��η�������������ʬʸ�������󤹤�
 *  �������η�ϩ����󤵤줿word_list��
 *  anthy_commit_word_list��splitter_context���ɲä����
 *
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2002 2/27
 * Copyright (C) 2000-2004 TABATA Yusuke
 * Copyright (C) 2004 YOSHIDA Yuichi
 * Copyright (C) 2000-2003 TABATA Yusuke, UGAWA Tomoharu
 *
 * $Id: wordlist.c,v 1.50 2002/11/17 14:45:47 yusuke Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alloc.h>
#include <record.h>
#include <xstr.h>
#include <wtype.h>
#include <conf.h>
#include <ruleparser.h>
#include <dic.h>
#include <splitter.h>
#include "wordborder.h"

int anthy_score_per_freq = 55;
int anthy_score_per_depword = 950;
int anthy_score_per_len = 2;

static allocator wordseq_rule_ator;

/** ��Ω����ʻ�Ȥ��θ��³����°��Υ������ΥΡ��ɤ��б� */
struct wordseq_rule {
  wtype_t wt; /* ��Ω����ʻ� */
  int ratio; /* ����Υ��������Ф�����Ψ */
  const char *wt_name; /* �ʻ�̾(�ǥХå���) */
  int node_id; /* ���μ�Ω��θ���³����°�쥰�����ΥΡ��ɤ�id */
  struct wordseq_rule *next;
};

/* ñ����³�롼�� */
static struct wordseq_rule *gRules;

/* �ǥХå��� */
void
anthy_print_word_list(struct splitter_context *sc,
		      struct word_list *wl)
{
  xstr xs;
  const char *wn = "---";
  if (!wl) {
    printf("--\n");
    return ;
  }
  /* ��Ƭ�� */
  xs.len = wl->part[PART_CORE].from - wl->from;
  xs.str = sc->ce[wl->from].c;
  anthy_putxstr(&xs);
  printf(".");
  /* ��Ω�� */
  xs.len = wl->part[PART_CORE].len;
  xs.str = sc->ce[wl->part[PART_CORE].from].c;
  anthy_putxstr(&xs);
  printf(".");
  /* ������ */
  xs.len = wl->part[PART_POSTFIX].len;
  xs.str = sc->ce[wl->part[PART_CORE].from + wl->part[PART_CORE].len].c;
  anthy_putxstr(&xs);
  printf("-");
  /* ��°�� */
  xs.len = wl->part[PART_DEPWORD].len;
  xs.str = sc->ce[wl->part[PART_CORE].from +
		  wl->part[PART_CORE].len +
		  wl->part[PART_POSTFIX].len].c;
  anthy_putxstr(&xs);
  if (wl->core_wt_name) {
    wn = wl->core_wt_name;
  }
  printf(" %s %d %d %d\n", wn, wl->score, wl->part[PART_DEPWORD].ratio, wl->seg_class);
}

/** word_list��ɾ������ */
static void
eval_word_list(struct word_list *wl)
{
  struct part_info *part = wl->part;

  /* ��Ω��Υ����������٤ˤ����� */
  wl->score += part[PART_CORE].freq * anthy_score_per_freq;
  /* ��°����Ф������ */
  if (part[PART_DEPWORD].len) {
    int score;
    int len = part[PART_DEPWORD].len - wl->weak_len;
    if (len > 5) {
      len = 5;
    }
    if (len < 0) {
      len = 0;
    }
    score = len * part[PART_DEPWORD].ratio * anthy_score_per_depword;
    score /= RATIO_BASE;
    wl->score += score;
  }

  /* �ƥѡ��Ȥˤ��Ĵ�� */
  wl->score *= part[PART_CORE].ratio;
  wl->score /= RATIO_BASE;
  wl->score *= part[PART_POSTFIX].ratio;
  wl->score /= RATIO_BASE;
  wl->score *= part[PART_PREFIX].ratio;
  wl->score /= RATIO_BASE;
  wl->score *= part[PART_DEPWORD].ratio;
  wl->score /= RATIO_BASE;

  /* Ĺ���ˤ����� */
  wl->score += (wl->len - wl->weak_len) * anthy_score_per_len;
}

/** word_list����Ӥ��롢�޴���Τ���ʤΤǡ�
    ��̩����ӤǤ���ɬ�פ�̵�� */
static int
word_list_same(struct word_list *wl1, struct word_list *wl2)
{
  if (wl1->node_id != wl2->node_id ||
      wl1->score != wl2->score ||
      wl1->from != wl2->from ||
      wl1->len != wl2->len ||
      anthy_wtype_get_pos(wl1->part[PART_CORE].wt) != anthy_wtype_get_pos(wl2->part[PART_CORE].wt) ||
      wl1->head_pos != wl2->head_pos) {
    return 0;
  }
  if (wl1->part[PART_DEPWORD].dc != wl2->part[PART_DEPWORD].dc) {
    return 0;
   }
   return 1;
 }


/** ��ä�word_list�Υ�������׻����Ƥ��饳�ߥåȤ��� */
void 
anthy_commit_word_list(struct splitter_context *sc,
		       struct word_list *wl)
{
  struct word_list *tmp;

  /* ��°�������word_list�ǡ�Ĺ��0�Τ��äƤ���Τ� */
  if (wl->len == 0) return;
  /**/
  wl->last_part = PART_DEPWORD;

  /* �����׻� */
  eval_word_list(wl);
  /* hmm�ǻ��Ѥ��륯�饹������ */
  anthy_set_seg_class(wl);

  /* Ʊ�����Ƥ�word_list���ʤ�����Ĵ�٤� */
  for (tmp = sc->word_split_info->cnode[wl->from].wl; tmp; tmp = tmp->next) {
    if (word_list_same(tmp, wl)) {
      return ;
    }
  }
  /* wordlist�Υꥹ�Ȥ��ɲ� */
  wl->next = sc->word_split_info->cnode[wl->from].wl;
  sc->word_split_info->cnode[wl->from].wl = wl;
  /* �ǥХå��ץ��� */
  if (anthy_splitter_debug_flags() & SPLITTER_DEBUG_WL) {
    anthy_print_word_list(sc, wl);
  }
}

struct word_list *
anthy_alloc_word_list(struct splitter_context *sc)
{
  struct word_list* wl;
  wl = anthy_smalloc(sc->word_split_info->WlAllocator);
  return wl;
}

/* ��³�γ��Ѹ��������졢��ư����դ��� */
static void
make_following_word_list(struct splitter_context *sc,
			 struct word_list *tmpl)
{
  /* ����xs�ϼ�Ω�����θ�³��ʸ���� */
  xstr xs;
  xs.str = sc->ce[tmpl->from+tmpl->len].c;
  xs.len = sc->char_count - tmpl->from - tmpl->len;
  tmpl->part[PART_DEPWORD].from =
    tmpl->part[PART_POSTFIX].from + tmpl->part[PART_POSTFIX].len;
  
  if (tmpl->node_id >= 0) {
    /* ���̤�word_list */
    anthy_scan_node(sc, tmpl, &xs, tmpl->node_id);
  } else {
    /* ��Ω�줬�ʤ�word_list */
    struct wordseq_rule *r;
    struct word_list new_tmpl;
    new_tmpl = *tmpl;
    /* ̾��θ��³���롼����Ф��� */
    for (r = gRules; r; r = r->next) {
      if (anthy_wtype_get_pos(r->wt) == POS_NOUN) {
	new_tmpl.part[PART_CORE].wt = r->wt;
	new_tmpl.core_wt_name = r->wt_name;
	new_tmpl.node_id = r->node_id;
	new_tmpl.head_pos = anthy_wtype_get_pos(new_tmpl.part[PART_CORE].wt);
	anthy_scan_node(sc, &new_tmpl, &xs, new_tmpl.node_id);
      }
    }
  }
}

static void
push_part_back(struct word_list *tmpl, int len,
	       seq_ent_t se, wtype_t wt)
{
  tmpl->len += len;
  tmpl->part[PART_POSTFIX].len += len;
  tmpl->part[PART_POSTFIX].wt = wt;
  tmpl->part[PART_POSTFIX].seq = se;
  tmpl->part[PART_POSTFIX].ratio = RATIO_BASE * 2 / 3;
  tmpl->last_part = PART_POSTFIX;
}

/* �������򤯤äĤ��� */
static void 
make_suc_words(struct splitter_context *sc,
	       struct word_list *tmpl)
{
  int i, right;

  wtype_t core_wt = tmpl->part[PART_CORE].wt;
  /* ���졢̾��������̾��Τ����줫����°����դ� */
  int core_is_num = 0;
  int core_is_name = 0;
  int core_is_sv_noun = 0;
  

  /* �ޤ������������դ���Ω�줫�����å����� */
  if (anthy_wtypecmp(anthy_wtype_num_noun, core_wt)) {
    core_is_num = 1;
  }
  if (anthy_wtypecmp(anthy_wtype_name_noun, core_wt)) {
    core_is_name = 1;
  }
  if (anthy_wtype_get_sv(core_wt)) {
    core_is_sv_noun = 1;
  }
  if (!core_is_num && !core_is_name && !core_is_sv_noun) {
    return ;
  }

  right = tmpl->part[PART_CORE].from + tmpl->part[PART_CORE].len;
  /* ��Ω��α�¦��ʸ������Ф��� */
  for (i = 1;
       i <= sc->word_split_info->seq_len[right];
       i++){
    xstr xs;
    seq_ent_t suc;
    xs.str = sc->ce[right].c;
    xs.len = i;
    suc = anthy_get_seq_ent_from_xstr(&xs);
    if (anthy_get_seq_ent_pos(suc, POS_SUC)) {
      /* ��¦��ʸ�������°��ʤΤǡ���Ω����ʻ�ˤ��碌�ƥ����å� */
      struct word_list new_tmpl;
      if (core_is_num &&
	  anthy_get_seq_ent_wtype_freq(suc, anthy_wtype_num_postfix)) {
	new_tmpl = *tmpl;
	push_part_back(&new_tmpl, i, suc, anthy_wtype_num_postfix);
	make_following_word_list(sc, &new_tmpl);
      }
      if (core_is_name &&
	  anthy_get_seq_ent_wtype_freq(suc, anthy_wtype_name_postfix)) {
	new_tmpl = *tmpl;
	push_part_back(&new_tmpl, i, suc, anthy_wtype_name_postfix);
	new_tmpl.weak_len += i;
	make_following_word_list(sc, &new_tmpl);
      }
      if (core_is_sv_noun &&
	  anthy_get_seq_ent_wtype_freq(suc, anthy_wtype_sv_postfix)) {
	new_tmpl = *tmpl;
	push_part_back(&new_tmpl, i, suc, anthy_wtype_sv_postfix);
	new_tmpl.weak_len += i;
	make_following_word_list(sc, &new_tmpl);
      }
    }
  }
}

static void
push_part_front(struct word_list *tmpl, int len,
		seq_ent_t se, wtype_t wt)
{
  tmpl->from = tmpl->from - len;
  tmpl->len = tmpl->len + len;
  tmpl->part[PART_PREFIX].from = tmpl->from;
  tmpl->part[PART_PREFIX].len += len;
  tmpl->part[PART_PREFIX].wt = wt;
  tmpl->part[PART_PREFIX].seq = se;
  tmpl->part[PART_PREFIX].ratio = RATIO_BASE * 2 / 3;
}

/* ��Ƭ���򤯤äĤ��Ƥ����������򤯤äĤ��� */
static void
make_pre_words(struct splitter_context *sc,
	       struct word_list *tmpl)
{
  int i;
  wtype_t core_wt = tmpl->part[PART_CORE].wt;
  /* ��Ω��Ͽ��줫�� */
  if (!anthy_wtypecmp(anthy_wtype_num_noun, core_wt)) {
    return ;
  }
  /* ��Ƭ������󤹤� */
  for (i = 1; 
       i <= sc->word_split_info->rev_seq_len[tmpl->part[PART_CORE].from];
       i++) {
    seq_ent_t pre;
    /* ����xs�ϼ�Ω����������ʸ���� */
    xstr xs;
    xs.str = sc->ce[tmpl->part[PART_CORE].from - i].c;
    xs.len = i;
    pre = anthy_get_seq_ent_from_xstr(&xs);
    if (anthy_get_seq_ent_pos(pre, POS_PRE)) {
      struct word_list new_tmpl;
      if (anthy_get_seq_ent_wtype_freq(pre, anthy_wtype_num_prefix)) {
	new_tmpl = *tmpl;
	push_part_front(&new_tmpl, i, pre, anthy_wtype_num_prefix);
	make_following_word_list(sc, &new_tmpl);
	/* ���ξ����������⤯�äĤ��� */
	make_suc_words(sc, &new_tmpl);
      }
    }
  }
}

/* wordlist���������� */
static void
setup_word_list(struct word_list *wl, int from, int len, int is_compound)
{
  int i;
  wl->from = from;
  wl->len = len;
  wl->weak_len = 0;
  wl->is_compound = is_compound;
  /* part��������������� */
  for (i = 0; i < NR_PARTS; i++) {
    wl->part[i].from = 0;
    wl->part[i].len = 0;
    wl->part[i].wt = anthy_wt_none;
    wl->part[i].seq = 0;
    wl->part[i].freq = 1;/* ���٤��㤤ñ��Ȥ��Ƥ��� */
    wl->part[i].ratio = RATIO_BASE;
    wl->part[i].dc = DEP_RAW;
  }
  /* ��Ω��Υѡ��Ȥ����� */
  wl->part[PART_CORE].from = from;
  wl->part[PART_CORE].len = len;
  /**/
  wl->score = 0;
  wl->node_id = -1;
  wl->last_part = PART_CORE;
  wl->head_pos = POS_NONE;
  wl->tail_ct = CT_NONE;
  /**/
  wl->core_wt_name = NULL;
}

/*
 * ������Ω����Ф��ơ���Ƭ��������������°����դ�����Τ�
 * ʸ��θ���(=word_list)�Ȥ���cache���ɲä���
 */
static void
make_word_list(struct splitter_context *sc,
	       seq_ent_t se,
	       int from, int len,
	       int is_compound)
{
  struct word_list tmpl;
  struct wordseq_rule *r;

  /* �ƥ�ץ졼�Ȥν���� */
  setup_word_list(&tmpl, from, len, is_compound);
  tmpl.part[PART_CORE].seq = se;

  /* �ƥ롼��˥ޥå����뤫��� */
  for (r = gRules; r; r = r->next) {
    int freq;
    if (!is_compound) {
      freq = anthy_get_seq_ent_wtype_freq(se, r->wt);
    } else {
      freq = anthy_get_seq_ent_wtype_compound_freq(se, r->wt);
    }
    if (freq) {
      /* ��Ω����ʻ�Ϥ��Υ롼��ˤ��äƤ��� */
      if (anthy_splitter_debug_flags() & SPLITTER_DEBUG_ID) {
	/* �ʻ�ɽ�ΥǥХå���*/
	xstr xs;
	xs.str = sc->ce[tmpl.part[PART_CORE].from].c;
	xs.len = tmpl.part[PART_CORE].len;
	anthy_putxstr(&xs);
	printf(" name=%s freq=%d node_id=%d\n", r->wt_name, freq, r->node_id);
      }
      /* ���ܤ����롼��ξ����ž������ */
      tmpl.part[PART_CORE].wt = r->wt;
      tmpl.part[PART_CORE].ratio = r->ratio;
      tmpl.part[PART_CORE].freq = freq;
      tmpl.core_wt_name = r->wt_name;
      tmpl.node_id = r->node_id;
      tmpl.head_pos = anthy_wtype_get_pos(tmpl.part[PART_CORE].wt);
      /**/
      tmpl.part[PART_POSTFIX].from =
	tmpl.part[PART_CORE].from +
	tmpl.part[PART_CORE].len;
      /**/
      if (anthy_wtype_get_pos(r->wt) == POS_NOUN ||
	  anthy_wtype_get_pos(r->wt) == POS_NUMBER) {
	/* ��Ƭ������������̾�졢����ˤ����դ��ʤ����Ȥˤ��Ƥ��� */
	make_pre_words(sc, &tmpl);
	make_suc_words(sc, &tmpl);
      }
      /* ��Ƭ����������̵���ǽ����ư���Ĥ��� */
      make_following_word_list(sc, &tmpl);
    }
  }
}

static void
make_dummy_head(struct splitter_context *sc)
{
  struct word_list tmpl;
  setup_word_list(&tmpl, 0, 0, 0);
  tmpl.part[PART_CORE].seq = 0;
  tmpl.part[PART_CORE].wt = anthy_wtype_noun;
  tmpl.score = 0;
  tmpl.head_pos = anthy_wtype_get_pos(tmpl.part[PART_CORE].wt);
  make_suc_words(sc, &tmpl);
}


static int
is_indep(seq_ent_t se)
{
  if (!se) {
    return 0;
  }
  return anthy_get_seq_ent_indep(se);
}

/* ����ƥ����Ȥ�ʸ����������Ƥ�word_list����󤹤� */
void 
anthy_make_word_list_all(struct splitter_context *sc)
{
  int i, j;
  xstr xs;
  seq_ent_t se;
  struct depword_ent {
    struct depword_ent *next;
    int from, len;
    int is_compound;
    seq_ent_t se;
  } *head, *de;
  struct word_split_info_cache *info;
  allocator de_ator;

  info = sc->word_split_info;
  head = 0;
  de_ator = anthy_create_allocator(sizeof(struct depword_ent), 0);

  /* ���Ƥμ�Ω������ */
  /* ���������Υ롼�� */
  for (i = 0; i < sc->char_count ; i++) {
    int search_len = sc->char_count - i;
    int search_from = 0;
    if (search_len > 30) {
      search_len = 30;
    }

    /* ʸ����Ĺ�Υ롼��(Ĺ��������) */
    for (j = search_len; j > search_from; j--) {
      xs.len = j;
      xs.str = sc->ce[i].c;

      se = anthy_get_seq_ent_from_xstr(&xs);

      if (se) {
	/* �ơ���ʬʸ����ñ��ʤ����Ƭ������������
	   ����Ĺ��Ĵ�٤ƥޡ������� */
	if (j > info->seq_len[i] &&
	    anthy_get_seq_ent_pos(se, POS_SUC)) {
	  info->seq_len[i] = j;
	}
	if (j > info->rev_seq_len[i + j] &&
	    anthy_get_seq_ent_pos(se, POS_PRE)) {
	  info->rev_seq_len[i + j] = j;
	}
      }

      /* ȯ��������Ω���ꥹ�Ȥ��ɲ� */
      if (is_indep(se)) {
	de = (struct depword_ent *)anthy_smalloc(de_ator);
	de->from = i;
	de->len = j;
	de->se = se;
	de->is_compound = 0;

	de->next = head;
	head = de;
      }
      /* ȯ������ʣ����ꥹ�Ȥ��ɲ� */
      if (anthy_get_nr_compound_ents(se) > 0) {
	de = (struct depword_ent *)anthy_smalloc(de_ator);
	de->from = i;
	de->len = j;
	de->se = se;
	de->is_compound = 1;

	de->next = head;
	head = de;
      }
    }
  }

  /* ȯ��������Ω�����Ƥ��Ф�����°��ѥ�����θ��� */
  for (de = head; de; de = de->next) {
    make_word_list(sc, de->se, de->from, de->len, de->is_compound);
  }
  

    /* ��Ω���̵��word_list */
  for (i = 0; i < sc->char_count; i++) {
    struct word_list tmpl;
    if (i == 0) {
      setup_word_list(&tmpl, i, 0, 0);
      make_following_word_list(sc, &tmpl);
    } else {
      int type = anthy_get_xchar_type(*sc->ce[i - 1].c);
      if (type & XCT_CLOSE || type & XCT_SYMBOL) {
	setup_word_list(&tmpl, i, 0, 0);
	make_following_word_list(sc, &tmpl);
      }
    }
  }  

  /* ��Ƭ��0ʸ���μ�Ω����դ��� */
  make_dummy_head(sc);

  anthy_free_allocator(de_ator);
}

static void
parse_line(char **tokens, int nr)
{
  struct wordseq_rule *r;
  int tmp;
  if (nr < 2) {
    printf("Syntex error in indepword defs"
	   " :%d.\n", anthy_get_line_number());
    return ;
  }
  /* �Ԥ���Ƭ�ˤ��ʻ��̾�������äƤ��� */
  r = anthy_smalloc(wordseq_rule_ator);
  r->wt_name = anthy_name_intern(tokens[0]);
  anthy_init_wtype_by_name(tokens[0], &r->wt);

  /* ��Ψ */
  tmp = atoi(tokens[1]);
  if (tmp == 0) {
    tmp = 1;
  }
  r->ratio = RATIO_BASE - tmp*(RATIO_BASE/64);

  /* ���μ��ˤϥΡ���̾�����äƤ��� */
  r->node_id = anthy_get_node_id_by_name(tokens[2]);

  /* �롼����ɲ� */
  r->next = gRules;
  gRules = r;
}

/** ��°�쥰��դ��ɤ߹��� */
static int 
init_word_seq_tab(void)
{
  const char *fn;
  char **tokens;
  int nr;

  wordseq_rule_ator = anthy_create_allocator(sizeof(struct wordseq_rule),
					     NULL);

  fn = anthy_conf_get_str("INDEPWORD");
  if (!fn){
    printf("independent word dict unspecified.\n");
    return -1;
  }
  if (anthy_open_file(fn) == -1) {
    printf("Failed to open indep word dict (%s).\n", fn);
    return -1;
  }
  /* �ե�������Ԥ����ɤ� */
  gRules = NULL;
  while (!anthy_read_line(&tokens, &nr)) {
    parse_line(tokens, nr);
    anthy_free_line();
  }
  anthy_close_file();

  return 0;
}


int
anthy_init_wordlist(void)
{
  return init_word_seq_tab();
}
