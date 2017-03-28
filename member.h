/* vim:set tw=80 ts=8 sw=8 sts=8 cc=80 noet cino+=t0: */
#if MEMBERSHIP_MATRIX

#if DENSE_MEMBERSHIP_MATRIX
#define MEMBER_T int32_t
#define MAJOR(b) ((b) / 32 + SIGN(b))
#define MINOR(b) (abs(b) % 32)
#define _MEMBER(a, b) (rtc->membership_matrix[a][MAJOR(b)] & 1 << MINOR(b))
#define _SET_MEMBER(a, b) rtc->membership_matrix[a][MAJOR(b)] |= 1 << MINOR(b)
#else
#define MEMBER_T bool
#define _MEMBER(a, b) (rtc->membership_matrix[(a)][(b)])
#define _SET_MEMBER(a, b) rtc->membership_matrix[(a)][(b)] = true
#endif

#if MEMBERSHIP_MATRIX == LITERAL_CLAUSE
#define MEMBER(lit, c) _MEMBER((lit), (c))
#define SET_MEMBER(lit, c) _SET_MEMBER((lit), (c))
#else
#define MEMBER(lit, c) _MEMBER((c), (lit))
#define SET_MEMBER(lit, c) _SET_MEMBER((c), (lit))
#endif

#if DENSE_MEMBERSHIP_MATRIX
#if MEMBERSHIP_MATRIX == LITERAL_CLAUSE
#define DIM1 (2 * rtc->v_size)
#define DIM2 (rtc->c_size * sizeof(int) / 32)
#else
#define DIM1 (rtc->c_size)
#define DIM2 (4 + 2 * rtc->v_size * sizeof(int) / 32)
#endif
#else
#if MEMBERSHIP_MATRIX == LITERAL_CLAUSE
#define DIM1 (2 * rtc->v_size)
#define DIM2 (rtc->c_size)
#else
#define DIM1 (rtc->c_size)
#define DIM2 (2 * rtc->v_size)
#endif
#endif

#if MEMBERSHIP_MATRIX == LITERAL_CLAUSE
#define DIM1_OFFSET ((int)DIM1 / 2)
#define DIM2_OFFSET 0
#else
#define DIM1_OFFSET 0
#define DIM2_OFFSET (DIM2 / 2)
#endif

#elif SORT_CLAUSE_LITERALS == ASCENDINGLY
#if SMALL_CLAUSE_OPTIMIZATION
#error ?
#endif
#define MEMBER(lit, c)                                                         \
	bsearch((&lit), CLAUSE(c)->_literals, CLAUSE(c)->count,                 \
		sizeof(CLAUSE(c)->_literals[0]), compare)
#elif SORT_CLAUSE_LITERALS == MAG_SIGN
#if SMALL_CLAUSE_OPTIMIZATION
#error ?
#endif
#define MEMBER(lit, c)                                                         \
	bsearch((&lit), CLAUSE(c)->_literals, CLAUSE(c)->count,                 \
		sizeof(CLAUSE(c)->_literals[0]), compare_mag_sign)
#elif HASHTABLE
#define MEMBER(lit, c) hashtable_lookup(&rtc->table, lit, c)
#else
#define MEMBER(lit, c) member((lit), CLAUSE(c))
#endif
