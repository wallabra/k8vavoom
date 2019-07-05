namespace ZDBSP {

#define FIXED_MAX   INT_MAX
#define FIXED_MIN   INT_MIN

#define FRACBITS    (16)

typedef int fixed_t;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef signed short SWORD;
#ifdef _WIN32
typedef unsigned long DWORD;
#else
typedef uint32_t DWORD;
#endif
typedef uint32_t angle_t;

//angle_t PointToAngle (fixed_t x, fixed_t y);

//==========================================================================
//
// PointToAngle
//
//==========================================================================
static inline __attribute__((unused)) angle_t PointToAngle (fixed_t x, fixed_t y) {
  double ang = atan2 (double(y), double(x));
  const double rad2bam = double(1<<30) / M_PI;
  double dbam = ang * rad2bam;
  // Convert to signed first since negative double to unsigned is undefined.
  return angle_t(int(dbam)) << 1;
}


static const WORD NO_MAP_INDEX = 0xffff;
static const DWORD NO_INDEX = 0xffffffff;
static const angle_t ANGLE_MAX = 0xffffffff;
static const DWORD DWORD_MAX = 0xffffffff;
static const angle_t ANGLE_180 = (1u<<31);
static const angle_t ANGLE_EPSILON = 5000;


#ifdef __BIG_ENDIAN__

// Swap 16bit, that is, MSB and LSB byte.
// No masking with 0xFF should be necessary.
static inline __attribute__((unused)) short LittleShort (short x) {
  return (short)((((unsigned short)x)>>8) | (((unsigned short)x)<<8));
}

static inline __attribute__((unused)) unsigned short LittleShort (unsigned short x) {
  return (unsigned short)((x>>8) | (x<<8));
}

// Swapping 32bit.
static inline __attribute__((unused)) unsigned int LittleLong (unsigned int x) {
  return (unsigned int)(
              (x>>24)
              | ((x>>8) & 0xff00)
              | ((x<<8) & 0xff0000)
              | (x<<24));
}

static inline __attribute__((unused)) int LittleLong (int x) {
  return (int)(
         (((unsigned int)x)>>24)
         | ((((unsigned int)x)>>8) & 0xff00)
         | ((((unsigned int)x)<<8) & 0xff0000)
         | (((unsigned int)x)<<24));
}

#else

#define LittleShort(x)          (x)
#define LittleLong(x)           (x)

#endif // __BIG_ENDIAN__


#if defined(__i386__)

static inline __attribute__((unused)) fixed_t Scale (fixed_t a, fixed_t b, fixed_t c) {
  fixed_t result, dummy;

  asm volatile
    ("imull %3\n\t"
     "idivl %4"
     : "=a,a,a,a,a,a" (result),
       "=&d,&d,&d,&d,d,d" (dummy)
     :   "a,a,a,a,a,a" (a),
         "m,r,m,r,d,d" (b),
         "r,r,m,m,r,m" (c)
     : "%cc"
      );

  return result;
}

static inline __attribute__((unused)) fixed_t DivScale30 (fixed_t a, fixed_t b) {
  fixed_t result, dummy;
  asm volatile
    ("idivl %4"
    : "=a,a" (result),
      "=d,d" (dummy)
    : "a,a" (a<<30),
      "d,d" (a>>2),
      "r,m" (b) \
    : "%cc");
  return result;
}

static inline __attribute__((unused)) fixed_t MulScale30 (fixed_t a, fixed_t b) {
  return ((int64_t)a * b) >> 30;
}

static inline __attribute__((unused)) fixed_t DMulScale30 (fixed_t a, fixed_t b, fixed_t c, fixed_t d) {
  return (((int64_t)a * b) + ((int64_t)c * d)) >> 30;
}

static inline __attribute__((unused)) fixed_t DMulScale32 (fixed_t a, fixed_t b, fixed_t c, fixed_t d) {
  return (((int64_t)a * b) + ((int64_t)c * d)) >> 32;
}

#else // non-x86

static inline __attribute__((unused)) fixed_t Scale (fixed_t a, fixed_t b, fixed_t c) {
  return (fixed_t)(double(a)*double(b)/double(c));
}

static inline __attribute__((unused)) fixed_t DivScale30 (fixed_t a, fixed_t b) {
  return (fixed_t)(double(a)/double(b)*double(1<<30));
}

static inline __attribute__((unused)) fixed_t MulScale30 (fixed_t a, fixed_t b) {
  return (fixed_t)(double(a)*double(b)/double(1<<30));
}

static inline __attribute__((unused)) fixed_t DMulScale30 (fixed_t a, fixed_t b, fixed_t c, fixed_t d) {
  return (fixed_t)((double(a)*double(b)+double(c)*double(d))/double(1<<30));
}

static inline __attribute__((unused)) fixed_t DMulScale32 (fixed_t a, fixed_t b, fixed_t c, fixed_t d) {
  return (fixed_t)((double(a)*double(b)+double(c)*double(d))/4294967296.0);
}

#endif

} // namespace
