/* vim:set tw=80 ts=8 sw=8 sts=8 cc=80 noet cino+=t0: */
#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE /* for qsort_r */

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* typedef int nat; */
typedef unsigned int nat;

#define SIGN(n) (((n) > 0) - ((n) < 0))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#ifndef LINE_SIZE
#define LINE_SIZE 64
#endif

#endif /* COMMON_H */
