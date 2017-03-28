/* vim:set tw=80 ts=8 sw=8 sts=8 cc=80 noet cino+=t0: */
#ifndef LL_H
#define LL_H

#include "common.h"

/* doubly linked list */

struct ll_node {
	struct ll_node *next, *prev;
	int val;
};

struct ll {
	struct ll_node *head, *tail;
};

void ll_append(struct ll *ll, int val);

void ll_print(struct ll *ll);

void ll_move_to_front(struct ll *ll, struct ll_node *node);

void ll_delete(struct ll *ll);

#endif /* LL_H */
