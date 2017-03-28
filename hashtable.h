/* vim:set tw=80 ts=8 sw=8 sts=8 cc=80 noet cino+=t0: */
#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "common.h"

struct hashtable_entry {
	int literal;
	nat clause;
};

struct hashtable {
	nat count, size;
	nat collisions, accesses;
	struct hashtable_entry *entries;
};

void hashtable_new(struct hashtable *ht, nat size);

nat hashtable_hash(struct hashtable *ht, int lit, nat c);

nat hashtable_next(struct hashtable *ht, nat index, nat i);

bool hashtable_lookup(struct hashtable *ht, int lit, nat c);

void hashtable_put(struct hashtable *ht, int lit, nat c);

void hashtable_delete(struct hashtable *ht);

#endif /* HASHTABLE_H */
