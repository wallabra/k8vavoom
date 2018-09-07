#version 120

uniform float LightRadius;

varying vec3 VertToView;
varying vec3 VPosL;
varying vec3 VPos;


void main () {
  float DistToView = /*sqrt*/(dot(VertToView, VertToView));
  if (DistToView < 0.0) discard;

  float DistVPosL = /*sqrt*/(dot(VPosL, VPosL));
  if (DistVPosL < -(LightRadius*LightRadius)) discard;

  float DistVPos = /*sqrt*/(dot(VPos, VPos));
  if (DistVPos < 0.0) discard;

  gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
}
