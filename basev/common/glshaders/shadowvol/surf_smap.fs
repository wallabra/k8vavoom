#version 120
$include "common/common.inc"

uniform vec3 LightPos;
uniform float LightRadius;

varying vec3 VertWorldPos;


void main () {
  //gl_FragDepth = gl_FragCoord.z;
  //gl_FragColor.r = gl_FragCoord.z;
  //gl_FragColor = vec4(length(VertLightDir), gl_FragCoord.z, gl_FragCoord.w, 1.0);
  //gl_FragColor = vec4(length(VertLightDir), 0.0, 0.0, 1.0);
  //vec4 fc = vec4(length(VertLightDir), 0.0, 0.0, 1.0);
  //if (VertLightDir.z < 0) fc.g = 1.0;
  //if (VertLightDir.y > 0) fc.g = 1.0;
  float dist = distance(LightPos, VertWorldPos)+2;
  vec4 fc = vec4(0.0, 0.0, 0.0, 1.0);
  if (dist >= LightRadius) {
    fc.r = 99999.0;
    fc.b = 1.0;
  } else {
    fc.r = dist/LightRadius;
  }
  gl_FragColor = fc;
}
