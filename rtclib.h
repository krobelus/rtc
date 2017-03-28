/* vim:set tw=80 ts=8 sw=8 sts=8 cc=80 noet cino+=t0: */
#ifndef RTCLIB_H
#define RTCLIB_H

#define SPR 1
#define RAT 2
#define SIZE_ASCENDINGLY 1
#define SIZE_DESCENDINGLY 2
#define SUBSUMPTION 1
#define UNIT_IMPLICATION 2
#define ASCENDINGLY 1
#define MAG_SIGN 2
#define LITERAL_CLAUSE 1
#define CLAUSE_LITERAL 2
#define PTHREADS 1
#define CILK 2
#define LACE 3

#ifndef DEBUG
#define DEBUG false
#endif

#ifndef PROPERTY
#define PROPERTY SPR
#endif

#ifndef ITERATIONS
/* no limit */
#endif

#ifndef REDUNDANCY_CHECK
#define REDUNDANCY_CHECK UNIT_IMPLICATION
#endif

#ifndef ASSIGNMENT_LIMIT
/* no limit */
#endif

#ifndef WATCH_LITERALS
#define WATCH_LITERALS false
#endif

#ifndef SORT_CLAUSES
#define SORT_CLAUSES false
#endif

#ifndef SORT_CLAUSE_LITERALS
#define SORT_CLAUSE_LITERALS false
#endif

#ifndef SORT_VARIABLE_OCCURRENCES
#define SORT_VARIABLE_OCCURRENCES true
#endif

#ifndef STORE_REFUTATIONS
#define STORE_REFUTATIONS true
#endif

#ifndef DETECT_ENVIRONMENT_CHANGE
#define DETECT_ENVIRONMENT_CHANGE true
#endif

#ifndef PARALLEL
#define PARALLEL false
#undef THREADS
#define THREADS 0
#else
#ifndef THREADS
#define THREADS 3
#endif
#endif

#ifndef SMALL_CLAUSE_OPTIMIZATION
#define SMALL_CLAUSE_OPTIMIZATION true
#endif

#ifndef SMALL_CLAUSE_SIZE
#define SMALL_CLAUSE_SIZE 10
#endif

#ifndef LOG_L_SIZES
#define LOG_L_SIZES false
#endif

#ifndef MEMBERSHIP_MATRIX
#define MEMBERSHIP_MATRIX false
#endif

#ifndef DENSE_MEMBERSHIP_MATRIX
#define DENSE_MEMBERSHIP_MATRIX true
#endif

#ifndef RESOLVENT_LITERAL_MAP
#define RESOLVENT_LITERAL_MAP false
#endif

#if MEMBERSHIP_MATRIX || DENSE_MEMBERSHIP_MATRIX && !MEMBERSHIP_MATRIX ||      \
    RAT_ITERATE_VARIABLES && PROPERTY != RAT ||                                \
    REDUNDANCY_CHECK == SUBSUMPTION && ASSIGNMENT_LIMIT
#error ?
#endif

#include "common.h"
#include "hashtable.h"
#include "member.h"

#include <pthread.h>

#define ACTIVE(clause) ((clause)->size)
#define SET_CLAUSE_INACTIVE(clause) clause->size = 0

struct refutation {
	nat d;
	nat length;
	union {
		int small[2];
		int *large;
	} L;
};

struct decision {
	int *L;
#if STORE_REFUTATIONS
	struct refutation *refutations;
	nat refutation_count, refutation_size;
#endif
	nat L_length;
	int redundant;
};

struct clause {
	nat count, size;
	struct decision *decision;
#if SMALL_CLAUSE_OPTIMIZATION
	int *_literals;
	int literals_inline[SMALL_CLAUSE_SIZE];
#define LITERAL(clause, i)                                                     \
	(i < SMALL_CLAUSE_SIZE ? (clause)->literals_inline[i]                  \
			       : (clause)->_literals[i - SMALL_CLAUSE_SIZE])
#else
	int _literals[];
#define LITERAL(clause, i) ((clause)->_literals[i])
#endif
};

struct variable {
	nat count, size;
	char positive_changed, negative_changed;
	/* clause indices */
	int occurrences[];
};

struct watch {
	int i1, i2;
	int i1_l1, i2_l1;
};

#define RUP_LEVELS 3
struct watcher {
	/* assert(0 <= l1_begin <= l2_begin); */
	/* assert(l0_end <= l1_end <= l2_end <= size); */
	nat *clauses[RUP_LEVELS];
	nat counts[RUP_LEVELS], sizes[RUP_LEVELS];
	nat dirty;
};

#if PARALLEL == PTHREADS
struct shared {
	nat c;
};
#endif

struct rtc {
	nat c_count, c_size;
	nat v_count, v_size;
	nat max_clause_count;
	int status;
	FILE *out_formula, *out_proof, *out_log;

#if SMALL_CLAUSE_OPTIMIZATION
	struct clause *clauses;
#define CLAUSE(i) (&rtc->clauses[i])
#else
	struct clause **clauses;
#define CLAUSE(i) (rtc->clauses[i])
#endif
	/* occurrence lists */
	struct variable **variables;

#if MEMBERSHIP_MATRIX
	MEMBER_T **membership_matrix;
#endif
	/* struct { */
	/* 	int iterations, */
	/* 	    clauses_skipped, */
	/* 	    literal_sets_skipped, */
	/* 	    duplicate_d_skipped, */
	/* 	    d_skipped, */
	/* 	    removed_clauses_total_length; */
	/* } stats; */

	pthread_t workers[THREADS];
	struct rtc *clones[THREADS];
#if PARALLEL == PTHREADS
	struct shared *shared;
#endif

	/* THREAD_LOCALS begin */

	/* map for variables, tracks assignment */
	int *variable_map;
	/* assignment stack */
	int *assignments;
	nat assignments_count, assignments_size;

	int *L;

	int *resolvent_literals;
	nat resolvent_count;
	nat c, d;
	bool C_n_D_disjoint;
#if RESOLVENT_LITERAL_MAP
	uint8_t *resolvent_literal_map;
#endif
#if WATCH_LITERALS
	struct watch *watches;
	struct watcher *watchers;
	nat *dirty_watches;
	int *dirty_watchers;
	nat dirty_watches_count, dirty_watchers_count, dirty_watches_size,
	    dirty_watchers_size;
#endif

	/* THREAD_LOCALS end */
};

/*
 * compute the next combination of numbers from 0 .. n-1 where the current
 * length is k
 * returns the length of the next combination (k or k+1)
 * or 0 after the last combination
 *
 * example: n = 3, k = 0
 * 0
 * 1
 * 2
 * 0 1
 * 0 2
 * 1 2
 * 0 1 2
 */
/* nat next_combination(nat *dest, nat n, nat k); */

// r !grep '^[a-z].*{' rtclib.c | sed -e 's/).*/);/'

struct rtc *rtc_new(struct rtc *rtc);
void rtc_abandon(struct rtc *rtc);
void rtc_allocate_thread_locals(struct rtc *rtc);
void rtc_release_thread_locals(struct rtc *rtc);
void rtc_watcher_lift(struct rtc *rtc, nat level, int l);
void rtc_watcher_add(struct rtc *rtc, nat level, int l, nat c);
void rtc_watcher_remove(struct rtc *rtc, nat level, int l, nat *iptr);
void rtc_watcher_dirty(struct rtc *rtc, nat level, int l);
void rtc_watch_dirty(struct rtc *rtc, nat c);
void ckwchs(struct rtc *rtc, nat level);
void ckwchr(struct rtc *rtc, nat level, int l);
void rtc_init(struct rtc *rtc);
int rtc_log(struct rtc *rtc, const char *msg, ...);
void rtc_print(FILE *f, struct rtc *rtc);
void clause_print(FILE *f, struct clause *clause);
void variable_print(FILE *f, struct variable *variable);
void watcher_print(struct watcher *w);
void rtc_add(struct rtc *rtc, int literal);
void rtc_build_occurrence_lists(struct rtc *rtc);
void rtc_add_occurrence(struct rtc *rtc, int literal, int clause);
void rtc_preprocess(struct rtc *rtc);
void rtc_age_change_flags(struct rtc *rtc);
void *rtc_pthreads_worker(void *_rtc);
void rtc_cilk_worker(struct rtc *rtc, nat c);
void rtc_preprocess_parallel(struct rtc *rtc);
void rtc_remove_all_positive_clauses(struct rtc *rtc);
bool rtc_clause_is_spr(struct rtc *rtc, nat c);
bool rtc_rup(struct rtc *rtc);
void rtc_reset(struct rtc *rtc, nat level, nat ac, nat wc, nat wrc);
void rtc_assign(struct rtc *rtc, int l);
bool rtc_propagate(struct rtc *rtc, nat level);
int madd(int a, int b, nat m);
bool rtc_imply(struct rtc *rtc, nat level, int l);
bool rtc_imply_visit(struct rtc *rtc, nat level, int l, nat lvl, nat *iptr);
bool rtc_spr(struct rtc *rtc);
void rtc_clause_add_refutation(struct rtc *rtc, nat c, int *L, nat length);
bool rtc_clause_verify_refutations(struct rtc *rtc, nat c);
bool rtc_clause_is_spr_on_L(struct rtc *rtc, int *L, nat length);
void rtc_resolvent_add(struct rtc *rtc, int l);
bool rtc_resolvent_contains(struct rtc *rtc, int l);
void rtc_resolvent_c_add(struct rtc *rtc);
bool rtc_resolvent_d_add(struct rtc *rtc, nat d, int *L, nat length);
void rtc_resolvent_d_remove(struct rtc *rtc, struct clause *C);
void rtc_resolvent_c_remove(struct rtc *rtc);
bool rtc_clauses_disjoint(struct rtc *rtc, nat c, nat d);
bool rtc_implies_resolvent(struct rtc *rtc, struct clause *clause);
bool rtc_resolvent_is_implied(struct rtc *rtc);
bool rtc_resolvent_is_implied_i(struct rtc *rtc);
bool rtc_resolvent_is_subsumed_i(struct rtc *rtc);
void rtc_remove_clause(struct rtc *rtc, nat c);
int compare_mag_sign(const void *_a, const void *_b);
int compare(const void *_a, const void *_b);
int rtc_compare_occurrences(const void *_a, const void *_b, void *_rtc);
int rtc_compare_clauses_by_count(const void *_a, const void *_b);
int rtc_compare_clauses_by_count_reverse(const void *_a, const void *_b);
bool member(int lit, struct clause *C);
nat next_combination(nat *dest, nat n, nat k);

#endif /* RTCLIB_H */
