#ifdef VV_CMP_SHITTY_CHECKS
  $include "shadowvol/cubemap_check_three_inc.fs"
#else
  #ifdef VV_CMP_FASTEST_CHECKS
    $include "shadowvol/cubemap_check_three_inc.fs"
  #else
    $include "shadowvol/cubemap_check_four_inc.fs"
  #endif
#endif
