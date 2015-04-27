/*
 * Copyright (C) 2000-2007 TABATA Yusuke
 * Copyright (C) 2004-2006 YOSHIDA Yuichi
 */
/*
 * ��°�쥰��դ�Х��ʥ경����
 * init_word_seq_tab()
 *   ��°��ơ��֥���ΥΡ��ɤؤΥݥ��󥿤ν����
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <anthy/xstr.h>
#include <anthy/splitter.h>
#include <anthy/anthy.h>
#include <anthy/depgraph.h>
#include <anthy/diclib.h>

static struct dep_node* gNodes;
static char** gNodeNames;
static int nrNodes;

struct rule {
  wtype_t wt;
  const char *wtype_name;
  int node_id;
};

/* ñ����³�롼�� */
static struct rule *gRules;
static int nrRules;

static int 
get_node_id_by_name(const char *name)
{
  int i;
  /* ��Ͽ�ѤߤΤ�Τ���õ�� */
  for (i = 0; i < nrNodes; i++) {
    if (!strcmp(name,gNodeNames[i])) {
      return i;
    }
  }
  /* �ʤ��ä��ΤǺ�� */
  gNodes = realloc(gNodes, sizeof(struct dep_node)*(nrNodes+1));
  gNodeNames = realloc(gNodeNames, sizeof(char*)*(nrNodes+1));
  gNodes[nrNodes].nr_branch = 0;
  gNodes[nrNodes].branch = 0;
  gNodeNames[nrNodes] = strdup(name);
  nrNodes++;
  return nrNodes-1;
}


/* ���ܾ�狼��branch���ܤ��Ф� */
static struct dep_branch *
find_branch(struct dep_node *node, xstr **strs, int nr_strs)
{
  struct dep_branch *db;
  int i, j;
  /* Ʊ�����ܾ��Υ֥�����õ�� */
  for (i = 0; i < node->nr_branch; i++) {
    db = &node->branch[i];
    if (nr_strs != db->nr_strs) {
      continue ;
    }
    for (j = 0; j < nr_strs; j++) {
      if (anthy_xstrcmp(db->str[j], strs[j])) {
	goto fail;
      }
    }
    /**/
    return db;
  fail:;
  }
  /* �������֥�������ݤ��� */
  node->branch = realloc(node->branch,
			 sizeof(struct dep_branch)*(node->nr_branch+1));
  db = &node->branch[node->nr_branch];
  node->nr_branch++;
  db->str = malloc(sizeof(xstr*)*nr_strs);
  for (i = 0; i < nr_strs; i++) {
    db->str[i] = strs[i];
  }
  db->nr_strs = nr_strs;
  db->nr_transitions = 0;
  db->transition = 0;
  return db;
}

/*
 * ���ܤ�parse����
 *  doc/SPLITTER����
 */
static void
parse_transition(char *token, struct dep_transition *tr)
{
  int ct = CT_NONE;
  enum dep_class dc = DEP_NONE;
  char *str = token;
  tr->head_pos = POS_NONE;
  tr->weak = 0;
  /* ���ܤ�°�������*/
  while (*token != '@') {
    switch(*token){
    case ':':
    case '.':
      tr->weak = 1;
      break;
    case 'C':
      /* ���ѷ� */
      switch (token[1]) {
      case 'z': ct = CT_MIZEN; break;
      case 'y': ct = CT_RENYOU; break;
      case 's': ct = CT_SYUSI; break;
      case 't': ct = CT_RENTAI; break;
      case 'k': ct = CT_KATEI; break;
      case 'm': ct = CT_MEIREI; break;
      case 'g': ct = CT_HEAD; break;
      }
      token ++;
      break;
    case 'H':
      /* ��Ω�������ʻ� */
      switch (token[1]) {
      case 'n':	tr->head_pos = POS_NOUN; break;
      case 'v':	tr->head_pos = POS_V; break;
      case 'j':	tr->head_pos = POS_AJV; break;
      }
      token ++;
      break;
    case 'S':
      /* ʸ���°�� */
      switch (token[1]) {
	/*      case 'n': sc = DEP_NO; break;*/
      case 'f': dc = DEP_FUZOKUGO; break;
      case 'k': dc = DEP_KAKUJOSHI; break;
      case 'y': dc = DEP_RENYOU; break;
      case 't': dc = DEP_RENTAI; break;
      case 'e': dc = DEP_END; break;
      case 'r': dc = DEP_RAW; break;
      default: printf("unknown (S%c)\n", token[1]);
      }
      token ++;
      break;
    default:
      printf("Unknown (%c) %s\n", *token, str);
      break;
    }
    token ++;
  }
  /* @�����ϥΡ��ɤ�̾�� */
  tr->next_node = get_node_id_by_name(token);
  /**/
  tr->ct = ct;
  tr->dc = dc;
}

/*
 * �Ρ���̾ ���ܾ��+ ������+
 */
static void
parse_dep(char **tokens, int nr)
{
  int id, row = 0;
  struct dep_branch *db;
  struct dep_node *dn;
  int nr_strs;
  xstr **strs = alloca(sizeof(xstr*) * nr);

  /* �Ρ��ɤȤ���id����� */
  id = get_node_id_by_name(tokens[row]);
  dn = &gNodes[id];
  row ++;

  nr_strs = 0;

  /* ���ܾ�����°���������� */
  for (; row < nr && tokens[row][0] == '\"'; row++) {
    char *s;
    s = strdup(&tokens[row][1]);
    s[strlen(s)-1] =0;
    strs[nr_strs] = anthy_cstr_to_xstr(s, ANTHY_UTF8_ENCODING);
    nr_strs ++;
    free(s);
  }

  /* ���ܾ�郎�ʤ����Ϸٹ��Ф��ơ��������ܾ����ɲä��� */
  if (nr_strs == 0) {
    char *s;

    fprintf (stderr, "node %s has a branch without any transition condition.\n",
	     tokens[0]);
    s = strdup("");
    strs[0] = anthy_cstr_to_xstr(s, ANTHY_UTF8_ENCODING);
    nr_strs = 1;
    free(s);
  }

  /* �֥�����������ΥΡ��ɤ��ɲä��� */
  db = find_branch(dn, strs, nr_strs);
  for ( ; row < nr; row++){
    struct dep_transition *tr;
    db->transition = realloc(db->transition,
			     sizeof(struct dep_transition)*
			     (db->nr_transitions+1));
    tr = &db->transition[db->nr_transitions];
    parse_transition(tokens[row], tr);
    db->nr_transitions ++;
  }
}

/* ʸˡ����ե�������˶��ΥΡ��ɤ����뤫�����å����� */
static void
check_nodes(void)
{
  int i;
  for (i = 1; i < nrNodes; i++) {
    if (gNodes[i].nr_branch == 0) {
      fprintf (stderr, "node %s has no branch.\n", gNodeNames[i]);
    }
  }
}

static int
get_tokens (char *buf, char **tokens, int n)
{
  int i;
  char *p = buf;

  for (i = 0; i < n; i++)
    {
      tokens[i] = p;
      p = strchr (p, ' ');
      if (p == NULL)
	return i + 1;

      *p++ = '\0';
    }

  return -1;
}

#define MAX_TOKEN 256
#define BUFSIZE 1024
#define DEPWORD_INPUT_FILENAME "all.depword"
#ifndef INDEPWORD_INPUT_FILENAME
#define INDEPWORD_INPUT_FILENAME "indepword-wt.txt"
#endif

static void
init_depword_tab(void)
{
  FILE *fp;
  char buf[BUFSIZE];
  int lineno = 0;

  get_node_id_by_name ("@");

  if ((fp = fopen (DEPWORD_INPUT_FILENAME, "r")) == NULL)
    {
      fprintf (stderr, "Failed to open (%s).\n", DEPWORD_INPUT_FILENAME);
      exit (1);
    }

  while (fgets (buf, BUFSIZE, fp) != NULL)
    {
      char *tokens[MAX_TOKEN];
      int nr;
      char *p;

      if ((p = strchr (buf, '\n')) == NULL)
	goto error;

      *p = '\0';

      lineno++;
      nr = get_tokens (buf, tokens, MAX_TOKEN);
      if (nr < 0)
	{
	error:
	  fprintf (stderr, "Too long line (%d): ignored\n", lineno);
	  continue;
	}

      parse_dep (tokens, nr);
    }

  fclose (fp);

  check_nodes ();
}


static void
parse_indep (char **tokens, int nr, int lineno)
{
  int node;
  wtype_t wt;
  int i;

  if (anthy_type_to_wtype (tokens[0], &wt) == NULL)
    {
      fprintf (stderr, "%d: no such WT\n", lineno);
      return;
    }

  for (i = 0; i < nrRules; i++)
    if (anthy_wtype_equal (gRules[i].wt, wt))
      return;

  gRules = (struct rule *)realloc (gRules, sizeof (struct rule)*(nrRules+1));
  if (gRules == NULL)
    {
      fprintf (stderr, "%d: malloc failed.\n", lineno);
      exit (1);
    }

  if (nr == 2)
    node = get_node_id_by_name (tokens[1]);
  else if (nr >= 3)
    {
      int i;
      struct dep_branch *db;
      struct dep_node *dn;
      xstr *strs[1];

      node = get_node_id_by_name (tokens[0]); /* New node */
      dn = &gNodes[node];
      strs[0] = anthy_cstr_to_xstr ("", ANTHY_UTF8_ENCODING);
      db = find_branch (dn, strs, 1);

      db->transition = (struct dep_transition *)realloc (db->transition,
							 sizeof (struct dep_transition)*
							 (db->nr_transitions + nr - 1));

      for (i = 1; i < nr; i++)
	{
	  struct dep_transition *tr;

	  tr = &db->transition[db->nr_transitions];
	  parse_transition (tokens[i], tr);
	  db->nr_transitions++;
	}
    }
  else
    {
      if (nr != 1)
	fprintf (stderr, "%d: syntax error (ignored).\n", lineno);
      return;
    }

  gRules[nrRules].wt = wt;
  gRules[nrRules].wtype_name = strdup (tokens[0]);
  gRules[nrRules].node_id = node;
  nrRules++;
}

/** ��Ω�줫�������ɽ */
static void
init_indep_word_seq_tab (void)
{
  FILE *fp;
  char buf[BUFSIZE];
  int lineno = 0;

  if ((fp = fopen (INDEPWORD_INPUT_FILENAME, "r")) == NULL)
    {
      fprintf (stderr, "Failed to open (%s).\n", INDEPWORD_INPUT_FILENAME);
      exit (1);
    }

  while (fgets (buf, BUFSIZE, fp) != NULL)
    {
      char *tokens[MAX_TOKEN];
      int nr;
      char *p;

      if ((p = strchr (buf, '\n')) == NULL)
	goto error;

      *p = '\0';

      lineno++;
      nr = get_tokens (buf, tokens, MAX_TOKEN);
      if (nr < 0)
	{
	error:
	  fprintf (stderr, "Too long line (%d): ignored\n", lineno);
	  continue;
	}

      parse_indep (tokens, nr, lineno);
    }
}

/*  
    �ͥåȥ���Х��ȥ���������4byte�񤭽Ф�
*/
static void
write_nl(FILE* fp, int i)
{
  i = anthy_dic_htonl(i);
  fwrite(&i, sizeof(int), 1, fp);
}

static void
write_transition(FILE* fp, struct dep_transition* transition)
{
  write_nl(fp, transition->next_node); 
  write_nl(fp, transition->ct); 
  write_nl(fp, transition->dc); 
  write_nl(fp, transition->head_pos); 
  write_nl(fp, transition->weak); 
}

static void
write_xstr(FILE* fp, xstr* str)
{
  int i;
  xchar c;
  write_nl(fp, str->len);

  for (i = 0; i < str->len; i++) {
    c = anthy_dic_htonl(str->str[i]);
    fwrite(&c, sizeof(xchar), 1, fp);
  }
}

static void
write_branch(FILE* fp, struct dep_branch* branch)
{
  int i;

  write_nl(fp, branch->nr_strs);
  for (i = 0; i < branch->nr_strs; ++i) {
    write_xstr(fp, branch->str[i]);
  }

  write_nl(fp, branch->nr_transitions);
  for (i = 0; i < branch->nr_transitions; ++i) {
    write_transition(fp, &branch->transition[i]);
  }
}

static void
write_node(FILE* fp, struct dep_node* node)
{
  int i;
  write_nl(fp, node->nr_branch);
  for (i = 0; i < node->nr_branch; ++i) {
    write_branch(fp, &node->branch[i]);
  }
}

static void
write_wtype (FILE *fp, const char *name)
{
  int i;
  int len = strlen (name);

  if (len >= 8)
    {
      fprintf (stderr, "WT name too long (%s).\n", name);
      exit (1);
    }

  for (i = 0; i < len; i++)
    fputc (name[i], fp);

  for (; i < 8; i++)
    fputc('\0', fp);
}

static void
write_file(const char* file_name)
{
  int i;
  FILE* fp = fopen(file_name, "w");
  int* node_offset = malloc(sizeof(int) * nrNodes); /* gNodes�Υե������ΰ��� */

  /* �ƥ롼�� */
  write_nl(fp, nrRules);
  for (i = 0; i < nrRules; ++i) {
    write_wtype(fp, gRules[i].wtype_name);
    write_nl(fp, gRules[i].node_id);
  }

  write_nl(fp, nrNodes);

  for (i = 0; i < nrNodes; ++i) {
    write_node(fp, &gNodes[i]);
  }

  free(node_offset);
  fclose(fp);
}

int
main(int argc, char* argv[])
{
  anthy_init_wtypes ();

  /* ��°�쥰��� */
  init_depword_tab();
  /* ��Ω�줫�������ɽ */
  init_indep_word_seq_tab();

  write_file("anthy.dep");

  return 0;
}
