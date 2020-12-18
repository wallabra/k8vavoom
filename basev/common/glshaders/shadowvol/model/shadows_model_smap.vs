#version 120
$include "common/common.inc"

//attribute vec3 Position;

uniform float Inter;
//uniform vec3 LightPos;
uniform mat4 ModelToWorldMat;

attribute vec4 Vert2;
attribute vec2 TexCoord;

varying vec2 TextureCoordinate;

$include "shadowvol/smap_builder_decl.vs"


void main () {
  vec4 Vert = mix(vec4(Position, 1.0), Vert2, Inter)*ModelToWorldMat;
  TextureCoordinate = TexCoord;
  $include "shadowvol/smap_builder_calc.vs"
}
