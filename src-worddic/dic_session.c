/*
 * ����Υ��å������������񥭥�å�����Υ���ȥ꤬
 * �ɤ��Ѵ�����ƥ����Ȥˤ�äƻ��Ѥ���Ƥ��뤫��������롣
 * ���񥭥�å�����Υ���ȥ�����Ƥθ��ߥ����ƥ��֤�
 * �Ѵ�����ƥ����Ȥ��ȤäƤ��ʤ����Τߤ˲������뤳�Ȥ��Ǥ��롣
 */

#include <stdlib.h>
#include <string.h>

#include <alloc.h>
#include <conf.h>
#include "dic_main.h"
#include "dic_personality.h"
#include "mem_dic.h"

static struct dic_session *g_current_session;

/* ���å�����current�ѡ����ʥ�ƥ��Υ���å�����Ф��ƺ�������� */
struct dic_session *
anthy_create_session(void)
{
  int i;
  struct mem_dic * d = anthy_current_personal_dic_cache;
  for (i = 0; i < MAX_SESSION; i++) {
    if (d->sessions[i].is_free) {
      d->sessions[i].is_free = 0;
      d->sessions[i].dic = d;
      return &d->sessions[i];
    }
  }
  return 0;
}

void
anthy_activate_session(struct dic_session *d)
{
  g_current_session = d;
}

void
anthy_release_session(struct dic_session *d)
{
  if (g_current_session == d) {
    g_current_session = 0;
  }
  d->is_free = 1;
  anthy_invalidate_seq_ent_mask(d->dic, ~d->mask);
}

int
anthy_get_current_session_mask(void)
{
  if (g_current_session) {
    return g_current_session->mask;
  }
  return 0;
}

void
anthy_init_sessions(struct mem_dic *d)
{
  int i;
  for (i = 0; i < MAX_SESSION; i++) {
    d->sessions[i].id = i;
    d->sessions[i].mask = (1<<i);
    d->sessions[i].is_free = 1;
  }
}
