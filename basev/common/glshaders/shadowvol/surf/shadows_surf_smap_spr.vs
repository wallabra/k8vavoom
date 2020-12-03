#version 120
$include "common/common.inc"

uniform vec3 SAxis;
uniform vec3 TAxis;
uniform float TexIW;
uniform float TexIH;
uniform vec3 TexOrg;

// put this also into fragment shader
varying vec2 TextureCoordinate;

$include "shadowvol/smap_builder_decl.vs"


void main () {
  vec4 Vert = gl_Vertex;
  $include "shadowvol/smap_builder_calc.vs"

  TextureCoordinate = vec2(
    dot(gl_Vertex.xyz-TexOrg, SAxis)*TexIW,
    dot(gl_Vertex.xyz-TexOrg, TAxis)*TexIH
  );
}
