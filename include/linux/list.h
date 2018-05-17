#ifndef _LINUX_LIST_H
#define _LINUX_LIST_H

#include <urcu/list.h>

#define list_head			cds_list_head
#define LIST_HEAD_INIT(l)		CDS_LIST_HEAD_INIT(l)
#define LIST_HEAD(l)			CDS_LIST_HEAD(l)
#define INIT_LIST_HEAD(l)		CDS_INIT_LIST_HEAD(l)
#define list_add(n, h)			cds_list_add(n, h)
#define list_add_tail(n, h)		cds_list_add_tail(n, h)
#define __list_del_entry(l)		cds_list_del(l)
#define list_del(l)			cds_list_del(l)
#define list_del_init(l)		cds_list_del_init(l)
#define list_replace(o, n)		cds_list_replace(o, n)
#define list_replace_init(o, n)		cds_list_replace_init(o, n)
#define list_move(l, h)			cds_list_move(l, h)
#define list_empty(l)			cds_list_empty(l)
#define list_splice(l, h)		cds_list_splice(l, h)
#define list_entry(p, t, m)		cds_list_entry(p, t, m)
#define list_first_entry(p, t, m)	cds_list_first_entry(p, t, m)
#define list_for_each(p, h)		cds_list_for_each(p, h)
#define list_for_each_prev(p, h)	cds_list_for_each_prev(p, h)
#define list_for_each_safe(p, n, h)	cds_list_for_each_safe(p, n, h)
#define list_for_each_prev_safe(p, n, h) cds_list_for_each_prev_safe(p, n, h)
#define list_for_each_entry(p, h, m)	cds_list_for_each_entry(p, h, m)
#define list_for_each_entry_reverse(p, h, m) cds_list_for_each_entry_reverse(p, h, m)
#define list_for_each_entry_safe(p, n, h, m) cds_list_for_each_entry_safe(p, n, h, m)
#define list_for_each_entry_safe_reverse(p, n, h, m) cds_list_for_each_entry_safe_reverse(p, n, h, m)

static inline int list_empty_careful(const struct list_head *head)
{
	struct list_head *next = head->next;
	return (next == head) && (next == head->prev);
}

static inline void list_move_tail(struct list_head *list,
				  struct list_head *head)
{
	list_del(list);
	list_add_tail(list, head);
}

static inline void list_splice_init(struct list_head *list,
				    struct list_head *head)
{
	list_splice(list, head);
	INIT_LIST_HEAD(list);
}

#define list_last_entry(ptr, type, member) \
	list_entry((ptr)->prev, type, member)

#define list_first_entry_or_null(ptr, type, member) \
	(!list_empty(ptr) ? list_first_entry(ptr, type, member) : NULL)

/* hlists: */

#include <urcu/hlist.h>

#define hlist_head			cds_hlist_head
#define hlist_node			cds_hlist_node

#endif /* _LIST_LIST_H */
