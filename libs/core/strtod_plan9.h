/*
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */
#ifndef PLAN9_STRTOD_CODE
#define PLAN9_STRTOD_CODE

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * This routine will convert to arbitrary precision
 * floating point entirely in multi-precision fixed.
 * The answer is the closest floating point number to
 * the given decimal number. Exactly half way are
 * rounded ala ieee rules.
 *
 * returns something close on range error (0/inf) (and sets flag)
 * 0 doesn't have its sign properly set
 *
 * on error, can return anything, including NaN/Inf
 */
double fmtstrtod (const char *as, char **aas, int *rangeErr);

// this may do duplicate rounding
float fmtstrtof (const char *as, char **aas, int *rangeErr);

#if defined(__cplusplus)
}
#endif

#endif
