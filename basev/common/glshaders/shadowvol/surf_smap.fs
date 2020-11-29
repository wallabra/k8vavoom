#version 120
$include "common/common.inc"


void main () {
  //gl_FragDepth = gl_FragCoord.z;
  //gl_FragColor.r = gl_FragCoord.z;
  /*
  gl_FragColor.r = 0.1;
  gl_FragColor.g = 0.8;
  gl_FragColor.b = 0.1;
  gl_FragColor.a = 1.0;
  */
#ifdef VV_SMAP_CLEAR
  gl_FragColor.r = 0.2;
  gl_FragColor.g = 0.2;
  //gl_FragColor.b = 0.8;
  gl_FragColor.b = gl_FragCoord.z;
  gl_FragColor.a = 1.0;
  //gl_FragDepth = 8192.0;
#else
  gl_FragColor.r = gl_FragCoord.z;
  gl_FragColor.g = gl_FragCoord.w;
  gl_FragColor.b = 0.0;
  gl_FragColor.a = 1.0;
#endif
}
