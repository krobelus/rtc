/* vim:set tw=80 ts=8 sw=8 sts=8 cc=80 noet cino+=t0: */

#include "rtclib.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

/* static uint64_t l_can_be_removed[2]; */
#if LOG_L_SIZES
#define L_sizes_max 5
static uint64_t L_sizes[L_sizes_max];
#endif

struct rtc *rtc_new(struct rtc *rtc) {
	rtc->variables = calloc(rtc->v_size, sizeof(rtc->variables[0]));
	rtc->clauses = calloc(rtc->c_size, sizeof(rtc->clauses[0]));
#if PARALLEL == PTHREADS
	rtc->shared = malloc(sizeof(rtc->shared[0]));
#endif
#if MEMBERSHIP_MATRIX
	rtc->membership_matrix = malloc(DIM1 * sizeof(MEMBER_T *)) +
				 DIM1_OFFSET * sizeof(MEMBER_T *);
	for(nat i = 0; i < DIM1; i++)
		(rtc->membership_matrix - DIM1_OFFSET)[i] =
		    calloc(sizeof(MEMBER_T), DIM2) +
		    DIM2_OFFSET * sizeof(MEMBER_T);
#endif
	return rtc;
}

void rtc_abandon(struct rtc *rtc) {
	rtc_release_thread_locals(rtc);
	for(int i = 0; i < THREADS; i++) {
		rtc_release_thread_locals(rtc->clones[i]);
		free(rtc->clones[i]);
	}
	for(nat i = 1; i < rtc->v_size; i++) {
		struct variable *var = rtc->variables[i];
		if(var)
			free(var);
	}
	free(rtc->variables);
	for(nat i = 1; i < rtc->c_size; i++) {
		struct clause *clause = CLAUSE(i);
		struct decision *dc = clause->decision;
#if STORE_REFUTATIONS
		for(nat i = 0; i < dc->refutation_count; i++)
			if(dc->refutations[i].length > 2)
				free(dc->refutations[i].L.large);
		free(dc->refutations);
#endif
		free(dc->L);
		free(dc);
#if SMALL_CLAUSE_OPTIMIZATION
		free(clause->_literals);
#else
		free(clause);
#endif
	}
	free(rtc->clauses);
#if MEMBERSHIP_MATRIX
	for(nat i = 0; i < DIM1; i++)
		free((rtc->membership_matrix - DIM1_OFFSET)[i] - DIM2_OFFSET);
	free(rtc->membership_matrix - DIM1_OFFSET);
#endif
	if(rtc->out_formula)
		fclose(rtc->out_formula);
	if(rtc->out_proof)
		fclose(rtc->out_proof);
	if(rtc->out_log)
		fclose(rtc->out_log);
#if PARALLEL == PTHREADS
	free(rtc->shared);
#endif
	free(rtc);
}
bool rtc_C_n_D_subseteq_not_L(struct rtc *rtc, nat c, nat d, int *L, nat length)
{
	(void)length;
	assert(length == 1);
	struct clause *D = CLAUSE(d);
	int notl = L[0];
	for(nat i = 0; i < D->count; i++) {
		int lit = LITERAL(D, i);
		if(MEMBER(lit, c) && lit != notl)
			return false;
	}
	return true;
}

void rtc_allocate_thread_locals(struct rtc *rtc) {
	rtc->variable_map = calloc(rtc->v_size, sizeof(rtc->variable_map[0]));
	rtc->assignments_size = rtc->v_size / 2;
	rtc->assignments =
	    malloc(rtc->assignments_size * sizeof(rtc->assignments[0]));
	rtc->L = malloc(rtc->max_clause_count * sizeof(rtc->L[0]));
	nat resolvent_size =
	    3 * (rtc->max_clause_count + 5); // FIXME what is the minimum
	rtc->resolvent_literals =
	    malloc(resolvent_size * sizeof(rtc->resolvent_literals));
	rtc->resolvent_count = 0;
#if RESOLVENT_LITERAL_MAP
	nat size = sizeof(rtc->resolvent_literal_map[0]);
	rtc->resolvent_literal_map =
	    calloc(2 * rtc->v_size, size) + rtc->v_size * size;
#endif
#if WATCH_LITERALS
	rtc->watches = malloc(rtc->c_size * sizeof(rtc->watches[0]));
	rtc->watchers = malloc(2 * rtc->v_size * sizeof(rtc->watchers[0])) +
			rtc->v_size * sizeof(rtc->watchers[0]);
	for(int i = 1; i < (int)rtc->v_size; i++) {
		if(!rtc->variables[abs(i)])
			continue;
		for(int polarity = -1; polarity < 2; polarity += 2) {
			struct watcher *watcher = &rtc->watchers[i * polarity];
			watcher->counts[0] = 0;
			watcher->sizes[0] = 2;
			watcher->clauses[0] = malloc(
			    watcher->sizes[0] * sizeof(watcher->clauses[0][0]));
			for(nat i = 1; i < RUP_LEVELS; i++) {
				watcher->counts[i] = 0;
				watcher->sizes[i] = 0;
				watcher->clauses[i] = NULL;
			}
			watcher->dirty = 0;
		}
	}
	for(nat c = 1; c < rtc->c_size; c++) {
		struct clause *C = CLAUSE(c);
		struct watch *watch = &rtc->watches[c];
		watch->i1 = 0;
		watch->i1_l1 = 0;
		watch->i2 = 1;
		watch->i2_l1 = 1;
		rtc_watcher_add(rtc, 0, LITERAL(C, 0), c);
		rtc_watcher_add(rtc, 0, LITERAL(C, 1), c);
	}
	rtc->dirty_watches_count = 0;
	rtc->dirty_watches_size = rtc->c_size;
	rtc->dirty_watchers_count = 0;
	rtc->dirty_watchers_size = 2 * rtc->v_size;
	rtc->dirty_watches =
	    malloc(rtc->dirty_watches_size * sizeof(rtc->dirty_watches[0]));
	rtc->dirty_watchers =
	    malloc(rtc->dirty_watchers_size * sizeof(rtc->dirty_watchers[0]));
#endif
}

void rtc_release_thread_locals(struct rtc *rtc) {
	free(rtc->variable_map);
	free(rtc->assignments);
	free(rtc->L);
	free(rtc->resolvent_literals);
#if RESOLVENT_LITERAL_MAP
	free(rtc->resolvent_literal_map - rtc->v_size);
#endif
#if WATCH_LITERALS
	for(int i = 1; i < (int)rtc->v_size; i++) {
		struct variable *var = rtc->variables[abs(i)];
		if(!var)
			continue;
		for(nat lvl = 0; lvl < RUP_LEVELS; lvl++) {
			free(rtc->watchers[i].clauses[lvl]);
			free(rtc->watchers[-i].clauses[lvl]);
		}
	}
	free(rtc->watches);
	free(rtc->watchers - rtc->v_size);
	free(rtc->dirty_watches);
	free(rtc->dirty_watchers);
#endif
}

#if WATCH_LITERALS
void rtc_watcher_lift(struct rtc *rtc, nat level, int l) {
	rtc_watcher_dirty(rtc, level, l);
	struct watcher *watcher = &rtc->watchers[l];
	assert(level == 0 || level == 1 || level == 2);
	if(!watcher->clauses[level]) {
		nat src = (level == 2 && watcher->clauses[1]) ? 1 : 0;
		watcher->counts[level] = watcher->counts[src];
		watcher->sizes[level] = watcher->sizes[src];
		watcher->clauses[level] = malloc(
		    watcher->sizes[src] * sizeof(watcher->clauses[level][0]));
		memcpy(watcher->clauses[level], watcher->clauses[src],
		       watcher->sizes[level] *
			   sizeof(watcher->clauses[level][0]));
	}
}
void rtc_watcher_add(struct rtc *rtc, nat level, int l, nat c) {
	struct watcher *watcher = &rtc->watchers[l];
	rtc_watcher_lift(rtc, level, l);
	if(watcher->counts[level] == watcher->sizes[level]) {
		watcher->sizes[level] *= 2;
		watcher->clauses[level] = realloc(
		    watcher->clauses[level],
		    watcher->sizes[level] * sizeof(watcher->clauses[level][0]));
	}
	watcher->clauses[level][watcher->counts[level]++] = c;
}

void rtc_watcher_remove(struct rtc *rtc, nat level, int l, nat *iptr) {
	struct watcher *watcher = &rtc->watchers[l];
	rtc_watcher_lift(rtc, level, l);
	nat i = *iptr;
	nat count = watcher->counts[level];
	assert(count);
	if(count - 1 != i) {
		watcher->clauses[level][i] = watcher->clauses[level][count - 1];
	}
	(*iptr)--;
	watcher->counts[level]--;
}

void rtc_watcher_dirty(struct rtc *rtc, nat level, int l) {
	struct watcher *watcher = &rtc->watchers[l];
	if(!level || watcher->dirty & (1 << level))
		return;
	watcher->dirty |= 1 << level;
	nat i = rtc->dirty_watchers_count;
	if(i == rtc->dirty_watchers_size) {
		rtc->dirty_watchers_size *= 2;
		rtc->dirty_watchers = realloc(
		    rtc->dirty_watchers,
		    rtc->dirty_watchers_size * sizeof(rtc->dirty_watchers[0]));
	}
	rtc->dirty_watchers[i] = l;
	rtc->dirty_watchers_count++;
}

void rtc_watch_dirty(struct rtc *rtc, nat c) {
	nat i = rtc->dirty_watches_count;
	if(i == rtc->dirty_watches_size) {
		rtc->dirty_watches_size *= 2;
		rtc->dirty_watches = realloc(rtc->dirty_watches,
					     rtc->dirty_watches_size *
						 sizeof(rtc->dirty_watches[0]));
	}
	rtc->dirty_watches[i] = c;
	rtc->dirty_watches_count++;
}

void ckwchs(struct rtc *rtc, nat level) {
	for(nat c = 1; c < rtc->c_size; c++) {
#ifndef NDEBUG
		struct watch *watch = &rtc->watches[c];
		assert(watch->i1 != watch->i2);
#endif
	}
	for(int i = 1; i < (int)rtc->v_size; i++) {
		ckwchr(rtc, level, i);
		ckwchr(rtc, level, -i);
	}
}

void ckwchr(struct rtc *rtc, nat level, int l) {
#ifndef NDEBUG
	struct variable *var = rtc->variables[abs(l)];
	if(!var)
		return;
	struct watcher *w = &rtc->watchers[l];
	assert(level == 1 || level == 2);
	nat lvl = level;
	while(lvl && !w->clauses[lvl])
		lvl--;
	for(nat i = 0; i < w->counts[lvl]; i++) {
		nat c = w->clauses[lvl][i];
		struct clause *C = CLAUSE(c);
		struct watch *watch = &rtc->watches[c];
		assert(LITERAL(C, watch->i1) == l ||
		       LITERAL(C, watch->i2) == l);
	}
#else
	(void)rtc;
	(void)level;
	(void)l;
#endif
}
#endif

void rtc_init(struct rtc *rtc) {
#if SORT_CLAUSES
	qsort(rtc->clauses + 1, rtc->c_size - 1, sizeof(rtc->clauses[0]),
#if SORT_CLAUSES == SIZE_ASCENDINGLY
	      rtc_compare_clauses_by_count
#elif SORT_CLAUSES == SIZE_DESCENDINGLY
	      rtc_compare_clauses_by_count_reverse
#else
#error ?
#endif
	      );
#endif
#if SORT_CLAUSE_LITERALS
	for(nat i = 1; i < rtc->c_size; i++) {
		struct clause *C = CLAUSE(i);
		qsort(C->_literals, C->count, sizeof(C->_literals[0]),
#if SORT_CLAUSE_LITERALS == ASCENDINGLY
		      compare
#elif SORT_CLAUSE_LITERALS == MAG_SIGN
		      compare_mag_sign
#else
#error ?
#endif
		      );
#if SMALL_CLAUSE_OPTIMIZATION
		memcpy(C->literals_inline, C->_literals,
		       MIN(SMALL_CLAUSE_SIZE, C->size) *
			   sizeof(C->_literals[0]));
#endif
	}
#endif
#if SMALL_CLAUSE_OPTIMIZATION
	for(nat i = 1; i < rtc->c_size; i++) {
		struct clause *C = CLAUSE(i);
		for(nat i = SMALL_CLAUSE_SIZE; i < C->count; i++)
			C->_literals[i - SMALL_CLAUSE_SIZE] = C->_literals[i];
		/* C->_literals += SMALL_CLAUSE_SIZE; */
	}
#endif
	rtc_build_occurrence_lists(rtc);
#if SORT_VARIABLE_OCCURRENCES
	for(nat i = 1; i < rtc->v_size; i++) {
		struct variable *var = rtc->variables[i];
		if(!var)
			continue;
		qsort_r(var->occurrences, var->count,
			sizeof(var->occurrences[0]), rtc_compare_occurrences,
			rtc);
	}
#endif
#if MEMBERSHIP_MATRIX
	struct clause *clause;
	for(nat i = 1; i < rtc->c_size; i++) {
		clause = CLAUSE(i);
		for(nat j = 0; j < clause->count; j++)
			SET_MEMBER(LITERAL(clause, j), i);
	}
#endif
	for(nat i = 1; i < rtc->c_size; i++) {
		struct clause *clause = CLAUSE(i);
		struct decision *dc = clause->decision =
		    malloc(sizeof(clause->decision[0]));
		dc->L = NULL;
		dc->L_length = 0;
		dc->redundant = 0;
#if STORE_REFUTATIONS
		dc->refutation_count = 0;
		dc->refutation_size = 2 * clause->count;
		dc->refutations =
		    malloc(dc->refutation_size * sizeof(dc->refutations[0]));
		for(nat i = 0; i < dc->refutation_size; i++)
			dc->refutations[i].L.large = NULL;
#endif
	}
	rtc_allocate_thread_locals(rtc);
	for(int i = 0; i < THREADS; i++) {
		nat size = sizeof(struct rtc);
		rtc->clones[i] = malloc(size);
		memcpy(rtc->clones[i], rtc, size);
		rtc_allocate_thread_locals(rtc->clones[i]);
	}
}

int rtc_log(struct rtc *rtc, const char *msg, ...) {
	va_list ap;
	va_start(ap, msg);
	fprintf(rtc->out_log, ">> ");
	vfprintf(rtc->out_log, msg, ap);
	fprintf(rtc->out_log, "\n");
	va_end(ap);
	return 0;
}

void rtc_print(FILE *f, struct rtc *rtc) {
	fprintf(f, "p cnf %u %u\n", rtc->v_size - 1, rtc->c_count - 1);
	for(nat i = 1; i < rtc->c_size; i++)
		if(ACTIVE(CLAUSE(i)))
			clause_print(f, CLAUSE(i));
}

void clause_print(FILE *f, struct clause *clause) {
	if(!clause) {
		fprintf(f, "\n");
	} else {
		for(nat i = 0; i < clause->count; i++)
			fprintf(f, "%d ", LITERAL(clause, i));
		fprintf(f, "0\n");
	}
}

void variable_print(FILE *f, struct variable *variable) {
	if(!variable) {
		fprintf(f, "\n");
	} else {
		for(nat i = 0; i < variable->count; i++)
			fprintf(f, "%d ", variable->occurrences[i]);
		fprintf(f, "%u/%u\n", variable->count, variable->size);
	}
}

void watcher_print(struct watcher *w) {
	printf("watcher, dirty %d\n", w->dirty);
	for(nat lvl = 0; lvl < RUP_LEVELS; lvl++) {
		printf("level %u count %u size %u clauses", lvl, w->counts[lvl],
		       w->sizes[lvl]);
		if(w->clauses[lvl])
			for(nat i = 0; i < w->counts[lvl]; i++)
				printf(" %d", w->clauses[lvl][i]);
		printf("\n");
	}
}

/* void */
/* rtc_stats(struct rtc *rtc) */
/* { */
/* #define l(name) rtc_log(rtc, #name ": %d", rtc->stats.name); */
/* 	l(iterations); */
/* 	l(clauses_skipped); */
/* 	l(literal_sets_skipped); */
/* 	l(duplicate_d_skipped); */
/* 	l(d_skipped); */
/* 	rtc_log(rtc, "average length of removed clauses: %f", */
/* 			(double)rtc->stats.removed_clauses_total_length */
/* 			/ (rtc->c_size - rtc->c_count)); */
/* #undef l */
/* } */

void rtc_add(struct rtc *rtc, int literal) {
	if(!literal) {
		rtc->max_clause_count =
		    MAX(rtc->max_clause_count, CLAUSE(rtc->c_count)->count);
		rtc->c_count++;
		return;
	}
	assert(rtc->c_count < rtc->c_size);
	struct clause *clause = CLAUSE(rtc->c_count);
#if SMALL_CLAUSE_OPTIMIZATION
	if(!clause->_literals) {
		clause->size += 4;
		clause->_literals =
		    malloc(clause->size * sizeof(clause->_literals[0]));
	} else if(clause->count == clause->size) {
		clause->size += 4;
		clause->_literals =
		    realloc(clause->_literals,
			    clause->size * sizeof(clause->_literals[0]));
	}
	if(clause->count < SMALL_CLAUSE_SIZE)
		clause->literals_inline[clause->count] = literal;
#else
	nat size;
	if(!clause) {
		size = sizeof(struct clause) + 4 * sizeof(clause->_literals[0]);
		clause = CLAUSE(rtc->c_count) = calloc(1, size);
		clause->size = 4;
	} else if(clause->count == clause->size) {
		clause->size += 4;
		size = sizeof(struct clause) +
		       clause->size * sizeof(clause->_literals[0]);
		clause = CLAUSE(rtc->c_count) = realloc(clause, size);
	}
#endif
	clause->_literals[clause->count++] = literal;
}

void rtc_build_occurrence_lists(struct rtc *rtc) {
	for(nat i = 1; i < rtc->c_size; i++) {
		struct clause *clause = CLAUSE(i);
		for(nat j = 0; j < clause->count; j++)
			rtc_add_occurrence(rtc, LITERAL(clause, j), i);
	}
}

void rtc_add_occurrence(struct rtc *rtc, int literal, int clause) {
	nat v = abs(literal);
	struct variable *var = rtc->variables[v];
	if(!var) {
		rtc->v_count++;
		rtc->variables[v] =
		    calloc(1, sizeof(struct variable) +
				  1 * sizeof(var->occurrences[0]));
		var = rtc->variables[v];
		var->count = 0;
		var->size = 1;
		var->positive_changed = 'i';
		var->negative_changed = 'i';
	} else if(var->count == var->size) {
		var->size = (var->size + 2) * 2;
		nat size = sizeof(struct variable) +
			   var->size * sizeof(var->occurrences[0]);
		rtc->variables[v] = realloc(rtc->variables[v], size);
		var = rtc->variables[v];
	}
	var->occurrences[var->count++] = SIGN(literal) * clause;
}

void rtc_preprocess(struct rtc *rtc) {
	nat c_count;
	nat iterations = 0;
	do {
		c_count = rtc->c_count;
#if PARALLEL
		rtc_preprocess_parallel(rtc);
#else
		for(nat c = 1; c < rtc->c_size; c++) {
			if(rtc_clause_is_spr(rtc, c))
				rtc_remove_clause(rtc, c);
		}
#endif
#if DETECT_ENVIRONMENT_CHANGE
		rtc_age_change_flags(rtc);
#endif
		/* rtc->stats.iterations++; */
		iterations++;
		/* printf("disjoint: %d\n", C_n_D_not_disjoint); */
	} while(c_count != rtc->c_count
#if ITERATIONS
		&& iterations < ITERATIONS
#endif
		);
	rtc_log(rtc, "iterations: %d", iterations);
	/* printf("l_can_be_removed=%lu\n", l_can_be_removed[true]); */
	/* printf("l_cannot_be_removed=%lu\n", l_can_be_removed[false]); */
#if LOG_L_SIZES
	for(nat i = 1; i < L_sizes_max; i++)
		printf("l_size_%d=%lu\n", i, L_sizes[i]);
#endif
}

#if DETECT_ENVIRONMENT_CHANGE
void rtc_age_change_flags(struct rtc *rtc) {
	for(nat i = 1; i < rtc->v_size; i++) {
		struct variable *var = rtc->variables[i];
		if(!var)
			continue;
		var->positive_changed =
		    var->positive_changed == 'n' ? 'o' : '\0';
		var->negative_changed =
		    var->negative_changed == 'n' ? 'o' : '\0';
	}
}
#endif

#if PARALLEL
#if PARALLEL == PTHREADS
void *rtc_pthreads_worker(void *_rtc) {
	struct rtc *rtc = _rtc;
	struct shared *s = rtc->shared;
	nat c;
	while((c = __sync_fetch_and_add(&s->c, 1)) < rtc->c_size)
		(void)rtc_clause_is_spr(rtc, c);
	pthread_exit(NULL);
}
#elif PARALLEL == LACE
#include "lace.c"
VOID_TASK_2(rtc_lace_worker, struct rtc *, rtc, nat, c) {
	int16_t id = lace_get_worker()->worker;
	(void)rtc_clause_is_spr(rtc->clones[id], c);
}
#elif PARALLEL == CILK
#include <cilk/cilk_api.h>
void rtc_cilk_worker(struct rtc *rtc, nat c) {
	int id = __cilkrts_get_worker_number();
	(void)rtc_clause_is_spr(rtc->clones[id], c);
}
#endif
void rtc_preprocess_parallel(struct rtc *rtc) {
#if PARALLEL == PTHREADS
	struct shared *s = rtc->shared;
	s->c = 1;
	for(int i = 0; i < THREADS; i++)
		pthread_create(&rtc->workers[i], NULL, rtc_pthreads_worker,
			       rtc->clones[i]);
	for(int i = 0; i < THREADS; i++)
		pthread_join(rtc->workers[i], NULL);
#elif PARALLEL == CILK
	for(nat c = 1; c < rtc->c_size; c++)
		rtc_cilk_worker(rtc, c);
#elif PARALLEL == LACE
	lace_init(0, 2 * rtc->c_size);
	lace_startup(0, NULL, NULL);
	LACE_ME;
	for(nat c = 1; c < rtc->c_size - 1; c++)
		SPAWN(rtc_lace_worker, rtc, c);
	CALL(rtc_lace_worker, rtc, rtc->c_size - 1);
	for(nat c = 1; c < rtc->c_size - 1; c++)
		SYNC(rtc_lace_worker);
	lace_exit();
#else
#error ?
#endif
	rtc_remove_all_positive_clauses(rtc);
}

/* bool rtc_check_positive_clause(struct rtc *rtc, nat c) { */
/* 	bool redundant; */
/* 	struct clause *C = CLAUSE(c); */
/* 	struct decision *dc = C->decision; */
/* 	rtc->c = c; */
/* 	redundant = rtc_rup(rtc); */
/* 	if(!redundant) { */
/* 		rtc_resolvent_c_add(rtc); */
/* 		redundant = rtc_clause_is_spr_on_L(rtc, dc->L, dc->L_length); */
/* 		rtc_resolvent_c_remove(rtc); */
/* 	} */
/* 	dc->redundant = redundant ? 1 : -1; */
/* 	rtc_reset(rtc, 1, 0, 0, 0); */
/* 	return redundant; */
/* } */

void rtc_remove_all_positive_clauses(struct rtc *rtc) {
	for(nat c = 1; c < rtc->c_size; c++) {
		struct clause *C = CLAUSE(c);
		struct decision *dc = C->decision;
		if(dc->redundant > 0 && ACTIVE(C)
				&& rtc_clause_is_spr(rtc, c)
				/* && rtc_check_positive_clause(rtc, c) */
				)
			rtc_remove_clause(rtc, c);
	}
}
#endif

bool rtc_clause_is_spr(struct rtc *rtc, nat c) {
	struct clause *C = CLAUSE(c);
	struct decision *dc = C->decision;
	if(!ACTIVE(C))
		return false;
#if DETECT_ENVIRONMENT_CHANGE
	if(dc->redundant < 0) {
		bool changed = false;
		for(nat i = 0; !changed && i < C->count; i++) {
			int lit = LITERAL(C, i);
			struct variable *var = rtc->variables[abs(lit)];
			if((lit > 0 && var->negative_changed) ||
			   (lit < 0 && var->positive_changed))
				changed = true;
		}
		if(!changed) {
			/* rtc->stats.clauses_skipped++; */
			return false;
		}
	}
#endif
	rtc->c = c;
	bool redundant = false;
#if STORE_REFUTATIONS
	if(dc->redundant < 0 && rtc_clause_verify_refutations(rtc, c)) {
		redundant = false;
	} else {
		redundant = rtc_rup(rtc) || rtc_spr(rtc);
	}
#else
	redundant = rtc_rup(rtc) || rtc_spr(rtc);
#endif
	rtc_reset(rtc, 1, 0, 0, 0);
#if !defined(NDEBUG) && WATCH_LITERALS
	for(int l = 1; l < (int)rtc->v_size; l++) {
		if(!rtc->variables[abs(l)])
			continue;
		for(int p = -1; p < 2; p += 2) {
			struct watcher *watcher = &rtc->watchers[p * l];
			assert(!watcher->clauses[1]);
			assert(!watcher->clauses[2]);
		}
	}
#endif
	dc->redundant = redundant ? 1 : -1;
	return redundant;
}

bool rtc_rup(struct rtc *rtc) {
	nat c = rtc->c;
	assert(rtc->assignments_count == 0);
	for(nat i = 1; i < rtc->v_size; i++) {
		assert(!rtc->variable_map[i]);
	}
	struct clause *C = CLAUSE(c);
	for(nat i = 0; i < C->count; i++) {
		int l = LITERAL(C, i);
		rtc_assign(rtc, -l);
	}
	return rtc_propagate(rtc, 1);
}

void rtc_reset(struct rtc *rtc, nat level, nat ac, nat wc, nat wrc) {
	assert(level == 1 || level == 2);
	{
		for(nat i = ac; i < rtc->assignments_count; i++) {
			int l = rtc->assignments[i];
			rtc->variable_map[abs(l)] = 0;
		}
		rtc->assignments_count = ac;
	}
#if WATCH_LITERALS
	{
		for(nat i = wc; i < rtc->dirty_watches_count; i++) {
			nat c = rtc->dirty_watches[i];
			struct watch *watch = &rtc->watches[c];
			if(level == 1) {
				watch->i1 = 0;
				watch->i2 = 1;
				watch->i1_l1 = 0;
				watch->i2_l1 = 1;
			} else {
				assert(level == 2);
				watch->i1 = watch->i1_l1;
				watch->i2 = watch->i2_l1;
				assert(watch->i1_l1 != watch->i2_l1);
				assert(watch->i1 != watch->i2);
			}
		}
		rtc->dirty_watches_count = wc;
	}
	{
		for(nat i = wrc; i < rtc->dirty_watchers_count; i++) {
			int l = rtc->dirty_watchers[i];
			struct watcher *watcher = &rtc->watchers[l];
			watcher->dirty &= ~(1 << level);
			for(nat lvl = level; lvl < RUP_LEVELS; lvl++) {
				free(watcher->clauses[lvl]);
				watcher->clauses[lvl] = NULL;
			}
		}
		rtc->dirty_watchers_count = wrc;
	}
#else
	(void)level;
	(void)wc;
	(void)wrc;
#endif
}

void rtc_assign(struct rtc *rtc, int l) {
	assert(!rtc->variable_map[abs(l)]);
	nat c = rtc->assignments_count;
#if ASSIGNMENT_LIMIT
	if(c == ASSIGNMENT_LIMIT)
		return;
#endif
	if(c == rtc->assignments_size) {
		rtc->assignments_size *= 2;
		rtc->assignments =
		    realloc(rtc->assignments, rtc->assignments_size *
						  sizeof(rtc->assignments[0]));
	}
	rtc->variable_map[abs(l)] = l;
	rtc->assignments[c] = l;
	rtc->assignments_count++;
}

bool rtc_propagate(struct rtc *rtc, nat level) {
	for(nat i = 0; i < rtc->assignments_count; i++) {
		int l = rtc->assignments[i];
		if(!rtc_imply(rtc, level, l))
			return true;
	}
	return false;
}

int madd(int a, int b, nat m) {
	/* assert(b == 1 || b == -1); */
	/* assert(a >= 0 && a < (int)m); */
	int x = a + b;
	if(x == -1)
		return (int)m - 1;
	else if(x == (int)m)
		return 0;
	else
		return x;
}

bool rtc_imply(struct rtc *rtc, nat level, int l) {
	assert(rtc->variable_map[abs(l)] == l);
#if WATCH_LITERALS
	struct watcher *watcher = &rtc->watchers[-l];
	assert(level == 1 || level == 2);
	rtc_watcher_lift(rtc, level, -l);
	for(nat i = 0; i < watcher->counts[level]; i++) {
		if(!rtc_imply_visit(rtc, level, -l, level, &i))
			return false;
	}
	return true;
#else
	(void)level;
	struct variable *var = rtc->variables[abs(l)];
	for(nat o = 0; o < var->count; o++) {
		int occurrence = var->occurrences[o];
		nat c = abs(occurrence);
		struct clause *C = CLAUSE(c);
		if(SIGN(occurrence) == SIGN(l) || !ACTIVE(C) || c == rtc->c)
			continue;
		nat left = C->count;
		int unit = 0;
		bool tautology = false;
		for(nat i = 0; i < C->count; i++) {
			int lit = LITERAL(C, i);
			int assignment = rtc->variable_map[abs(lit)];
			if(assignment == lit) {
				tautology = true;
				break;
			} else if(assignment == -lit)
				left--;
			else // if(unit == 0)
				unit = lit;
		}
		if(tautology)
			continue;
		if(left == 0)
			return false;
		if(left == 1)
			rtc_assign(rtc, unit);
	}
	return true;
#endif
}

#if WATCH_LITERALS
bool rtc_imply_visit(struct rtc *rtc, nat level, int l, nat lvl, nat *iptr) {
	nat i = *iptr;
	struct watcher *watcher = &rtc->watchers[l];
	nat c = watcher->clauses[lvl][i];
	struct clause *C = CLAUSE(c);
	struct watch *watch = &rtc->watches[c];
	int i1 = watch->i1, i2 = watch->i2;
	int l1 = LITERAL(C, i1), l2 = LITERAL(C, i2);
	int a, b, la, *aptr, *aptr_l1, delta, assignment;
	assert(l1 != l2);
	assert(l == l1 || l == l2);
	if(!ACTIVE(C) || c == rtc->c)
		return true;
	if(l == l1)
		a = i1, b = i2, la = l1, delta = -1, aptr = &watch->i1,
		aptr_l1 = &watch->i1_l1;
	else
		a = i2, b = i1, la = l2, delta = 1, aptr = &watch->i2,
		aptr_l1 = &watch->i2_l1;
	assert(a != b);
	assert(la);
	do {
		a = madd(a, delta, C->count);
		la = LITERAL(C, a);
		assignment = rtc->variable_map[abs(la)];
		assert(la);
	} while(a != b && assignment == -la);
	if(assignment == la) {
		/* tautology */
		return true;
	}
	assert(la);
	if(a == b) {
		if(assignment) {
			/* conflict */
			assert(assignment == -la);
			return false;
		} else {
			/* unit clause */
			rtc_assign(rtc, la);
		}
	} else {
		rtc_watcher_remove(rtc, level, l, iptr);

		rtc_watcher_add(rtc, level, la, c);

		rtc_watch_dirty(rtc, c);
		*aptr = a;
		if(level == 1)
			*aptr_l1 = a;
	}
	return true;
}
#endif

bool rtc_spr(struct rtc *rtc) {
	nat c = rtc->c;
	struct clause *C = CLAUSE(c);
	struct decision *dc = C->decision;
	bool redundant = false;
#if STORE_REFUTATIONS
	dc->refutation_count = 0;
#endif
	int *L = rtc->L;
	nat length = 0;
	nat max_length = C->count;
	nat offsets[C->count];
	int literal_pool[C->count];
	for(nat i = 0; i < C->count; i++)
		literal_pool[i] = LITERAL(C, i);
	rtc_resolvent_c_add(rtc);
	while((length = next_combination(offsets, max_length, length))) {
#if PROPERTY == RAT
		if(length > 1)
			break;
#endif
		for(nat i = 0; i < length; i++) {
			int offset = offsets[i];
			L[i] = literal_pool[offset];
		}
#if LOG_L_SIZES
#if PARALLEL
#error ?
#endif
		if(length < L_sizes_max)
			L_sizes[length]++;
#endif
		redundant = rtc_clause_is_spr_on_L(rtc, L, length);
#if PROPERTY == SPR
		int o0 = offsets[0];
#endif
		if(redundant)
			goto done;
#if STORE_REFUTATIONS
		rtc_clause_add_refutation(rtc, c, L, length);
#endif
#if PROPERTY == SPR
		/* __sync_fetch_and_add(&l_can_be_removed[rtc->C_n_D_disjoint], 1); */
		if(rtc->C_n_D_disjoint) {
			/* PRECONDITIONS:
			* 1. the resolvent is not implied
			* 2. C and D share only variables which are in L
			* 3. L is of length 1
			*
			* So, C u (D \ -L) is not implied by any clause
			* Consider another resolvent C u (D \ -L')
			* where L' is a subset of C, like L
			* and L subseteq L'
			* given that L' does not share any variables with D
			* apart from those that are already in L
			* then the two resolvents are identical
			* C u (D \ -L) = C u (D \ -L')
			*
			* therefore the same implicant can be used to refute
			* SPR for literal sets L'
			*
			* so I can skip any literals set L' that contains L
			* since L is of length 1, I can remove it from the
			* literal pool that is used to form L
			*/
			/* remove L[0] from literal_pool */
			if((nat)o0 != max_length - 1)
				literal_pool[o0] = literal_pool[max_length - 1];
			max_length--;
			length = 0;
		}
#endif
	}
done:
	if(redundant) {
		nat size = length * sizeof(dc->L[0]);
		if(!dc->L) {
			dc->L = malloc(size);
		} else if(length > dc->L_length) {
			dc->L = realloc(dc->L, size);
		}
		memcpy(dc->L, L, size);
		dc->L_length = length;
	}
	rtc_resolvent_c_remove(rtc);
	return redundant;
}

#if STORE_REFUTATIONS
void rtc_clause_add_refutation(struct rtc *rtc, nat c, int *L, nat length) {
	struct clause *C = CLAUSE(c);
	struct decision *dc = C->decision;
	if(dc->refutation_count == dc->refutation_size) {
		dc->refutation_size *= 2;
		dc->refutations =
		    realloc(dc->refutations,
			    dc->refutation_size * sizeof(dc->refutations[0]));
		for(nat i = dc->refutation_count; i < dc->refutation_size; i++)
			dc->refutations[i].L.large = NULL;
	}
	struct refutation *r = &dc->refutations[dc->refutation_count];
	r->d = rtc->d;
	r->length = length;
	if(length <= 2)
		memcpy(r->L.small, L, length * sizeof(r->L.small[0]));
	else {
		if(!r->L.large)
			r->L.large = malloc(C->count * sizeof(r->L.large[0]));
		memcpy(r->L.large, L, length * sizeof(r->L.large[0]));
	}
	dc->refutation_count++;
}

bool rtc_clause_verify_refutations(struct rtc *rtc, nat c) {
	struct clause *C = CLAUSE(c);
	struct decision *dc = C->decision;
	bool valid = true;
	valid = !rtc_rup(rtc);
	rtc_resolvent_c_add(rtc);
	for(nat i = 0; valid && i < dc->refutation_count; i++) {
		struct refutation *r = &dc->refutations[i];
		nat d = r->d;
		nat length = r->length;
		int *L = length <= 2 ? r->L.small : r->L.large;
		if(!ACTIVE(CLAUSE(d))) {
			valid = false;
		} else {
			valid = !rtc_resolvent_d_add(rtc, d, L, length);
			assert(valid);
			valid = !rtc_resolvent_is_implied(rtc);
			rtc_resolvent_d_remove(rtc, C);
		}
	}
	rtc_resolvent_c_remove(rtc);
	rtc_reset(rtc, 1, 0, 0, 0);
	return valid;
}
#endif

bool rtc_clause_is_spr_on_L(struct rtc *rtc, int *L, nat length) {
	nat c = rtc->c;
	struct clause *C = CLAUSE(c);
	rtc->C_n_D_disjoint = false;
	rtc->d = 0;
	for(nat i = 0; i < length; i++) {
		struct variable *var = rtc->variables[abs(L[i])];
		if(!var)
			continue;
		for(nat o = 0; o < var->count; o++) {
			int occurrence = var->occurrences[o];
			nat d = abs(occurrence);
			if(SIGN(occurrence) == SIGN(L[i]))
				continue;
			if(!ACTIVE(CLAUSE(d)))
				continue;
			bool redundant = rtc_resolvent_d_add(rtc, d, L, length);
			if(!redundant) {
				redundant = rtc_resolvent_is_implied(rtc);
			}
			rtc_resolvent_d_remove(rtc, C);
			if(!redundant) {
				rtc->d = d;
#if PROPERTY == RAT
				return false;
#else
				if(length > 1)
					return false;
#endif
				if(rtc_clauses_disjoint(rtc, c, d)) {
				/* if(rtc_C_n_D_subseteq_not_L(rtc, c, d, L, length)) { */
					/* prefer to find a D where C n D = {}
					*/
					rtc->C_n_D_disjoint = true;
					return false;
				} else {
					/* rtc->stats.d_skipped++; */
				}
			}
		}
	}
	return rtc->d == 0;
}

void rtc_resolvent_add(struct rtc *rtc, int l) {
#if RESOLVENT_LITERAL_MAP
	rtc->resolvent_literal_map[l]++;
#endif
	rtc->resolvent_literals[rtc->resolvent_count++] = l;
}

bool rtc_resolvent_contains(struct rtc *rtc, int l) {
#if RESOLVENT_LITERAL_MAP
	return rtc->resolvent_literal_map[l];
#else
	for(nat i = 0; i < rtc->resolvent_count; i++)
		if(rtc->resolvent_literals[i] == l)
			return true;
	return false;
#endif
}

void rtc_resolvent_c_add(struct rtc *rtc) {
	struct clause *C = CLAUSE(rtc->c);
	rtc->resolvent_count = 0;
	for(nat i = 0; i < C->count; i++) {
		int lit = LITERAL(C, i);
		rtc_resolvent_add(rtc, lit);
	}
}

bool rtc_resolvent_d_add(struct rtc *rtc, nat d, int *L, nat length) {
	nat i, j;
	struct clause *C = CLAUSE(rtc->c);
	struct clause *D = CLAUSE(d);
	rtc->resolvent_count = C->count;
	for(i = 0; i < D->count; i++) {
		int lit = LITERAL(D, i);
		bool ok = true;
		for(j = 0; j < length; j++) {
			if(lit == L[j])
				return true;
			if(lit == -L[j])
				ok = false;
		}
		if(ok) {
			//?
			if(rtc_resolvent_contains(rtc, -lit))
				return true;
			if(rtc_resolvent_contains(rtc, lit))
				continue;
			rtc_resolvent_add(rtc, lit);
		}
	}
	return false;
}

void rtc_resolvent_d_remove(struct rtc *rtc, struct clause *C) {
#if RESOLVENT_LITERAL_MAP
	for(nat i = C->count; i < rtc->resolvent_count; i++) {
		int lit = rtc->resolvent_literals[i];
		rtc->resolvent_literal_map[lit]--;
	}
#else
	(void)rtc;
	(void)C;
#endif
}

void rtc_resolvent_c_remove(struct rtc *rtc) {
#if RESOLVENT_LITERAL_MAP
	for(nat i = 0; i < rtc->resolvent_count; i++) {
		int lit = rtc->resolvent_literals[i];
		rtc->resolvent_literal_map[lit] = 0;
	}
#else
	(void)rtc;
#endif
}

bool rtc_clauses_disjoint(struct rtc *rtc, nat c, nat d) {
	struct clause *D = CLAUSE(d);
	for(nat i = 0; i < D->count; i++) {
		int lit = LITERAL(D, i);
		if(MEMBER(lit, c))
			return false;
	}
	return true;
}

bool rtc_implies_resolvent(struct rtc *rtc, struct clause *clause) {
	for(nat i = 0; i < clause->count; i++)
		if(!rtc_resolvent_contains(rtc, LITERAL(clause, i)))
			return false;
	return true;
}

bool rtc_resolvent_is_implied(struct rtc *rtc) {
#if REDUNDANCY_CHECK == SUBSUMPTION
	return rtc_resolvent_is_subsumed_i(rtc);
#else /* REDUNDANCY_CHECK == UNIT_IMPLICATION */
	return rtc_resolvent_is_implied_i(rtc);
#endif
}

bool rtc_resolvent_is_implied_i(struct rtc *rtc) {
	bool conflict = false;
	nat ac = rtc->assignments_count;
#if WATCH_LITERALS
	nat wc = rtc->dirty_watches_count, wrc = rtc->dirty_watchers_count;
#endif
	nat c = rtc->c;
	struct clause *C = CLAUSE(c);
	for(nat i = C->count; !conflict && i < rtc->resolvent_count; i++) {
		int l = rtc->resolvent_literals[i];
		int assignment = rtc->variable_map[abs(l)];
		if(assignment == l)
			conflict = true;
		else if(assignment == -l)
			continue;
		else
			rtc_assign(rtc, -l);
	}
	for(nat i = ac; !conflict && i < rtc->assignments_count; i++) {
		int l = rtc->assignments[i];
		int assignment = rtc->variable_map[abs(l)];
		assert(l);
		assert(assignment == l);
		if(assignment == -l || !rtc_imply(rtc, 2, l))
			conflict = true;
	}
#if WATCH_LITERALS
	rtc_reset(rtc, 2, ac, wc, wrc);
#else
	rtc_reset(rtc, 2, ac, 0, 0);
#endif
	return conflict;
}

bool rtc_resolvent_is_subsumed_i(struct rtc *rtc) {
	for(nat li = 0; li < rtc->resolvent_count; li++) {
		int lit = rtc->resolvent_literals[li];
		struct variable *var = rtc->variables[abs(lit)];
		for(nat i = 0; i < var->count; i++) {
			int occurrence = var->occurrences[i];
			if(SIGN(lit) != SIGN(occurrence))
				continue;
			struct clause *implicant = CLAUSE(abs(occurrence));
			if(!ACTIVE(implicant) || (nat)abs(occurrence) == rtc->c)
				continue;
			if(rtc_implies_resolvent(rtc, implicant))
				return abs(occurrence);
		}
	}
	return 0;
}

void rtc_remove_clause(struct rtc *rtc, nat c) {
	nat i, j;
	struct clause *C = CLAUSE(c);
	struct decision *dc = C->decision;
	if(rtc->out_proof) {
		fprintf(rtc->out_proof, "d");
		for(i = 0; i < dc->L_length; i++)
			fprintf(rtc->out_proof, " %d", dc->L[i]);
		for(i = 0; i < C->count; i++) {
			int lit = LITERAL(C, i);
			for(j = 0; j < dc->L_length; j++) {
				if(lit == dc->L[j])
					break;
			}
			if(j == dc->L_length)
				fprintf(rtc->out_proof, " %d", lit);
		}
		fprintf(rtc->out_proof, " 0\n");
	}
#if DETECT_ENVIRONMENT_CHANGE
	/* mark all literals in the clause as changed */
	for(nat i = 0; i < C->count; i++) {
		int lit = LITERAL(C, i);
		struct variable *var = rtc->variables[abs(lit)];
		if(lit > 0)
			var->positive_changed = 'n';
		else
			var->negative_changed = 'n';
	}
#endif
	assert(ACTIVE(C));
	SET_CLAUSE_INACTIVE(C);
	rtc->c_count--;
	/* rtc->stats.removed_clauses_total_length += C->count; */
}

int compare_mag_sign(const void *_a, const void *_b) {
	int a = *(int *)_a, b = *(int *)_b;
	int ma = abs(a) << 1, mb = abs(b) << 1;
	int sa = (unsigned)a >> 31, sb = (unsigned)b >> 31;
	return (ma | sa) - (mb | sb);
}

int compare(const void *_a, const void *_b) {
	int a = *(int *)_a, b = *(int *)_b;
	return a - b;
}

/*
* The occurrences in small clauses should come first.
*/
int rtc_compare_occurrences(const void *_a, const void *_b, void *_rtc) {
	struct rtc *rtc = _rtc;
	struct clause *a = CLAUSE(abs(*(int *)_a));
	struct clause *b = CLAUSE(abs(*(int *)_b));
	return a->count - b->count;
}

int rtc_compare_clauses_by_count(const void *_a, const void *_b) {
	const struct clause *a, *b;
#if SMALL_CLAUSE_OPTIMIZATION
	a = _a, b = _b;
#else
	a = *(struct clause **)_a, b = *(struct clause **)_b;
#endif
	return a->count - b->count;
}

int rtc_compare_clauses_by_count_reverse(const void *_a, const void *_b) {
	return -rtc_compare_clauses_by_count(_a, _b);
}

bool member(int lit, struct clause *C) {
	for(nat i = 0; i < C->count; i++) {
		if(LITERAL(C, i) == lit)
			return true;
	}
	return false;
}

nat next_combination(nat *dest, nat n, nat k) {
	int i;
	/* find the rightmost digit that is not maxed */
	for(i = k - 1; i >= 0; i--)
		if(dest[i] != n + i - k)
			break;
	if(i == -1) {
		/* last combination for length k */
		if(k == n)
			return 0;
		k++;
		/* inital combination - 0 1 2 … */
		for(i = 0; (nat)i < k; i++)
			dest[i] = i;
	} else {
		dest[i]++;
		for(i++; (nat)i < k; i++)
			dest[i] = dest[i - 1] + 1;
	}
	return k;
}
