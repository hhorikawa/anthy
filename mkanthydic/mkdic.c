/*
 * cannadic�����Υե����뤫�鼭��ե��������
 *
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2002 1/1
 *
 * Copyright (C) 2000-2005 TABATA Yusuke
 * Copyright (C) 2001-2002 TAKAI Kousuke
 */
/*
 * ������ɤߤ�index�Ȥ����ʻ���Ѵ����ñ��(=entry)�򸡺�
 * ���빽¤�ˤʤäƤ��롣
 *
 * �ɤ� -> ñ�졢ñ�졢��
 *
 * ����ե�����ϥͥåȥ���Х��ȥ����������Ѥ��롣
 *
 * ����ե������ʣ���Υ�������󤫤鹽������Ƥ���
 *  0 �إå� 16*4 bytes
 *  2 �ɤߤΥ���ǥå��� (�ɤ�64�Ĥ���)
 *  3 �ɤ�
 *  4 �ڡ���
 *  5 �ڡ����Υ���ǥå���
 *  6 ���㼭��(?)
 *  7 �ɤ� hash
 *  8 ���� hash (not yet)
 *
 * source ���μ���ե�����
 * file_dic ��������ե�����
 *
 * yomi_hash ����ե�����˽��Ϥ����hash��bitmap
 * index_hash ���Υ��������struct yomi_entry�򸡺����뤿���hash
 *
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <config.h>

#ifdef USE_UCS4
#include <iconv.h>
#endif

#include <file_dic.h>
#include <xstr.h>
#include <wtype.h>

#include "mkdic.h"

#define MAX_LINE_LEN 1024
#define NR_HEADER_SECTIONS 16
#define SECTION_ALIGNMENT 8
#define MAX_WTYPE_LEN 20

#define DEFAULT_FN "anthy.dic"
static const char *output_fn = DEFAULT_FN;

static const char *progname;
static FILE *yomi_entry_index_out, *yomi_entry_out;
static FILE *page_out, *page_index_out;
static FILE *uc_out;
static FILE *yomi_hash_out;
static FILE *versatile_hash_out;
static int yomi_hash_collision;

/* �ե�������ν���˽��ä��¤٤� */
struct file_section {
  FILE **fpp;
} file_array[] = {
  {&yomi_entry_index_out},
  {&yomi_entry_out},
  {&page_out},
  {&page_index_out},
  {&uc_out},
  {&yomi_hash_out},
  {&versatile_hash_out},
  {NULL},
};

#ifdef USE_UCS4
static iconv_t euc_to_utf8;
#endif


/* ����ν�����Υե�����򥪡��ץ󤹤� */
static void
open_output_files(void)
{
  struct file_section *fs;
  for (fs = file_array; fs->fpp; fs ++) {
    if (!(*(fs->fpp) = tmpfile())) {
      fprintf (stderr, "%s: cannot open temporary file: %s\n",
	       progname, strerror (errno));
      exit (2);
    }
  }
}

static void
flush_output_files (void)
{
  struct file_section *fs;
  for (fs = file_array; fs->fpp; fs ++) {
    if (ferror(*(fs->fpp))) {
      fprintf (stderr, "%s: write error\n", progname);
      exit (1);
    }
  }
  for (fs = file_array; fs->fpp; fs ++) {
    if (fflush(*(fs->fpp))) {
      fprintf (stderr, "%s: write error: %s\n", progname, strerror (errno));
      exit (1);
    }
  }
}

/* 2�Ĥ�ʸ����ζ�����ʬ��Ĺ������� */
static int
common_len(xstr *s1, xstr *s2)
{
  int m,i;
  if (!s1 || !s2) {
    return 0;
  }
  if (s1->len < s2->len) {
    m = s1->len;
  }else{
    m = s2->len;
  }
  for (i = 0; i < m; i++) {
    if (s1->str[i] != s2->str[i]) {
      return i;
    }
  }
  return m;
}

/* ���ꥸ�ʥ�μ���ե�����Υ��󥳡��ǥ��󥰤���
   �����Υ��󥳡��ǥ��󥰤��Ѵ����� */
static char *
source_str_to_file_dic_str(char *str)
{
#ifdef USE_UCS4
  /* EUC����UTF8���Ѵ����� */
  char *inbuf = str;
  size_t outbytesleft = strlen(str) * 6 + 2;
  char *outbuf = alloca(outbytesleft);
  char *buf = outbuf;
  size_t inbytesleft = strlen(str);
  size_t res;
  res = iconv(euc_to_utf8, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
  if (res == (size_t) -1) {
    fprintf(stderr, "failed to iconv()\n");
    exit(1);
  }
  *outbuf = 0;
  return strdup(buf);
#else
  return strdup(str);
#endif
}

/* �ͥåȥ��byteorder��4bytes�񤭽Ф� */
void
write_nl(FILE *fp, int i)
{
  i = htonl(i);
  fwrite(&i, sizeof(int), 1, fp);
}

/*
 * 2�Ĥ�ʸ����κ�ʬ����Ϥ���
 * AAA ABBB �Ȥ���2�Ĥ�ʸ����򸫤����ˤ�
 * ABBB��AAA�Τ�����2ʸ����ä���BBB���դ�����ΤȤ���
 * \0x2BBB�Ƚ��Ϥ���롣
 */
static int
output_diff(xstr *p, xstr *c)
{
  int i, m, len = 1;
  m = common_len(p, c);
  if (p && p->len > m) {
    fprintf(page_out, "%c", p->len - m + 1);
  } else {
    fprintf(page_out, "%c", 1);
  }
  for (i = m; i < c-> len; i++) {
    char buf[8];
    len += anthy_sputxchar(buf, c->str[i], 0);
    fputs(buf, page_out);
  }
  return len;
}

static void
print_usage(void)
{
  printf("please use mkanthydic command.\n");
  exit(0);
}

static char *
read_line(FILE *fp, char *buf)
{
  /* Ĺ������Ԥ�̵�뤹�� */
  int toolong = 0;

  while (fgets(buf, MAX_LINE_LEN, fp)) {
    int len = strlen(buf);
    if (buf[0] == '#') {
      continue ;
    }
    if (buf[len - 1] != '\n') {
      toolong = 1;
      continue ;
    }

    buf[len - 1] = 0;
    if (toolong) {
      toolong = 0;
    } else {
      return buf;
    }
  }
  return NULL;
}

/** cannadic�����μ���ιԤ���index�Ȥʤ���ʬ����Ф� */
static char *
get_index_from_line(char *buf)
{
  char *sp, *res;
  sp = strchr(buf, ' ');
  if (!sp) {
    /* ����Υե����ޥåȤ��������� */
    return NULL;
  }
  *sp = 0;
  res = source_str_to_file_dic_str(buf);
  *sp = ' ';
  return res;
}

/** cannadic�����μ���ιԤ���index�ʳ�����ʬ����Ф� */
static char *
get_entry_from_line(char *buf)
{
  char *sp;
  sp = strchr(buf, ' ');
  while(*sp == ' ') {
    sp ++;
  }
  return source_str_to_file_dic_str(sp);
}

static void
begin_new_page(int i)
{
  fputc(0, page_out);
  write_nl(page_index_out, i);
}

static void
output_entry_index(int i)
{
  write_nl(yomi_entry_index_out, i);
}

static int
index_hash(xstr *xs)
{
  int i;
  unsigned int h = 0;
  for (i = 0; i < xs->len; i++) {
    h += xs->str[i];
  }
  return (int)(h % YOMI_HASH);
}

/** ����κǸ�ˡ��ɤߤ��Ĥ��ɲä��� */
static void
push_back_word_entry(struct yomi_entry *ye, const char *wt_name,
		     int freq, const char *word)
{
  wtype_t wt;
  if (freq == 0) {
    return ;
  }
  if (!anthy_type_to_wtype(wt_name, &wt)) {
    /* anthy���Τ�ʤ��ʻ� */
    return ;
  }
  ye->entries = realloc(ye->entries,
			sizeof(struct word_entry) *
			(ye->nr_entries + 1));

  ye->entries[ye->nr_entries].wt = strdup(wt_name);
  ye->entries[ye->nr_entries].freq = freq;
  ye->entries[ye->nr_entries].word = strdup(word);
  ye->nr_entries ++;
}

static int
parse_wtype(char *wtbuf, char *cur)
{
  /* �ʻ� */
  char *t;
  int freq;
  if (strlen(cur) >= MAX_WTYPE_LEN) {
    return 0;
  }
  strcpy(wtbuf, cur);
  t = strchr(wtbuf, '*');
  freq = 1;
  if (t) {
    int tmp_freq;
    *t = 0;
    t++;
    tmp_freq = atoi(t);
    if (tmp_freq) {
      freq = tmp_freq;
    }
  }
  return freq;
}

/* ʣ�������Ǥ�Ĺ���� 1,2,3, ... 9,a,b,c */
static int
get_element_len(xchar xc)
{
  if (xc > '0' && xc <= '9') {
    return xc - '0';
  }
  if (xc >= 'a' && xc <= 'z') {
    return xc - 'a' + 10;
  }
  return 0;
}

/** ʣ�����η��������å� */
static int
check_compound_candidate(xstr *index, const char *cur)
{
  xstr *xs = anthy_cstr_to_xstr(cur, 0);
  int i, total = 0;
  for (i = 0; i < xs->len - 1; i++) {
    if (xs->str[i] == '_') {
      total += get_element_len(xs->str[i+1]);
    }
  }
  anthy_free_xstr(xs);
  if (total != index->len) {
    fprintf(stderr, "Invalid compound candidate (%s, length = %d).\n",
	    cur, total);
    return 0;
  }
  return 1;
}

/** �ɤߤ��б�����Ԥ�ʬ�䤷�ơ������������ */
static void
push_back_word_entry_line(struct yomi_entry *ye, const char *ent)
{
  char *buf = alloca(strlen(ent) + 1);
  char *cur = buf;
  char *n;
  char wtbuf[MAX_WTYPE_LEN];
  int freq = 0;

  strcpy(buf, ent);
  wtbuf[0] = 0;

  while (1) {
    /* �ȡ�������ڤ롣cur�θ�ζ���\0��õ�� */
    for (n = cur; *n != ' ' && *n; n++) {
      if (*n == '\\') {
	n++;
      }
    }
    if (*n) {
      *n = 0;
    } else {
      n = NULL;
    }
    if (cur[0] == '#') {
      if (isalpha(cur[1])) {
	/* #XX*?? ��ѡ��� */
	freq = parse_wtype(wtbuf, cur);
      } else {
	if (cur[1] == '_' &&
	    check_compound_candidate(ye->index_xstr, &cur[1])) {
	  /* #_ ʣ����� */
	  push_back_word_entry(ye, wtbuf, freq, cur);
	}
      }
    } else {
      /* �ɤߤ��ɲ� */
      push_back_word_entry(ye, wtbuf, freq, cur);
    }
    if (!n) {
      return ;
    }
    cur = n;
    cur ++;
  }
}

/** Ʊ��ñ�줬̵���������å� */
static int
check_same_word(struct yomi_entry *ye, int idx)
{
  struct word_entry *base = &ye->entries[idx];
  int i;
  for (i = idx -1; i >= 0; i--) {
    struct word_entry *cur = &ye->entries[i];
    if (base->freq != cur->freq) {
      return 0;
    }
    if (strcmp(base->wt, cur->wt)) {
      return 0;
    }
    if (strcmp(base->word, cur->word)) {
      return 0;
    }
    return 1;
  }
  return 0;
}

/** qsort�Ѥ���Ӵؿ� */
static int
compare_word_entry(const void *p1, const void *p2)
{
  const struct word_entry *e1 = p1;
  const struct word_entry *e2 = p2;
  return e2->freq - e1->freq;
}

/** ����ʤ�ñ���ä� */
static int
normalize_word_entry(struct yomi_entry *ye)
{
  int i, nr_dup = 0;
  if (!ye) {
    return 0;
  }
  /* ñ����¤٤� */
  qsort(ye->entries, ye->nr_entries,
	sizeof(struct word_entry),
	compare_word_entry);
  /* ���֤ä��顢0�� */
  for (i = 0; i < ye->nr_entries; i++) {
    if (check_same_word(ye, i)) {
      ye->entries[i].freq = 0;
      nr_dup ++;
    }
  }
  /* �Ƥӥ����� */
  qsort(ye->entries, ye->nr_entries,
	sizeof(struct word_entry),
	compare_word_entry);
  return ye->nr_entries - nr_dup;
}

/** ��Ĥ��ɤߤ��Ф���ñ������Ƥ���Ϥ��� */
static int
output_yomi_entry(struct yomi_entry *ye)
{
  int i;
  int count = 0;

  if (!ye) {
    return 0;
  }
  /* ��ñ�����Ϥ��� */
  for (i = 0; i < ye->nr_entries; i++) {
    struct word_entry *we = &ye->entries[i];
    /**/
    if (!we->freq) {
      continue;
    }
    if (i > 0) {
      /* ����ܰʹߤ϶��򤫤�Ϥޤ� */
      count += fprintf(yomi_entry_out, " ");
    }
    /* �ʻ�����٤���Ϥ��� */
    if (i == 0 ||
	(strcmp(ye->entries[i-1].word, we->word) ||
	 strcmp(ye->entries[i-1].wt, we->wt) ||
	 ye->entries[i-1].freq != we->freq)) {
      count += fprintf(yomi_entry_out, "%s", we->wt);
      if (we->freq > 1) {
	count += fprintf(yomi_entry_out, "*%d", we->freq);
      }
      count += fprintf(yomi_entry_out, " ");
    }
    /* ñ�����Ϥ����꤬����ñ���id */
    we->offset = count + ye->offset;
    /* ñ�����Ϥ��� */
    count += fprintf(yomi_entry_out, "%s", we->word);
  }

  fputc(0, yomi_entry_out);
  return count + 1;
}

/*�����ɤߤ��б�����yomi_entry���֤�
**/
struct yomi_entry *
find_yomi_entry(struct yomi_entry_list *yl, xstr *index, int create)
{
  struct yomi_entry *ye;
  int hash = index_hash(index);
  int search = 0;
  /* hash chain����õ�� */
  for (ye = yl->hash[hash];ye ; ye = ye->hash_next) {
    search ++;
    if (!anthy_xstrcmp(ye->index_xstr, index)) {
      return ye;
    }
  }
  if (!create) {
    return NULL;
  }

  /* ̵���Τǳ��� */
  ye = malloc(sizeof(struct yomi_entry));
  ye->nr_entries = 0;
  ye->entries = 0;
  ye->next = NULL;
  ye->index_xstr = anthy_xstr_dup(index);

  /* hash chain�ˤĤʤ� */
  ye->hash_next = yl->hash[hash];
  yl->hash[hash] = ye;

  /* �ꥹ�ȤˤĤʤ� */

  ye->next = yl->head;
  yl->head = ye;

  yl->nr_entries ++;

  return ye;
}

/* ����ե��������hash bitmap�˥ޡ������դ��� */
static void
mark_hash_array(unsigned char *hash_array, xstr *xs)
{
  int val, idx, bit, mask;
  val = anthy_xstr_hash(xs);
  val &= (YOMI_HASH_ARRAY_SIZE*YOMI_HASH_ARRAY_BITS-1);
  idx=(val>>YOMI_HASH_ARRAY_SHIFT)&(YOMI_HASH_ARRAY_SIZE-1);
  bit= val & ((1<<YOMI_HASH_ARRAY_SHIFT)-1);
  mask = (1<<bit);
  if (hash_array[idx] & mask) {
    yomi_hash_collision ++;
  }
  hash_array[idx] |= mask;
}

/* �ɤ�hash�Υӥåȥޥåפ��� */
static void
mk_yomi_hash(FILE *yomi_hash_out, struct yomi_entry_list *yl)
{
  unsigned char *hash_array;
  int i;
  struct yomi_entry *ye;
  hash_array = (unsigned char *)malloc(YOMI_HASH_ARRAY_SIZE);
  for (i = 0; i < YOMI_HASH_ARRAY_SIZE; i++) {
    hash_array[i] = 0;
  }
  for (i = 0; i < yl->nr_valid_entries; i++) {
    ye = yl->ye_array[i];
    mark_hash_array(hash_array, ye->index_xstr);
  }
  fwrite(hash_array, YOMI_HASH_ARRAY_SIZE, 1, yomi_hash_out);
  printf("generated yomi hash bitmap (%d collisions/%d entries)\n",
	 yomi_hash_collision, yl->nr_valid_entries);
	 
}

static struct adjust_command *
parse_modify_freq_command(const char *buf)
{
  char *line = alloca(strlen(buf) + 1);
  char *yomi, *wt, *word, *type_str;
  struct adjust_command *cmd;
  int type = 0;
  strcpy(line, buf);
  yomi = strtok(line, " ");
  wt = strtok(NULL, " ");
  word = strtok(NULL, " ");
  type_str = strtok(NULL, " ");
  if (!yomi || !wt || !word || !type_str) {
    return NULL;
  }
  if (!strcmp(type_str, "up")) {
    type = ADJUST_FREQ_UP;
  }
  if (!strcmp(type_str, "down")) {
    type = ADJUST_FREQ_DOWN;
  }
  if (!strcmp(type_str, "kill")) {
    type = ADJUST_FREQ_KILL;
  }
  if (!type) {
    return NULL;
  }
  cmd = malloc(sizeof(struct adjust_command));
  cmd->type = type;
  cmd->yomi = anthy_cstr_to_xstr(yomi, 0);
  cmd->wt = strdup(wt);
  cmd->word = strdup(word);
  return cmd;
}

static void
parse_adjust_command(const char *buf, struct adjust_command *ac_list)
{
  struct adjust_command *cmd = NULL;
  if (!strncmp("\\modify_freq ", buf, 13)) {
    cmd = parse_modify_freq_command(&buf[13]);
  }
  if (cmd) {
    cmd->next = ac_list->next;
    ac_list->next = cmd;
  }
}

/** ������Ԥ����ɤ߹���ǥꥹ�Ȥ���
 * ���Υ��ޥ�ɤΥ��� */
static void
parse_dict_file(FILE *fin, struct yomi_entry_list *yl,
		struct adjust_command *ac_list)
{
  xstr *index_xs;
  char buf[MAX_LINE_LEN];
  char *ent, *index_str;
  struct yomi_entry *ye = NULL;

  /* ���Ԥ��Ľ��� */
  while (read_line(fin, buf)) {
    if (buf[0] == '\\') {
      parse_adjust_command(buf, ac_list);
      continue ;
    }
    index_str = get_index_from_line(buf);
    if (!index_str) {
      break;
    }
    ent = get_entry_from_line(buf);
    index_xs = anthy_file_dic_str_to_xstr(index_str);

    /* �ɤߤ�30ʸ����ۤ������̵�� */
    if (index_xs->len < 31) {
      ye = find_yomi_entry(yl, index_xs, 1);
      push_back_word_entry_line(ye, ent);
    }

    free(ent);
    free(index_str);
    anthy_free_xstr(index_xs);
  }
}

static struct word_entry *
find_word_entry(struct yomi_entry_list *yl, xstr *yomi,
		char *wt, char *word)
{
    struct yomi_entry *ye = find_yomi_entry(yl, yomi, 0);
    int i;
    if (!ye) {
      return NULL;
    }
    for (i = 0; i < ye->nr_entries; i++) {
      struct word_entry *we = &ye->entries[i];
      if (!strcmp(we->wt, wt) &&
	  !strcmp(we->word, word)) {
	return we;
      }
    }
    return NULL;
}
		
/* ����Ĵ���Υ��ޥ�ɤ�Ŭ�Ѥ��� */
static void
apply_adjust_command(struct yomi_entry_list *yl,
		     struct adjust_command *ac_list)
{
  struct adjust_command *cmd;
  for (cmd = ac_list->next; cmd; cmd = cmd->next) {
    struct word_entry *we = find_word_entry(yl, cmd->yomi,
					    cmd->wt, cmd->word);
    if (!we) {
      continue;
    }
    if (cmd->type == ADJUST_FREQ_UP) {
      we->freq *= 4;
    }
    if (cmd->type == ADJUST_FREQ_DOWN) {
      we->freq /= 4;
      if (we->freq == 0) {
	we->freq = 1;
      }
    }
    if (cmd->type == ADJUST_FREQ_KILL) {
      we->freq = 0;
    }
  }
}

/* qsort�Ѥ���Ӵؿ� */
static int
compare_yomi_entry(const void *p1, const void *p2)
{
  const struct yomi_entry *const *y1 = p1;
  const struct yomi_entry *const *y2 = p2;
  return anthy_xstrcmp((*y1)->index_xstr, (*y2)->index_xstr);
}

/* yomi_entry��sort���� */
static void
sort_word_dict(struct yomi_entry_list *yl)
{
  int i;
  struct yomi_entry *ye;
  yl->nr_valid_entries = 0;
  /* ñ�������ɤߤ����� yl->ye_array�˵ͤ�ľ�� */
  yl->ye_array = malloc(sizeof(struct yomi_entry *) * yl->nr_entries);
  for (i = 0, ye = yl->head; i < yl->nr_entries; i++, ye = ye->next) {
    if (ye->nr_entries > 0) {
      yl->ye_array[yl->nr_valid_entries] = ye;
      yl->nr_valid_entries ++;
    }
  }
  /* �����Ȥ��� */
  qsort(yl->ye_array, yl->nr_valid_entries,
	sizeof(struct yomi_entry *),
	compare_yomi_entry);
  /**/
  yl->nr_words = 0;
  for (i = 0; i < yl->nr_valid_entries; i++) {
    struct yomi_entry *ye = yl->ye_array[i];
    yl->nr_words += normalize_word_entry(ye);
  }
}

/** ñ�켭�����Ϥ���
 * �ޤ������ΤȤ��˼�����Υ��ե��åȤ�׻����� */
static void
output_word_dict(struct yomi_entry_list *yl)
{
  xstr *prev = NULL;
  int entry_index = 0;
  int page_index = 0;
  struct yomi_entry *ye = NULL;
  int i;

  /* �ޤ����ǽ���ɤߤ��Ф��륨��ȥ�Υ���ǥå�����񤭽Ф� */
  write_nl(page_index_out, page_index);

  /* ���ɤߤ��Ф���롼�� */
  for (i = 0; i < yl->nr_valid_entries; i++) {
    ye = yl->ye_array[i];
    /* �������ڡ����γ��� */
    if ((i % WORDS_PER_PAGE) == 0 && (i != 0)) {
      page_index ++;
      prev = NULL;
      begin_new_page(page_index);
    }

    /* �ɤߤ��б�����������Ϥ��� */
    page_index += output_diff(prev, ye->index_xstr);
    output_entry_index(entry_index);
    ye->offset = entry_index;
    entry_index += output_yomi_entry(ye);
    /***/
    prev = ye->index_xstr;
  }

  /* �Ǹ���ɤߤ�λ */
  entry_index += output_yomi_entry(ye);
  write_nl(yomi_entry_index_out, entry_index);
  write_nl(page_index_out, 0);

  /**/
  printf("Total %d indexes, %d words, (%d pages).\n",
	 yl->nr_valid_entries,
	 yl->nr_words,
	 yl->nr_valid_entries / WORDS_PER_PAGE + 1);
}

/** �ե�����Υ�������������� */
static int
get_file_size(FILE *fp)
{
  if (!fp) {
    return 0;
  }
  return (ftell (fp) + SECTION_ALIGNMENT - 1) & (-SECTION_ALIGNMENT);
}

static void
copy_file(FILE *in, FILE *out)
{
  int i;
  size_t nread;
  char buf[BUFSIZ];

  /* Pad OUT to the next aligned offset.  */
  for (i = ftell (out); i & (SECTION_ALIGNMENT - 1); i++) {
    fputc (0, out);
  }

  /* Copy the contents.  */
  rewind (in);
  while ((nread = fread (buf, 1, sizeof buf, in)) > 0) {
    if (fwrite (buf, 1, nread, out) < nread) {
      /* Handle short write (maybe disk full).  */
      fprintf (stderr, "%s: %s: write error: %s\n",
	       progname, output_fn, strerror (errno));
      exit (1);
    }
  }
}

static void
generate_header(FILE *fp)
{
  int buf[NR_HEADER_SECTIONS];
  int i;
  struct file_section *fs;
  int off;

  /* ����� */
  for (i = 0; i < NR_HEADER_SECTIONS; i++) {
    buf[i] = 0;
  }

  /* �إå� */
  buf[0] = NR_HEADER_SECTIONS * sizeof(int);
  buf[1] = 0;

  /* �ƥ��������Υ��ե��å� */
  off = buf[0];
  for (i = 2, fs = file_array; fs->fpp; fs ++, i++) {
    buf[i] = off;
    off += get_file_size(*(fs->fpp));
  }

  /* �ե�����ؽ��Ϥ��� */
  for (i = 0; i < NR_HEADER_SECTIONS; i++) {
    write_nl(fp, buf[i]);
  }
}

/* �ƥ��������Υե������ޡ������ơ��ҤȤĤμ���ե�������� */
static void
link_dics(void)
{
  FILE *fp;
  struct file_section *fs;

  fp = fopen (output_fn, "w");
  if (!fp) {
      fprintf (stderr, "%s: %s: cannot create: %s\n",
	       progname, output_fn, strerror (errno));
      exit (1);
  }

  /* �إå�����Ϥ��� */
  generate_header(fp);

  /* �ƥ��������Υե�������礹�� */
  for (fs = file_array; fs->fpp; fs ++) {
    copy_file(*(fs->fpp), fp);
  }

  if (fclose (fp)) {
    fprintf (stderr, "%s: %s: write error: %s\n",
	     progname, output_fn, strerror (errno));
    exit (1);
  }
}

static void
setup_versatile_hash(struct versatile_hash *vh)
{
  vh->buf = malloc(VERSATILE_HASH_SIZE);
  memset(vh->buf, 0, VERSATILE_HASH_SIZE);
}

static void
write_out_versatile_hash(struct versatile_hash *vh)
{
  int i;
  int nr = 0;
  for (i = 0; i < VERSATILE_HASH_SIZE; i++) {
    if (vh->buf[i]) {
      nr ++;
    }
  }
  printf("versatile hash density %d/%d\n", nr, VERSATILE_HASH_SIZE);
  fwrite(vh->buf, VERSATILE_HASH_SIZE, 1, versatile_hash_out);
  free(vh->buf);
}

static void
init(void)
{
  int res;
  res = anthy_init_xstr();
  if (res == -1) {
    fprintf (stderr, "failed to init dic lib\n");
    exit(1);
  }

#ifdef USE_UCS4
  euc_to_utf8 = iconv_open("UTF-8", "EUC-JP");
  if (euc_to_utf8 == (iconv_t) -1) {
    fprintf(stderr, "failed in iconv_open(%s)", strerror(errno));
    exit(1);
  }
#endif
}

/* ���ޥ�ɥ饤�󥪥ץ����β�� */
#define OPT_NONE 0
#define OPT_HELP 1
#define OPT_OUTFN 2
#define OPT_DICFN 3
#define OPT_UCFN 4

struct cmd_arg {
  int type;
  char *fn;
  struct cmd_arg *next;
};

static void
link_cmd_arg(struct cmd_arg *head, int type, const char *fn)
{
  struct cmd_arg *arg = malloc(sizeof(struct cmd_arg));
  arg->type = type;
  arg->fn = strdup(fn);
  arg->next = head->next;
  head->next = arg;
}

static void
parse_args(struct cmd_arg *arg_list, int argc, char **argv)
{
  int i;
  char *cur;

  progname = argv[0];

  arg_list->next = NULL;
  arg_list->type = OPT_NONE;

  for (i = 1; i < argc; i++) {
    cur = argv[i];
    if (!strcmp(cur, "--help")) {
      arg_list->type = OPT_HELP;
    }
    if (i + 1 < argc) {
      if (!strcmp(cur, "-o")) {
	link_cmd_arg(arg_list, OPT_OUTFN, argv[i+1]);
	i++;
	continue;
      }
      if (!strcmp(cur, "-uc")) {
	link_cmd_arg(arg_list, OPT_UCFN, argv[i+1]);
	i++;
	continue;
      }
    }
    link_cmd_arg(arg_list, OPT_DICFN, argv[i]);
  }
}

/**/
int
main(int argc, char **argv)
{
  struct yomi_entry_list yl;
  struct versatile_hash vh;
  struct adjust_command ac_list;
  struct uc_dict *ud;
  struct cmd_arg arg_list;
  struct cmd_arg *arg;
  int i;

  init();
  parse_args(&arg_list, argc, argv);

  /* ñ�켭����� */
  yl.head = NULL;
  yl.nr_entries = 0;
  for (i = 0; i < YOMI_HASH; i++) {
    yl.hash[i] = NULL;
  }
  ac_list.next = NULL;

  if (arg_list.type == OPT_HELP) {
    print_usage();
  }

  for (arg = arg_list.next; arg; arg = arg->next) {
    if (arg->type == OPT_OUTFN) {
      output_fn = arg->fn;
    }
    if (arg->type == OPT_DICFN) {
      FILE *fp;
      /* �ե�����̾�����ꤵ�줿�Τ��ɤ߹��� */
      fp = fopen(arg->fn, "r");
      if (fp) {
	printf("file = %s\n", arg->fn);
	parse_dict_file(fp, &yl, &ac_list);
      }
    }
  }

  apply_adjust_command(&yl, &ac_list);

  /* �ɤߤ��¤��ؤ��� */
  sort_word_dict(&yl);

  /* ñ�켭�����Ϥ��� */
  open_output_files();
  output_word_dict(&yl);

  /* �ɤߥϥå������ */
  mk_yomi_hash(yomi_hash_out, &yl);

  /* ������ɤ߹��� */
  ud = create_uc_dict(&yl);
  for (arg = arg_list.next; arg; arg = arg->next) {
    if (arg->type == OPT_UCFN) {
      read_uc_file(ud, arg->fn);
      printf("uc = %s\n", arg->fn);
    }
  }

  /* ���㼭����� */
  make_ucdict(uc_out, ud);

  /* ���ѥϥå������ */
  setup_versatile_hash(&vh);

  /* ���㼭���ϥå���˽񤭹��� */
  fill_uc_to_hash(&vh, ud);

  /* ���ѥϥå����񤭽Ф� */
  write_out_versatile_hash(&vh);

  /* ����ե�����ˤޤȤ�� */
  flush_output_files();
  link_dics();

  return 0;
}
