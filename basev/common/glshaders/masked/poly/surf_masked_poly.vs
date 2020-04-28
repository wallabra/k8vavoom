#version 120
$include "common/common.inc"

attribute vec3 Position;

uniform vec3 SAxis;
uniform vec3 TAxis;
uniform float TexIW;
uniform float TexIH;
uniform vec3 TexOrg;
varying vec2 TextureCoordinate;

#ifdef VV_MASKED_GLOW
$include "common/glow_vars.vs"
#endif


void main () {
  // transforming the vertex
  //gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  gl_Position = gl_ModelViewProjectionMatrix*vec4(Position, 1.0);
  // pass texture coordinates
  TextureCoordinate = vec2(
    (dot(Position, SAxis)+TexOrg.x)*TexIW,
    (dot(Position, TAxis)+TexOrg.y)*TexIH
  );

#ifdef VV_MASKED_GLOW
  $include "common/glow_calc.vs"
#endif
}
