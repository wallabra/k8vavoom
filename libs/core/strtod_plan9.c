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
//#define P9_S2F_DEBUG
//#define P9_USE_LDEXP
#include <stdio.h>

//#include <ctype.h>
//#include <errno.h>
#ifdef P9_USE_LDEXP
# include <math.h>
#endif
#include <stdint.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>

//#include "strtod_plan9.h"


#define nelem(x)  (sizeof(x)/sizeof *(x))


static uint32_t umuldiv (uint32_t a, uint32_t b, uint32_t c) {
  double d = ((double)a*(double)b)/(double)c;
  if (d >= 4294967295.0) d = 4294967295.0;
  return d;
}


static const uint64_t uvnan    = 0x7FF0000000000001UL;
static const uint64_t uvinf    = 0x7FF0000000000000UL;
static const uint64_t uvneginf = 0xFFF0000000000000UL;

//static const uint64_t unan    = 0x7F800001U;
static const uint64_t uinf    = 0x7F800000U;
//static const uint64_t uneginf = 0xFF800000U;

static inline double makeNaN (void) { return *(const double *)&uvnan; }
//static inline float makeNaNF (void) { return *(const float *)&unan; }

static inline double makeInf (int negative) { return *(const double *)(negative ? &uvneginf : &uvinf); }
static inline float makeInfF (void) { return *(const float *)&uinf; }

//static inline double makeNegInf (void) { return *(const double *)&uvneginf; }
//static inline float makeNegInfF (void) { return *(const float *)&uneginf; }

#define DBL_MANT_EXTRA_BIT  (0x0010000000000000UL)
#define DBL_MANT_MASK  (0x000FFFFFFFFFFFFFUL)
#define DBL_SIGN_MASK  (0x8000000000000000UL)
#define DBL_EXP_SHIFT  (52)
#define DBL_EXP_SMASK  (0x7FFU)
#define DBL_EXP_OFFSET (1023)


static inline double prevDouble (double d) {
  uint64_t n = *(const uint64_t *)&d;
  if ((n&DBL_MANT_MASK) == 0) {
    //return d; // this seems to be wrong
    // can we borrow bits from exponent?
    if ((uint32_t)((n>>DBL_EXP_SHIFT)&DBL_EXP_SMASK) < 1) return d;
  }
  --n;
  return *(const double *)&n;
}

static inline double nextDouble (double d) {
  uint64_t n = *(const uint64_t *)&d;
  if ((n&DBL_MANT_MASK) == DBL_MANT_MASK) {
    //return d; // this seems to be wrong
    // can we put bits into exponent?
    if ((uint32_t)((n>>DBL_EXP_SHIFT)&DBL_EXP_SMASK) >= DBL_EXP_SMASK-1) return d;
  }
  ++n;
  return *(const double *)&n;
}


/*
static inline int isInf (double d, int sign) {
  uint64_t x = *(const uint64_t*)&d;
  if (sign == 0) return (x == uvinf || x == uvneginf);
  if (sign > 0) return (x == uvinf);
  return (x == uvneginf);
}

static inline int isNaN (double d) {
  const uint64_t x = *(const uint64_t*)&d;
  return (uint32_t)(x>>32)==0x7FF00000 && !isInf(d, 0);
}
*/


static float sanitizedD2F (double d) {
  const float max_finite = 3.4028234663852885981170418348451692544e+38;
  // the half-way point between the max-finite and infinity value
  // since infinity has an even significand everything equal or greater than this value should become infinity
  const double half_max_finite_infinity = 3.40282356779733661637539395458142568448e+38;
  if (d >= max_finite) {
    return (d >= half_max_finite_infinity ? makeInfF() : max_finite);
  } else {
    return (float)(d);
  }
}


//#include <stdio.h>
static double ldexp_intr (double d, int pwr, int *uoflag) {
#ifdef P9_USE_LDEXP
  return ldexp(d, pwr);
#else
  if (pwr == 0) return d;
  //fprintf(stderr, "*** %a; pwr=%d\n", d, pwr);
  int exp = ((*(const uint64_t *)&d)>>DBL_EXP_SHIFT)&DBL_EXP_SMASK;
  if (exp == 0 || exp == DBL_EXP_SMASK) return d;
  if (pwr < 0) {
    if (pwr < -((int)(DBL_EXP_SMASK-1))) {
      if (uoflag) *uoflag = 1;
      return 0.0;
    }
    if ((exp += pwr) <= 0) {
      if (uoflag) *uoflag = 1;
      return 0.0;
    }
  } else {
    if (pwr > (int)DBL_EXP_SMASK-1) {
      if (uoflag) *uoflag = 1;
      return makeInf(((*(const uint64_t *)&d)&DBL_SIGN_MASK) != 0);
    }
    if ((exp += pwr) >= DBL_EXP_SMASK) {
      if (uoflag) *uoflag = 1;
      return makeInf(((*(const uint64_t *)&d)&DBL_SIGN_MASK) != 0);
    }
  }
  uint64_t n = *(const uint64_t *)&d;
  n &= DBL_MANT_MASK|DBL_SIGN_MASK;
  n |= ((uint64_t)(exp&DBL_EXP_SMASK))<<DBL_EXP_SHIFT;
  const double res = *(const double *)&n;
  //fprintf(stderr, " :: %a : %a; pwr=%d\n", ldexp(d, pwr), res, pwr);
  //if (res != ldexp(d, pwr)) abort();
  return res; //ldexp(d, pwr);
#endif
}


/*
 * This routine will convert to arbitrary precision
 * floating point entirely in multi-precision fixed.
 * The answer is the closest floating point number to
 * the given decimal number. Exactly half way are
 * rounded ala ieee rules.
 * Method is to scale input decimal between .500 and .999...
 * with external power of 2, then binary search for the
 * closest mantissa to this decimal number.
 * Nmant is is the required precision. (53 for ieee dp)
 * Nbits is the max number of bits/word. (must be <= 28)
 * Prec is calculated - the number of words of fixed mantissa.
 */
enum {
  Nbits  = 28, /* bits safely represented in a uint32_t */
  Nmant  = 53, /* bits of precision required */
  Prec   = (Nmant+Nbits+1)/Nbits, /* words of Nbits each to represent mantissa */
  Sigbit = 1<<(Prec*Nbits-Nmant), /* first significant bit of Prec-th word */
  Ndig   = 1500,
  One    = (uint32_t)(1<<Nbits),
  Half   = (uint32_t)(One>>1),
  Maxe   = 310,

  Fsign   = 1<<0, /* found - */
  Fesign  = 1<<1, /* found e- */
  Fdpoint = 1<<2, /* found . */
};


typedef struct Tab Tab;
struct Tab {
  int bp;
  int siz;
  const char *cmp;
};


static void frnorm (uint32_t *f) {
  int c = 0;
  for (int i = Prec-1; i > 0; --i) {
    f[i] += c;
    c = f[i]>>Nbits;
    f[i] &= One-1;
  }
  f[0] += c;
}


static int fpcmp (const char *a, uint32_t *f) {
  uint32_t tf[Prec];

  for (int i = 0; i < Prec; ++i) tf[i] = f[i];

  for (;;) {
    /* tf *= 10 */
    for (int i = 0; i < Prec; ++i) tf[i] = tf[i]*10;
    frnorm(tf);
    int d = (tf[0]>>Nbits)+'0';
    tf[0] &= One-1;

    /* compare next digit */
    int c = *a;
    if (c == 0) {
      if ('0' < d) return -1;
      if (tf[0] != 0) goto cont;
      for (int i = 1; i < Prec; ++i) if (tf[i] != 0) goto cont;
      return 0;
    }
    if (c > d) return 1;
    if (c < d) return -1;
    ++a;
  cont:;
  }
  return 0;
}


static void divby (char *a, int *na, int b) {
  char *p = a;
  int n = 0;
  while (n>>b == 0) {
    int c = *a++;
    if (c == 0) {
      while (n) {
        c = n*10;
        if (c>>b) break;
        n = c;
      }
      goto xx;
    }
    n = n*10+c-'0';
    (*na)--;
  }
  for (;;) {
    int c = n>>b;
    n -= c<<b;
    *p++ = c+'0';
    c = *a++;
    if (c == 0) break;
    n = n*10+c-'0';
  }
  (*na)++;
xx:
  while (n) {
    n = n*10;
    int c = n>>b;
    n -= c<<b;
    *p++ = c+'0';
    (*na)++;
  }
  *p = 0;
}


static const Tab tab1[] = {
  { 1,  0, ""},
  { 3,  1, "7"},
  { 6,  2, "63"},
  { 9,  3, "511"},
  {13,  4, "8191"},
  {16,  5, "65535"},
  {19,  6, "524287"},
  {23,  7, "8388607"},
  {26,  8, "67108863"},
  {27,  9, "134217727"},
};


static void divascii (char *a, int *na, int *dp, int *bp) {
  int b;
  const Tab *t;

  int d = *dp;
  if (d >= (int)nelem(tab1)) d = (int)nelem(tab1)-1;
  t = tab1+d;
  b = t->bp;
  if (memcmp(a, t->cmp, t->siz) > 0) --d;
  *dp -= d;
  *bp += b;
  divby(a, na, b);
}


static void mulby (char *a, char *p, char *q, int b) {
  int n = 0;
  *p = 0;
  for (;;) {
    --q;
    if (q < a) break;
    int c = *q-'0';
    c = (c<<b)+n;
    n = c/10;
    c -= n*10;
    --p;
    *p = c+'0';
  }
  while (n) {
    int c = n;
    n = c/10;
    c -= n*10;
    --p;
    *p = c+'0';
  }
}


static const Tab tab2[] = {
  { 1,  1, ""},       /* dp = 0-0 */
  { 3,  3, "125"},
  { 6,  5, "15625"},
  { 9,  7, "1953125"},
  {13, 10, "1220703125"},
  {16, 12, "152587890625"},
  {19, 14, "19073486328125"},
  {23, 17, "11920928955078125"},
  {26, 19, "1490116119384765625"},
  {27, 19, "7450580596923828125"},    /* dp 8-9 */
};


static void mulascii (char *a, int *na, int *dp, int *bp) {
  char *p;
  int b;
  const Tab *t;

  int d = -*dp;
  if (d >= (int)nelem(tab2)) d = (int)nelem(tab2)-1;
  t = tab2+d;
  b = t->bp;
  if (memcmp(a, t->cmp, t->siz) < 0) --d;
  p = a+*na;
  *bp -= b;
  *dp += d;
  *na += d;
  mulby(a, p+d, p, b);
}


static int xcmp (const char *a, const char *b) {
  int c1;
  while ((c1 = *b++) != 0) {
    int c2 = (*a++)|32; // poor man's tolower()
    if (c1 != c2) return 1;
  }
  return 0;
}


static const char *skipSpaces (const char *s) {
  while (*s) {
    switch (*s) {
      case '\t':
      case '\n':
      case '\v':
      case '\f':
      case '\r':
      case ' ':
        ++s;
        break;
      default:
        return s;
    }
  }
  return s;
}


static inline int hexDigit (int ch) {
  return
    ch >= '0' && ch <= '9' ? ch-'0' :
    ch >= 'A' && ch <= 'F' ? ch-'A'+10 :
    ch >= 'a' && ch <= 'f' ? ch-'a'+10 :
    -1;
}


static inline int decDigit (int ch) {
  return (ch >= '0' && ch <= '9' ? ch-'0' : -1);
}


// ////////////////////////////////////////////////////////////////////////// //
enum {
  S0, /* _    _S0 +S1 #S2 .S3 */
  S1, /* _+   #S2 .S3 */
  S2, /* _+#    #S2 .S4 eS5 */
  S3, /* _+.    #S4 */
  S4, /* _+#.#  #S4 eS5 */
  S5, /* _+#.#e +S6 #S7 */
  S6, /* _+#.#e+  #S7 */
  S7, /* _+#.#e+# #S7 */
};

double fmtstrtod (const char *as, char **aas, int *rangeErr) {
  double d;
  int na, ex, dp, bp, c, i, flag, state;
  uint32_t low[Prec], hig[Prec], mid[Prec];
  const char *s;
  char a[Ndig];

  if (rangeErr) *rangeErr = 0;

  if (!as) {
    if (aas) *aas = (char *)as;
    return makeNaN();
  }

  //k8: check for 0x
  flag = 0; /* Fsign, Fesign, Fdpoint */
  s = skipSpaces(as);
  switch (*s) {
    case '-':
      flag |= Fsign;
      /* fallthrough */
    case '+':
      s = skipSpaces(s+1);
      break;
  }

  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    // yay, hex!
    // <=, 'cause we aren't interested in denormalized floats
    if (hexDigit(s[2]) < 0) {
      if (aas) *aas = (char *)as;
      return makeNaN();
    }
    int ftype = -1; // 0: zero; 1: normalized
    int collexp = 0;
    s += 2;
    // collect into double; it is ok, as we are operating powers of two, not decimals
    d = 0.0;
    while (*s) {
      if (*s == '.') {
        if (ftype >= 0) {
          // double dot, wtf?!
          if (aas) *aas = (char *)as;
          return makeNaN();
        }
        ftype = (d != 0.0);
      } else {
        int hd = hexDigit(*s);
        if (hd < 0) break;
        if (ftype == 0) {
          if (hd != 0) {
            // denormalised numbers aren't supported
            if (aas) *aas = (char *)as;
            return makeNaN();
          }
        } else {
          if (rangeErr) {
            *rangeErr = 1;
            uint64_t mv = (*(const uint64_t *)&d)&DBL_MANT_MASK;
            mv = mv*16+((uint32_t)(hd&0x0f));
            if (mv > (DBL_MANT_MASK|DBL_MANT_EXTRA_BIT)) {
              // overflow
              *rangeErr = 1;
            }
          }
          d *= 16.0;
          d += (double)hd;
          if (ftype > 0) collexp += 4;
        }
      }
      ++s;
    }
    // we MUST have the dot (unless it is a zero)
    if (ftype < 0 && d != 0.0) {
      if (aas) *aas = (char *)as;
      return makeNaN();
    }
    //fprintf(stderr, "000: d=%a\n", d);
    // parse exponent
    int exp = 0;
    if (*s == 'p' || *s == 'P') {
      ++s;
      switch (*s) {
        case '-':
          flag |= Fesign;
          /* fallthrough */
        case '+':
          ++s;
          break;
      }
      if (decDigit(s[0]) < 0) {
        if (aas) *aas = (char *)as;
        return makeNaN();
      }
      for (;;) {
        int dd = decDigit(*s);
        if (dd < 0) break;
        ++s;
        exp = exp*10+dd;
        if (exp >= 0x7ffffff) {
          if (aas) *aas = (char *)as;
          return makeNaN();
        }
      }
      // for zero, exponent should be zero
      if (ftype == 0 && exp != 0) {
        if (rangeErr) *rangeErr = 1; // mark it with precision loss
        exp = 0;
      }
    }
    // done
    if (aas) *aas = (char *)skipSpaces(s);
    if (ftype == 0) return 0.0;
    if (flag&Fesign) exp = -exp;
    if (collexp > 0x7ffff) collexp = 0x7ffff;
    //fprintf(stderr, "001: d=%a; exp=%d; collexp=%d\n", d, exp, collexp);
    // process exponent
    d = ldexp_intr(d, exp-collexp, rangeErr);
    if (flag&Fsign) d = -d;
    return d;
  }

  flag = 0; /* Fsign, Fesign, Fdpoint */
  na = 0; /* number of digits of a[] */
  dp = 0; /* na of decimal point */
  ex = 0; /* exponent */

  state = S0;
  for (s = (const char *)as; ; ++s) {
    c = *s;
    if (c >= '0' && c <= '9') {
      switch (state) {
        case S0:
        case S1:
        case S2:
          state = S2;
          break;
        case S3:
        case S4:
          state = S4;
          break;
        case S5:
        case S6:
        case S7:
          state = S7;
          ex = ex*10+(c-'0');
          continue;
      }
      if (na == 0 && c == '0') {
        --dp;
        continue;
      }
      if (na < Ndig-50) a[na++] = c;
      continue;
    }
    switch (c) {
      case '\t':
      case '\n':
      case '\v':
      case '\f':
      case '\r':
      case ' ':
        if (state == S0) continue;
        break;
      case '-':
        if (state == S0) flag |= Fsign; else flag |= Fesign;
        /* fallthrough */
      case '+':
             if (state == S0) state = S1;
        else if (state == S5) state = S6;
        else break; /* syntax */
        continue;
      case '.':
        flag |= Fdpoint;
        dp = na;
        if (state == S0 || state == S1) {
          state = S3;
          continue;
        }
        if (state == S2) {
          state = S4;
          continue;
        }
        break;
      case 'e':
      case 'E':
        if (state == S2 || state == S4) {
          state = S5;
          continue;
        }
        break;
    }
    break;
  }

  /*
   * clean up return char-pointer
   */
  switch (state) {
    case S0:
      if (xcmp(s, "nan") == 0) {
        if (aas) *aas = (char *)s+3;
        return makeNaN();
      }
      /* fallthrough */
    case S1:
      if (xcmp(s, "infinity") == 0) {
        if (aas) *aas = (char *)skipSpaces(s+8);
        goto retinf;
      }
      if (xcmp(s, "inf") == 0) {
        if (aas) *aas = (char *)skipSpaces(s+3);
        goto retinf;
      }
      /* fallthrough */
    case S3:
      if (aas) *aas = (char *)as;
      goto ret0; /* no digits found */
    case S6:
      --s; /* back over +- */
      /* fallthrough */
    case S5:
      --s; /* back over e */
      break;
  }
  if (aas) *aas = (char *)skipSpaces(s);

  if (flag&Fdpoint) while (na > 0 && a[na-1] == '0') --na;
  if (na == 0) goto ret0; /* zero */
  a[na] = 0;
  if (!(flag&Fdpoint)) dp = na;
  if (flag&Fesign) ex = -ex;
  dp += ex;
  if (dp < -Maxe) {
    //errno = ERANGE;
    if (rangeErr) *rangeErr = 1;
    goto ret0;  /* underflow by exp */
  } else if (dp > +Maxe) {
    goto retinf; /* overflow by exp */
  }

  /*
   * normalize the decimal ascii number
   * to range .[5-9][0-9]* e0
   */
  bp = 0; /* binary exponent */
  while (dp > 0) divascii(a, &na, &dp, &bp);
  while (dp < 0 || a[0] < '5') mulascii(a, &na, &dp, &bp);

  /* close approx by naive conversion */
  mid[0] = 0;
  mid[1] = 1;
  for (i = 0; (c = a[i]) != 0; ++i) {
    mid[0] = mid[0]*10+(c-'0');
    mid[1] = mid[1]*10;
    if (i >= 8) break;
  }
  low[0] = umuldiv(mid[0], One, mid[1]);
  hig[0] = umuldiv(mid[0]+1, One, mid[1]);
  for (i = 1; i < Prec; ++i) {
    low[i] = 0;
    hig[i] = One-1;
  }

  /* binary search for closest mantissa */
  for (;;) {
    /* mid = (hig + low) / 2 */
    c = 0;
    for (i = 0; i < Prec; ++i) {
      mid[i] = hig[i]+low[i];
      if (c) mid[i] += One;
      c = mid[i]&1;
      mid[i] >>= 1;
    }
    frnorm(mid);

    /* compare */
    c = fpcmp(a, mid);
    if (c > 0) {
      c = 1;
      for (i = 0; i < Prec; ++i) {
        if (low[i] != mid[i]) {
          c = 0;
          low[i] = mid[i];
        }
      }
      if (c) break;  /* between mid and hig */
      continue;
    }
    if (c < 0) {
      for (i = 0; i < Prec; ++i) hig[i] = mid[i];
      continue;
    }

    /* only hard part is if even/odd roundings wants to go up */
    c = mid[Prec-1]&(Sigbit-1);
    if (c == Sigbit/2 && (mid[Prec-1]&Sigbit) == 0) mid[Prec-1] -= c;
    break; /* exactly mid */
  }

  /* normal rounding applies */
  c = mid[Prec-1]&(Sigbit-1);
  mid[Prec-1] -= c;
  if (c >= Sigbit/2) {
    mid[Prec-1] += Sigbit;
    frnorm(mid);
  }
  goto out;

ret0:
  return 0;

retinf:
  /* Unix strtod requires these.  Plan 9 would return Inf(0) or Inf(-1). */
  //errno = ERANGE;
  if (rangeErr) *rangeErr = 1;
  /*
  if (flag&Fsign) return -HUGE_VAL;
  return HUGE_VAL;
  */
  return makeInf(flag&Fsign);

out:
  d = 0.0;
  for (i = 0; i < Prec; ++i) d = d*One+mid[i];
  if (flag&Fsign) d = -d;
  //fprintf(stderr, "*** d=%a; sign='%c'; resd=%a\n", d, (flag&Fsign ? '-' : '+'), ldexp_intr(d, bp-Prec*Nbits, rangeErr));
  //d = ldexp(d, bp-Prec*Nbits);
  d = ldexp_intr(d, bp-Prec*Nbits, rangeErr);
  if (d == 0) {
    /* underflow */
    //errno = ERANGE;
    if (rangeErr) *rangeErr = 1;
  }
  return d;
}


// ////////////////////////////////////////////////////////////////////////// //
#ifdef P9_S2F_DEBUG
# include <stdio.h>
#endif

float fmtstrtof (const char *as, char **aas, int *rangeErr) {
  int rerr;
  if (!rangeErr) rangeErr = &rerr;

  const double dguess = fmtstrtod(as, aas, &rerr);
  if (rerr) return (float)dguess;
  if (dguess == 0) return (float)dguess;

  int exp = ((*(const uint64_t *)&dguess)>>DBL_EXP_SHIFT)&DBL_EXP_SMASK;
  if (exp == 0 || exp == DBL_EXP_SMASK) return (float)dguess;

  int isNegative = !!((*(const uint64_t *)&dguess)&DBL_SIGN_MASK);

  double dguesspos = dguess;
  // remove sign
  *(uint64_t *)&dguesspos &= ~DBL_SIGN_MASK;

  float fguesspos = sanitizedD2F(dguesspos);
  if (fguesspos == dguesspos) {
    // this shortcut triggers for integer values
    if (isNegative) fguesspos = -fguesspos;
    return fguesspos;
  }

  const double dprev = prevDouble(dguesspos);
  const double dnext = nextDouble(dguesspos);

/*
#ifdef P9_S2F_DEBUG
  fprintf(stderr, "dprev: %a\n", dprev);
  fprintf(stderr, "dcurr: %a\n", dguesspos);
  fprintf(stderr, "dnext: %a\n", dnext);
#endif
*/

  const float f1 = sanitizedD2F(dprev);
  //fguesspos;
  //const float f3 = sanitizedD2F(dnext);
  const float f4 = sanitizedD2F(nextDouble(dnext));
  //assert(f1 <= f2 && f2 <= f3 && f3 <= f4);
  //if (!(f1 <= fguesspos && fguesspos <= f3 && f3 <= f4)) abort();

#ifdef P9_S2F_DEBUG
  fprintf(stderr, "dgp=%a\n", dguesspos);
  fprintf(stderr, "f1 =%a\n", f1);
  fprintf(stderr, "fgp=%a\n", fguesspos);
  fprintf(stderr, "f4 =%a\n", f4);
#endif

  // if the guess doesn't lie near a single-precision boundary we can simply return its float value
  if (f1 == f4) {
    if (isNegative) fguesspos = -fguesspos;
    return fguesspos;
  }

  /* assert
  if (!(f1 != fguesspos && fguesspos == f3 && f3 == f4) &&
      !(f1 == fguesspos && fguesspos != f3 && f3 == f4) &&
      !(f1 == fguesspos && fguesspos == f3 && f3 != f4)) abort();
  */
#ifdef P9_S2F_DEBUG
  fprintf(stderr, "!!!\n");
#endif

  // THIS IS ABSOLUTELY WRONG!
  uint32_t dv = ((*(const uint64_t *)&dguesspos)&DBL_MANT_MASK)>>28;
  fguesspos = (dv&1 ? f4 : f1);
  if (isNegative) fguesspos = -fguesspos;
  return fguesspos;
}
