#version 120
$include "common/common.inc"

uniform mat4 ModelToWorldMat;
//uniform mat3 NormalToWorldMat;
//uniform vec3 ViewOrigin;
uniform float Inter;

attribute vec4 Vert2;
attribute vec2 TexCoord;
/*
attribute vec3 VertNormal;
attribute vec3 Vert2Normal;
*/

varying vec2 TextureCoordinate;
/*
varying vec3 Normal;
varying float Dist;
*/


void main () {
  vec4 Vert = mix(gl_Vertex, Vert2, Inter)*ModelToWorldMat;
  gl_Position = gl_ModelViewProjectionMatrix*Vert;

  /*
  Normal = NormalToWorldMat*mix(VertNormal, Vert2Normal, Inter);

  float SurfDist = dot(Normal, Vert.xyz);
  Dist = dot(ViewOrigin, Normal)-SurfDist;
  */
  TextureCoordinate = TexCoord;
}
