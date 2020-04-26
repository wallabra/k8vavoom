#version 120
$include "common/common.inc"

//#ifdef GL4ES_HACKS
attribute vec3 Position;
//#endif
//attribute vec2 TexCoord;
//

// vertex shader for simple (non-lightmapped) surfaces
//$include "common/texture_vars.vs"
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
//#ifdef GL4ES_HACKS
  gl_Position = gl_ModelViewProjectionMatrix*vec4(Position, 1.0);
/*
#else
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
#endif
*/
  // pass texture coordinates
  //TextureCoordinate = TexCoord;
  //$include "common/texture_calc.vs"
  TextureCoordinate = vec2(
    dot(Position-TexOrg, SAxis)*TexIW,
    dot(Position-TexOrg, TAxis)*TexIH
  );

#ifdef VV_MASKED_GLOW
  $include "common/glow_calc.vs"
#endif
}
