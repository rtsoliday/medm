#ifndef QTEDM_MEDM_CALC_H
#define QTEDM_MEDM_CALC_H

#include <stddef.h>

/* Bound both 80-entry stacks in the legacy calculator. Each infix byte
 * needs at most nine postfix bytes, including temporary numeric text. */
#define QTEDM_CALC_MAX_INFIX 79
#define QTEDM_CALC_POSTFIX_CAPACITY 1024

#ifdef __cplusplus
extern "C" {
#endif

long qtedmPostfix(char *infix, char *post, size_t capacity, short *error);
long calcPerform(double *args, double *result, char *post);

#ifdef __cplusplus
}
#endif

#endif
