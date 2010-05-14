/* ����ν������ꤹ�뤿��Υ⥸�塼�� */
#ifndef _ordering_h_included_
#define _ordering_h_included_

#include <xstr.h>

struct segment_list;
struct splitter_context;

/** ordering_context��wrapper��¤��
 */
struct ordering_context_wrapper{
  struct ordering_context *oc;
};

void anthy_proc_commit(struct segment_list *, struct splitter_context *);

void anthy_sort_candidate(struct segment_list *c, int nth);

void anthy_init_ordering_context(struct segment_list *,
				 struct ordering_context_wrapper *);
void anthy_release_ordering_context(struct segment_list *,
				    struct ordering_context_wrapper *);
void anthy_sort_metaword(struct segment_list *seg);

void anthy_do_commit_prediction(xstr *src, xstr *xs);

#endif
