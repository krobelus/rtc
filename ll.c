/* vim:set tw=80 ts=8 sw=8 sts=8 cc=80 noet cino+=t0: */

#include "ll.h"

void ll_append(struct ll *ll, int val) {
	struct ll_node *node = malloc(sizeof(struct ll_node));
	node->next = node->prev = NULL;
	node->val = val;
	if(ll->head == NULL)
		ll->head = ll->tail = node;
	else {
		ll->tail->next = node;
		node->prev = ll->tail;
		ll->tail = node;
	}
}

void ll_print(struct ll *ll) {
	struct ll_node *node = ll->head;
	while(node)
		printf("%d ", node->val), node = node->next;
	printf("\n");
}

void ll_move_to_front(struct ll *ll, struct ll_node *node) {
	if(ll->head == NULL)
		return;
	if(node == ll->head)
		return;
	if(node == ll->tail && node->prev)
		ll->tail = node->prev;

	if(node->prev)
		node->prev->next = node->next;
	if(node->next)
		node->next->prev = node->prev;
	node->next = ll->head;
	node->next->prev = node;
	node->prev = NULL;
	ll->head = node;
}

void ll_delete(struct ll *ll) {
	struct ll_node *node = ll->tail, *tmp;
	while(node)
		tmp = node->prev, free(node), node = tmp;
}
