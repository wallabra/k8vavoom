#version 120
$include "common/common.inc"

varying vec3 VertLightDir;
uniform float LightRadius;


void main () {
  //gl_FragDepth = gl_FragCoord.z;
  //gl_FragColor.r = gl_FragCoord.z;
  //gl_FragColor = vec4(length(VertLightDir), gl_FragCoord.z, gl_FragCoord.w, 1.0);
  //gl_FragColor = vec4(length(VertLightDir), 0.0, 0.0, 1.0);
  vec4 fc = vec4(length(VertLightDir), 0.0, 0.0, 1.0);
  //if (VertLightDir.z < 0) fc.g = 1.0;
  //if (VertLightDir.y > 0) fc.g = 1.0;
  fc.r = length(VertLightDir)/LightRadius;
  gl_FragColor = fc;
}
