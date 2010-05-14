/* 
 * Anthy�����ǻȤ�ʸ����
 * �ä˼������ä��褦�Ȥ��Ƥ���櫓�Ǥ�̵���Τǡ�
 * �����ˤ���ؿ��λ��Ѥ϶������ʤ���
 */
#ifndef _xstr_h_included_
#define _xstr_h_included_

/** ʸ����
 * EUC�����äƤ��� */
typedef int xchar;

/** ʸ����
 * xstr��typedef����Ƥ���
 */
typedef struct xstr_ {
  /** ʸ����ؤΥݥ��� */
  xchar *str;
  /** xchar�ο� */
  int len;
} xstr;

/* �ǥХå��Ѥν��ϴؿ� */
void anthy_putxchar(xchar );
void anthy_putxstr(xstr *);
void anthy_putxstrln(xstr *);

/* C��ʸ����ؤν񤭽Ф� */
int anthy_sputxchar(char *, xchar , int encoding);
int anthy_sputxstr(char *, xstr *);
int anthy_snputxstr(char *, int , xstr *, int encoding);

/* xstr��str����malloc����롢free��ξ���������뤫anthy_free_xstr�ǲ������� */
xstr *anthy_cstr_to_xstr(const char *, int );
/* ��̤�malloc�ǳ��ݤ���� */
char *anthy_xstr_to_cstr(xstr *, int);
xstr *anthy_file_dic_str_to_xstr(const char *);
char *anthy_xstr_to_file_dic_str(xstr *);

/* xstr��str����malloc����� */
xstr *anthy_xstr_dup(xstr *);
void anthy_free_xstr(xstr *);

/* ��̤�malloc�ǳ��ݤ���� */
xchar *anthy_xstr_dup_str(xstr *);
void anthy_free_xstr_str(xstr *);

/* ʸ����򥳥ԡ����� */
xstr* anthy_xstrcpy(xstr *, xstr *);
/* ʸ�������Ӥ��롣strcmp��Ʊ����ư��(�֤��ͤ����˰�̣������) */
int anthy_xstrcmp(xstr *, xstr *);
/* nʸ���ܤޤ�ʸ�������Ӥ��롣strncmp��Ʊ����ư��(�֤��ͤ����˰�̣������) */
int anthy_xstrncmp(xstr *, xstr *, int);
/* s->str��realloc���� */
xstr *anthy_xstrcat(xstr *s, xstr *d);
/* xs->str��realloc���� */
xstr *anthy_xstrappend(xstr *xs, xchar c);

/* strtoll��xstr�� */
long long anthy_xstrtoll(xstr *);
/* ���ѿ�������Ⱦ�ѿ����ؤ��Ѵ� */
xstr *anthy_xstr_wide_num_to_num(xstr *);
/* �Ҥ餬�ʤ��饫�����ʤؤ��Ѵ� */
xstr *anthy_xstr_hira_to_kata(xstr *);

/*  xchar�η� */
#define XCT_ALL 0xffffffff
#define XCT_NONE 0
#define XCT_HIRA 1
#define XCT_KATA 2
#define XCT_ASCII 4
#define XCT_NUM 8
#define XCT_WIDENUM 16
#define XCT_OPEN 32
#define XCT_CLOSE 64
/* ľ����ʸ���ΰ��� */
#define XCT_PART 128
/* ���� */
#define XCT_DEP 256
/* ������³����°��ʸ�� */
#define XCT_STRONG 512
/* ���� */
#define XCT_SYMBOL 1024
/* ���� */
#define XCT_KANJI 2048
/* ������ */
#define XCT_PUNCTUATION 4096

/** XCT_*���֤äƤ��� */
int anthy_get_xchar_type(const xchar );
/** ���Ƥ�ʸ�����Ф���XCT_*�������Ѥ�Ȥä���� */
int anthy_get_xstr_type(const xstr *);

/* hash */
int anthy_xstr_hash(xstr *);

/* xstr.c */
int anthy_init_xstr(void);
void anthy_quit_xstr(void);
void anthy_xstr_set_print_encoding(int );
/* �����file_dic.c������Ѥ��뤿��Υ��󥳡��ǥ��󥰴ط��δؿ�
   USE_UCS4��������줿�Ȥ��������Τ�¸�ߤ��� */
const char *
anthy_utf8_to_ucs4_xchar(const char *s, xchar *res);


#endif
