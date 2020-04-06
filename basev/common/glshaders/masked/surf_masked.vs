#version 120
$include "common/common.inc"

attribute vec3 Position;
attribute vec2 TexCoord;

varying vec2 TextureCoordinate;

#ifdef VV_MASKED_GLOW
$include "common/glow_vars.vs"
#endif


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*vec4(Position, 1.0);
  // pass texture coordinates
  TextureCoordinate = TexCoord;
#ifdef VV_MASKED_GLOW
  $include "common/glow_calc.vs"
#endif
}
