#version 120
$include "common/common.inc"

uniform vec3 SAxis;
uniform vec3 TAxis;
uniform float SOffs;
uniform float TOffs;
/*
uniform float TexIW;
uniform float TexIH;
*/
uniform float TexMinS;
uniform float TexMinT;
uniform float CacheS;
uniform float CacheT;

varying vec2 TextureCoordinate;
varying vec2 LightmapCoordinate;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;

  TextureCoordinate = gl_MultiTexCoord0.xy;

  float s = dot(gl_Vertex.xyz, SAxis)+SOffs;
  float t = dot(gl_Vertex.xyz, TAxis)+TOffs;

  vec2 lightst = vec2(
    (s-TexMinS+CacheS*16.0+8.0)/2048.0,
    (t-TexMinT+CacheT*16.0+8.0)/2048.0
  );

  LightmapCoordinate = lightst;
}
