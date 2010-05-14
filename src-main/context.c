/*
 * 変換や文節の伸縮などの操作が進行中の文字列や候補などを
 * まとめて変換コンテキストと呼ぶ。
 * Anthyのコンテキストに対する操作は全てここから呼ばれる。
 * 各操作に対して変換パイプラインの必要なモジュールを順に呼びだす。
 *
 * personalityの管理もする。
 *
 * Funded by IPA未踏ソフトウェア創造事業 2001 10/29
 * Copyright (C) 2000-2003 TABATA Yusuke
 *
 * $Id: context.c,v 1.26 2002/11/17 14:45:47 yusuke Exp $
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alloc.h>
#include <record.h>
#include <ordering.h>
#include <splitter.h>
#include <xstr.h>
#include "main.h"

static allocator context_ator;

/** 現在のpersonality 
 * 未設定時: null
 * 未設定のまま変換を開始した場合: "default"
 * anonymousの場合: ""
 */
static char *current_personality;

static void
context_dtor(void *p)
{
  anthy_do_reset_context((struct anthy_context *)p);
}

/** 現在のpersonalityを返す */
static char *
get_personality(void)
{
  if (!current_personality) {
    current_personality = strdup("default");
    anthy_dic_set_personality(current_personality);
  }
  return current_personality;
}

static void
release_segment(struct seg_ent *s)
{
  if (s->cands) {
    int i;
    for (i = 0; i < s->nr_cands; i++) {
      anthy_release_cand_ent(s->cands[i]);
    }
    free (s->cands);
  }
  if (s->mw_array) {
    free(s->mw_array);
  }
  free(s);
  
}

/** 文節リストの最後の要素を削除する */
static void
pop_back_seg_ent(struct anthy_context *c)
{
  struct seg_ent *s;
  s = c->seg_list.list_head.prev;
  if (s == &c->seg_list.list_head) {
    return ;
  }
  s->prev->next = s->next;
  s->next->prev = s->prev;
  release_segment(s);
  c->seg_list.nr_segments --;
}


/** n番目の文節の文字のindexを求める */
static int
get_nth_segment_index(struct anthy_context *c, int n)
{
  int i,s;
  for (i = 0, s = 0; i < c->str.len; i++) {
    if (c->split_info.ce[i].seg_border) {
      if (s == n) {
	return i;
      }
      s++;
    }
  }
  return -1;
}

/** n番目の文節の長さを求める．
 * segment_listが構成されていなくても計算できるようにする．
 */
static int
get_nth_segment_len(struct anthy_context *c, int sindex)
{
  int a,i,l;
  a = get_nth_segment_index(c, sindex);
  if ( a == -1){
    return -1;
  }
  l = 1;
  for (i = a+1; !c->split_info.ce[i].seg_border; i++) {
    l++;
  }
  return l;
}

/** metawordの配列を作る */
static void
make_metaword_array(struct anthy_context *ac,
		    struct seg_ent *se)
{
  int i;
  se->mw_array = NULL;
  for (i = se->len; i > 0; i--) {
    int j;
    /* 最後に濁点とかがついてたら直前の文字ごと落す */
    if (i < se->len &&
	anthy_get_xchar_type(se->str.str[i]) & XCT_PART) {
      /* FIXME 濁点とかがありえない並びをしてたら */
      i--;
      continue ;
    }

    se->nr_metaword = anthy_get_nr_metaword(&ac->split_info, se->from, i);
    if (!se->nr_metaword) {
      continue ;
    }
    /* metawordを配列に取り込む */
    se->mw_array = malloc(sizeof(struct meta_word*) * se->nr_metaword);
    for (j = 0; j < se->nr_metaword; j++) {
      se->mw_array[j] = anthy_get_nth_metaword(&ac->split_info, se->from, i, j);
    }
    return;
  }
}

static struct seg_ent*
create_segment(struct anthy_context *ac, int from, int len,
	       struct meta_word* best_mw)
{
  struct seg_ent* s;
  s = (struct seg_ent *)malloc(sizeof(struct seg_ent));
  s->str.str = &ac->str.str[from];
  s->str.len = len;
  s->from = from;
  s->len = s->str.len;
  s->nr_cands = 0;
  s->cands = NULL;
  s->best_seg_class = ac->split_info.ce[from].best_seg_class;
  s->best_mw = best_mw;
  make_metaword_array(ac, s);
  return s;
}

/** 変換コンテキストに文節を追加する */
static void
push_back_segment(struct anthy_context *ac, struct seg_ent *se)
{
  se->next = &ac->seg_list.list_head;
  se->prev = ac->seg_list.list_head.prev;
  ac->seg_list.list_head.prev->next = se;
  ac->seg_list.list_head.prev = se;
  ac->seg_list.nr_segments ++;
  se->committed = -1;
}

/** splitterによって配列中に付けられた文節境界のマークから、
 * 文節のリストを構成する
 */
static void
create_segment_list(struct anthy_context *ac, int from, int to)
{
  int i, n;
  struct seg_ent *s;
  /* from の所までにいくつの文節があるか調べる */
  i = 0; n = 0;
  while (i < from) {
    i += get_nth_segment_len(ac, n);
    n++;
  };
  /**/
  for (i = from; i < to; i++) {
    if (ac->split_info.ce[i].seg_border) {
      int len = get_nth_segment_len(ac, n);
      s = create_segment(ac, i, len, ac->split_info.ce[i].best_mw);

      push_back_segment(ac, s);
      n++;
    }
  }
}

/** コンテキストを作る */
struct anthy_context *
anthy_do_create_context(int encoding)
{
  struct anthy_context *ac;
  char *p = get_personality();

  if (!p) {
    return NULL;
  }

  ac = (struct anthy_context *)anthy_smalloc(context_ator);
  ac->str.str = NULL;
  ac->str.len = 0;
  ac->seg_list.nr_segments = 0;
  ac->seg_list.list_head.prev = &ac->seg_list.list_head;
  ac->seg_list.list_head.next = &ac->seg_list.list_head;
  ac->split_info.word_split_info = NULL;
  ac->split_info.ce = NULL;
  ac->ordering_info.oc = NULL;
  ac->dic_session = NULL;
  ac->prediction.str.str = NULL;
  ac->prediction.str.len = 0;
  ac->prediction.nr_prediction = 0;
  ac->prediction.predictions = NULL;
  ac->encoding = encoding;

  return ac;
}

/** コンテキストのアロケータを作る */
void
anthy_init_contexts(void)
{
  context_ator = anthy_create_allocator(sizeof(struct anthy_context),
					context_dtor);
}

void
anthy_quit_contexts(void)
{
  anthy_free_allocator(context_ator);
}

/* resetではcontextのために確保されたリソースを全て解放する */
void
anthy_do_reset_context(struct anthy_context *ac)
{
  int i, sc;
  struct prediction_cache* prediction = &ac->prediction;


  /* まず辞書セッションを解放 */
  if (ac->dic_session) {
    anthy_dic_release_session(ac->dic_session);
    ac->dic_session = NULL;
  }
  if (!ac->str.str) {
    /* 文字列が設定されていなければ解放すべき物はもう無い */
    return ;
  }
  free(ac->str.str);
  ac->str.str = NULL;
  anthy_release_split_context(&ac->split_info);
  anthy_release_ordering_context(&ac->seg_list,
				 &ac->ordering_info);

  sc = ac->seg_list.nr_segments;
  for (i = 0; i < sc; i++) {
    pop_back_seg_ent(ac);
  }
  ac->seg_list.nr_segments = 0;

  /* 予測された文字列の解放 */
  if (prediction->str.str) {
    free(prediction->str.str);
    prediction->str.str = NULL;
  }
  if (prediction->predictions) {
    for (i = 0; i < prediction->nr_prediction; ++i) {
      anthy_free_xstr(prediction->predictions[i].str);
    }
    free(prediction->predictions);
    prediction->predictions = NULL;
  }

}

void
anthy_do_release_context(struct anthy_context *ac)
{
  anthy_sfree(context_ator, ac);
}

static void
make_candidates(struct anthy_context *ac, int from, int from2, int is_reverse)
{
  int i;
  int len = ac->str.len;

  /* 文節の境界を設定 */
  /* from と from2の間に境界を作ることを禁止する */
  anthy_mark_border(&ac->split_info, from, from2, len);
  create_segment_list(ac, from, len);
  anthy_sort_metaword(&ac->seg_list);

  /* 候補の並びかえの準備をする */
  anthy_release_ordering_context(&ac->seg_list,
				 &ac->ordering_info);
  anthy_init_ordering_context(&ac->seg_list,
			      &ac->ordering_info);
  /* 候補を列挙 */
  for (i = 0; i < ac->seg_list.nr_segments; i++) {
    anthy_do_make_candidates(anthy_get_nth_segment(&ac->seg_list, i), is_reverse);
  }
  /* 候補をソート */
  anthy_sort_candidate(&ac->seg_list, 0);
}

int
anthy_do_context_set_str(struct anthy_context *ac, xstr *s, int is_reverse)
{
  int i;
  /*初期化*/
  anthy_do_reset_context(ac);

  /* 辞書セッションの開始 */
  if (!ac->dic_session) {
    ac->dic_session = anthy_dic_create_session();
    if (!ac->dic_session) {
      return -1;
    }
  }

  /* 文字列をコピー(一文字分余計にして0をセット) */
  ac->str.str = (xchar *)malloc(sizeof(xchar)*(s->len+1));
  anthy_xstrcpy(&ac->str, s);
  ac->str.str[s->len] = 0;

  /* splitterの初期化*/
  anthy_init_split_context(&ac->str, &ac->split_info, is_reverse);

  /* 解の候補を作成 */
  make_candidates(ac, 0, 0, is_reverse);
  
  /* 最初に設定した文節境界を覚えておく */
  for (i = 0; i < ac->seg_list.nr_segments; i++) {
    struct seg_ent *s = anthy_get_nth_segment(&ac->seg_list, i);
    ac->split_info.ce[s->from].initial_seg_len = s->len;
  }

  return 0;
}

void
anthy_do_resize_segment(struct anthy_context *ac,
			int nth, int resize)
{
  int i;
  int index, len, sc;

  /* resizeが可能か検査する */
  if (nth >= ac->seg_list.nr_segments) {
    return ;
  }
  index = get_nth_segment_index(ac, nth);
  len = get_nth_segment_len(ac, nth);
  if (index + len + resize > ac->str.len) {
    return ;
  }
  if (len + resize < 1) {
    return ;
  }

  /* nth以降のseg_entを解放する */
  sc = ac->seg_list.nr_segments;
  for (i = nth; i < sc; i++) {
    pop_back_seg_ent(ac);
  }

  /* resizeしたseg_borderをマークする */
  /* 現在のマークを消して新しいマークをつける */
  ac->split_info.ce[index+len].seg_border = 0;
  ac->split_info.ce[ac->str.len].seg_border = 1;
  for (i = index+len+resize+1; i < ac->str.len; i++) {
    ac->split_info.ce[i].seg_border = 0;
  }
  ac->split_info.ce[index+len+resize].seg_border = 1;
  for (i = index; i < ac->str.len; i++) {
    ac->split_info.ce[i].best_mw = NULL;
  }

  /* 解の候補を作成 */
  make_candidates(ac, index, index+len+resize, 0);
}

/*
 * n番めの文節を取得する、無い場合にはNULLを返す
 */
struct seg_ent *
anthy_get_nth_segment(struct segment_list *sl, int n)
{
  int i;
  struct seg_ent *se;
  if (n >= sl->nr_segments ||
      n < 0) {
    return NULL;
  }
  for (i = 0, se = sl->list_head.next; i < n; i++, se = se->next);
  return se;
}

int
anthy_do_set_prediction_str(struct anthy_context *ac, xstr* xs)
{
  struct prediction_cache* prediction = &ac->prediction;
  int nr_prediction;
  int i;

  /* まず辞書セッションを解放 */
  if (ac->dic_session) {
    anthy_dic_release_session(ac->dic_session);
    ac->dic_session = NULL;
  }
  /* 予測された文字列の解放 */
  if (prediction->str.str) {
    free(prediction->str.str);
    prediction->str.str = NULL;
  }
  if (prediction->predictions) {
    for (i = 0; i < prediction->nr_prediction; ++i) {
      anthy_free_xstr(prediction->predictions[i].str);
    }
    free(prediction->predictions);
    prediction->predictions = NULL;
  }
  /* 辞書セッションの開始 */
  if (!ac->dic_session) {
    ac->dic_session = anthy_dic_create_session();
    if (!ac->dic_session) {
      return -1;
    }
  }

  prediction->str.str = (xchar*)malloc(sizeof(xchar*)*(xs->len+1));
  anthy_xstrcpy(&prediction->str, xs);
  prediction->str.str[xs->len]=0;

  nr_prediction = anthy_traverse_record_for_prediction(xs, NULL);
  prediction->nr_prediction = nr_prediction;

  if (nr_prediction) {
    prediction->predictions = (struct prediction_t*)malloc(sizeof(struct prediction_t) *
							   nr_prediction);
    anthy_traverse_record_for_prediction(xs, prediction->predictions);
  }
  return 0;
}

/** 候補を表示する */
void
anthy_print_candidate(struct cand_ent *ce)
{
  int mod = (ce->score % 1000);
  int seg_score = 0;
  int dep_score = 0;

  if (ce->mw) {
    seg_score = ce->mw->score;
    dep_score = ce->mw->dep_score;
  }
  anthy_putxstr(&ce->str);
  printf(":(");
  /*if (ce->nr_words == 1) {printf("%d,", ce->elm[0].id);    }*/
  if (ce->flag & CEF_OCHAIRE) {
    putchar('o');
  }
  if (ce->flag & CEF_SINGLEWORD) {
    putchar('1');
  }
  if (ce->flag & CEF_GUESS) {
    putchar('g');
  }
  if (ce->flag & (CEF_KATAKANA | CEF_HIRAGANA)) {
    putchar('N');
  }
  if (ce->flag & CEF_USEDICT) {
    putchar('U');
  }
  printf(",%d,", seg_score);

    
  if (ce->mw) {
    switch (ce->mw->seg_class) {
    case SEG_HEAD:
      printf("H");
      break;
    case SEG_TAIL:
      printf("T");
      break;
    case SEG_BUNSETSU:
      printf("B");
      break;
    case SEG_SHUGO:
      printf("S");
      break;
    case SEG_JYUTSUGO:
      printf("J");
      break;
    case SEG_SHUSHOKUGO:
      printf("M");
      break;
    case SEG_SETSUZOKUGO:
      printf("C");
      break;
    case SEG_DOKURITSUGO:
      printf("I");
      break;
    case SEG_FUZOKUGO:
      printf("F");
      break;
    case SEG_HIRAKIKAKKO:
      printf("(");
      break;
    case SEG_TOJIKAKKO:
      printf(")");
      break;
    case SEG_MEISHI_KAKUJOSHI:
      printf("Nk");
      break;
    case SEG_MEISHI_SHUTAN:
      printf("Ne");
      break;
    case SEG_DOUSHI:
      printf("V");
      break;
    case SEG_DOUSHI_FUZOKUGO:
      printf("Vf");
      break;
    case SEG_DOUSHI_SHUTAN:
      printf("Ve");
      break;
    case SEG_KEIYOUSHI:
      printf("A");
      break;
    case SEG_KEIYOUSHI_FUZOKUGO:
      printf("Af");
      break;
    case SEG_KEIYOUSHI_SHUTAN:
      printf("Ae");
      break;
    case SEG_KEIYOUDOUSHI:
      printf("AJV");
      break;
    case SEG_KEIYOUDOUSHI_FUZOKUGO:
      printf("AJVf");
      break;
    case SEG_KEIYOUDOUSHI_SHUTAN:
      printf("AJVe");
      break;
    case SEG_RENYOU_SHUSHOKU:
      printf("YM");
      break;
    case SEG_RENTAI_SHUSHOKU:
      printf("TM");
      break;
    case SEG_MEISHI:
      printf("N");
      break;
    case SEG_MEISHI_FUZOKUGO:
      printf("Nf");
      break;
    case SEG_MEISHI_RENYOU:
      printf("Ny");
      break;
    case SEG_DOUSHI_RENYOU:
      printf("Vy");
      break;
    case SEG_KEIYOUSHI_RENYOU:
      printf("Ay");
      break;
    case SEG_KEIYOUDOUSHI_RENYOU:
      printf("AJVy");
      break;
    case SEG_FUKUSHI:
      printf("AV");
      break;
    case SEG_DOUSHI_RENTAI:
      printf("Vt");
      break;
    case SEG_KEIYOUSHI_RENTAI:
      printf("At");
      break;
    case SEG_KEIYOUDOUSHI_RENTAI:
      printf("AJVt");
      break;
    case SEG_RENTAISHI:
      printf("ME");
      break;
    case SEG_KAKUJOSHI:
      printf("Fk");
      break;
    case SEG_RENYOU:
      printf("Fy");
      break;
    case SEG_RENTAI:
      printf("Ft");
      break;
    case SEG_SHUTAN:
      printf("Fe");
      break;
    default:
      printf("?");
      break;
    }
  } else {
    putchar('-');
  }
  printf(",%d", dep_score);
  printf(")");

  if (ce->score >= 1000) {
    printf("%d,", ce->score/1000);
    if (mod < 100) {
      printf("0");
    }
    if (mod < 10) {
      printf("0");
    }
    printf("%d ", mod);
  } else {
    printf("%d ", ce->score);
  }
}

/** 文節を表示する */
static void
print_segment(struct seg_ent *e)
{
  int i;

  anthy_putxstr(&e->str);
  printf("(");
  for ( i = 0 ; i < e->nr_cands ; i++) {
    anthy_print_candidate(e->cands[i]);
    printf(",");
  }
  printf(")");
  printf(":\n");
}

/** コンテキストを表示する */
void
anthy_do_print_context(struct anthy_context *ac, int encoding)
{
  int i;
  struct char_ent *ce;
  anthy_xstr_set_print_encoding(encoding);

  ce = ac->split_info.ce;
  if (!ce) {
    printf("(invalid)\n");
    return ;
  }
  /* 各文字を表示する */
  for (i = 0, ce = ac->split_info.ce; i < ac->str.len; i++, ce++) {
    if (ce->seg_border) {
      printf("|");
    }
    anthy_putxchar(*(ce->c));
  }
  printf("\n");
  /* 各文節を表示する */
  for (i = 0; i < ac->seg_list.nr_segments; i++) {
    print_segment(anthy_get_nth_segment(&ac->seg_list, i));
  }
  printf("\n");
}

void
anthy_release_cand_ent(struct cand_ent *ce)
{
  if (ce->elm) {
    free(ce->elm);
  }
  if (&ce->str) {
    anthy_free_xstr_str(&ce->str);
  }
  free(ce);
}

int
anthy_do_set_personality(const char *id)
{
  if (current_personality) {
    /* すでに設定されてる */
    return -1;
  }
  if (!id || strchr(id, '/')) {
    return -1;
  }
  current_personality = strdup(id);
  anthy_dic_set_personality(current_personality);
  return 0;
}

void
anthy_init_personality(void)
{
  current_personality = NULL;
}

void
anthy_quit_personality(void)
{
  if (current_personality) {
    free(current_personality);
    current_personality = NULL;
  }
}
