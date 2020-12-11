#ifdef VV_SMAP_BLUR4_BI
# define VV_SMAP_WEIGHTED_BLUR
# ifndef VV_SMAP_BLUR4
#  define VV_SMAP_BLUR4
# endif
# ifdef VV_SMAP_BLUR8
#  undef VV_SMAP_BLUR16
# endif
# ifdef VV_SMAP_BLUR16
#  undef VV_SMAP_BLUR16
# endif
#endif

#ifdef VV_SMAP_BLUR8_FAST
# define VV_SMAP_BLUR8
# define VV_SMAP_BLUR_FAST8
#endif


// for each next blur, previous one must be defined
//#define VV_SMAP_BLUR4
//#define VV_SMAP_BLUR8
//#define VV_SMAP_BLUR16

//#define VV_SMAP_BLUR_FAST8

// ensure proper defines
#ifdef VV_SMAP_BLUR16
# ifndef VV_SMAP_BLUR8
#  define VV_SMAP_BLUR8
# endif
#endif

#ifdef VV_SMAP_BLUR8
# ifndef VV_SMAP_BLUR4
#  define VV_SMAP_BLUR4
# endif
#endif

#ifdef VV_SMAP_BLUR16
# define DCOUNT 17.0
#endif

#ifndef DCOUNT
# ifdef VV_SMAP_BLUR8
#  define DCOUNT 9.0
# endif
#endif

#ifndef DCOUNT
# define DCOUNT 5.0
#endif

#ifndef VV_DYNAMIC_DCOUNT
# ifdef VV_SMAP_BLUR_FAST8
#  define VV_DYNAMIC_DCOUNT
# endif
#endif


#ifdef GLVER_MAJOR_4
# define textureCubeFn  texture
#endif

#ifndef textureCubeFn
# ifdef GLVER_MAJOR_3
#  define textureCubeFn  texture
# endif
#endif

#ifndef textureCubeFn
# define textureCubeFn  textureCube
#endif


// try to do weighted 4-texel sampling?
// DO NOT SET!
//#define VV_SMAP_WEIGHTED_BLUR

#ifdef VV_MODEL_LIGHTING
// for models
# define VV_SMAP_BIAS  (0.008)
//# define VV_SMAP_BIAS_N  (0.1)
#else
// this seems to be enough for surfaces
//# define VV_SMAP_BIAS  (0.0006)
# define VV_SMAP_BIAS  (0.00065)
//# define VV_SMAP_BIAS_N  (0.1)
#endif

#ifdef VV_SMAP_WEIGHTED_BLUR
# ifdef VV_SMAP_BLUR8
#  undef VV_SMAP_WEIGHTED_BLUR
# endif
#endif

#ifdef VV_SMAP_WEIGHTED_BLUR
# ifndef VV_SMAP_BLUR4
#  undef VV_SMAP_WEIGHTED_BLUR
# endif
#endif
