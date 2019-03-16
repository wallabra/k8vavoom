#version 120
$include "common/common.inc"

#define VAVOOM_SIMPLE_ALPHA_FOG
$include "common/fog_vars.fs"


void main () {
  vec4 FinalColour_1;
  $include "common/fog_calc_main.fs"

  gl_FragColor = FinalColour_1;
}
