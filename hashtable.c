/* vim:set tw=80 ts=8 sw=8 sts=8 cc=80 noet cino+=t0: */

#include "hashtable.h"

void hashtable_new(struct hashtable *ht, nat size) {
	ht->count = 0;
	ht->size = size * 64;
	ht->accesses = ht->collisions = 0;
	ht->entries = calloc(ht->size, sizeof(ht->entries[0]));
}

nat hashtable_hash(struct hashtable *ht, int lit, nat c) {
	// http://stackoverflow.com/questions/682438/hash-function-providing-unique-nat-from-an-integer-coordinate-pair
	unsigned hash;
	hash = lit + c * 0x9E3779B9;
	return hash % ht->size;
}

nat hashtable_next(struct hashtable *ht, nat index, nat i) {
	ht->collisions++;
	nat hash;
	hash = index + ((i + 1) / 2) * (i % 2 ? 1 : -1);
	return hash % ht->size;
}

bool hashtable_lookup(struct hashtable *ht, int lit, nat c) {
	ht->accesses++;
	nat base = hashtable_hash(ht, lit, c);
	nat index = base;
	nat i = 0;
	while(1) {
		struct hashtable_entry entry = ht->entries[index];
		if(!entry.literal)
			return false;
		if(entry.literal == lit && entry.clause == c)
			return true;
		index = hashtable_next(ht, index, i++);
	}
}

void hashtable_put(struct hashtable *ht, int lit, nat c) {
	ht->accesses++;
	nat base = hashtable_hash(ht, lit, c);
	nat index = base;
	nat i = 0;
	while(1) {
		struct hashtable_entry *entry = &ht->entries[index];
		if(!entry->literal) {
			entry->literal = lit;
			entry->clause = c;
			ht->count++;
			return;
		}
		index = hashtable_next(ht, index, i++);
	}
}

void hashtable_delete(struct hashtable *ht) { free(ht->entries); }
