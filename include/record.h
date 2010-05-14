/* �ؽ�������ʤɤ���¸����ǡ����١��� */
#ifndef _record_h_included_
#define _record_h_included_
/*
 * �ǡ����١�����̾������ʣ����section���鹽������ƥ���������
 * ʸ����򥭡��Ȥ��ƹ�®�˼��Ф����Ȥ��Ǥ���column����ʤ롣
 *
 * �ǡ����١����ϥ�����section�䥫����column�ʤɤξ��֤����
 * ���Ϥ�����Ф��ƹԤ��롣
 * section���column�Ͻ���ط����äƤ���
 * ���ν���ط��Ȥ��̤�LRU�ν�����äƤ���
 */

#include "xstr.h"

/*
 * ������section�����ꤹ��
 * name: section��̾��
 * create_if_not_exist: ����section���ʤ���к�뤫�ɤ����Υե饰
 * �֤���: ���� 0 ������ -1
 * ���Ԥλ��ˤϥ�����section��̵���ˤʤ�
 * ��˥�����column��̵���ˤʤ�
 */
int anthy_select_section(const char *name, int create_if_not_exist);

/*
 * ������section�椫��name��column�򥫥���column�ˤ���
 * name: column��̾��
 * create_if_not_exist: ����column���ʤ���к�뤫�ɤ����Υե饰
 * �֤���: ���� 0 ������ -1
 * ���Ԥλ��ˤϥ�����column��̵���ˤʤ�
 */
int anthy_select_column(xstr *name, int create_if_not_exist);

/*
 * ������section�椫��name�˺Ǥ�Ĺ��ʸ�����ǥޥå�����
 * ̾����column�򥫥���column�ˤ���
 * name: column��̾��
 * �֤���: ���� 0 ������ -1
 * ���Ԥλ��ˤϤ˥�����column��̵���ˤʤ�
 */
int anthy_select_longest_column(xstr *name);

/*
 * ������section��κǽ��column�򥫥���column�ˤ���
 * �֤���: ���� 0 ������ -1
 * ���Ԥλ��ˤϥ�����column��̵���ˤʤ�
 */
int anthy_select_first_column(void);

/*
 * ������column�μ���column�򥫥���column�ˤ���
 * �֤���: ���� 0 ������ -1
 * ������column���Ф����ѹ������äƤ⡢�ե�����ˤ���¸����ʤ�
 * ���Ԥλ��ˤϥ�����column��̵���ˤʤ�
 */
int anthy_select_next_column(void);

/*
 * ������section���������
 * ��˥�����section,column��̵���ˤʤ�
 */
void anthy_release_section(void);

/*
 * ������section��LRU�ꥹ�Ȥ���Ƭ����count�İʹߤ��������
 * ��˥�����column��̵���ˤʤ�
 */
void anthy_truncate_section(int count);


/* ���ߤ�column���Ф������ */
xstr *anthy_get_index_xstr(void);
int anthy_get_nr_values(void);
int anthy_get_nth_value(int );
xstr *anthy_get_nth_xstr(int );/* intern����Ƥ���xstr���֤���� */

void anthy_set_nth_value(int nth, int val);
void anthy_set_nth_xstr(int nth, xstr *xs);/* �����ǥ��ԡ������ */

void anthy_truncate_column(int nth);/* To Be Implemented */

/*
 * ������column��������롣��λ��Υ�����column������
 * ��˥�����column��̵���ˤʤ�
 */
void anthy_release_column(void);

/*
 * ������column��LRU����Ƭ�����ؤ�äƤ���
 * ��˥�����column��̵���ˤʤ�
 */
int anthy_mark_column_used(void);


void anthy_reload_record(void);

#endif
