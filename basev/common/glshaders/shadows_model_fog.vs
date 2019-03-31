#version 120
$include "common/common.inc"

uniform mat4 ModelToWorldMat;
uniform mat3 NormalToWorldMat;
uniform vec3 ViewOrigin;
uniform float Inter;

attribute vec4 Vert2;
attribute vec3 VertNormal;
attribute vec3 Vert2Normal;
attribute vec2 TexCoord;

varying vec3 Normal;
varying vec2 TextureCoordinate;
varying float Dist;


void main () {
  vec4 Vert;
  Vert.xyz = (mix(gl_Vertex, Vert2, Inter)*ModelToWorldMat).xyz;
  Vert.w = 1.0;
  gl_Position = gl_ModelViewProjectionMatrix*Vert;

  Normal = NormalToWorldMat*mix(VertNormal, Vert2Normal, Inter);

  float SurfDist = dot(Normal, Vert.xyz);
  Dist = dot(ViewOrigin, Normal)-SurfDist;
  TextureCoordinate = TexCoord;
}
