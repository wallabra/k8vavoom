#version 120
$include "common/common.inc"


void main () {
  gl_FragDepth = gl_FragCoord.z;
}
