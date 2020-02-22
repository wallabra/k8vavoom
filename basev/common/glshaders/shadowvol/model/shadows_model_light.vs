#version 120
$include "common/common.inc"

uniform mat4 ModelToWorldMat;
uniform mat3 NormalToWorldMat;
uniform vec3 LightPos;
uniform vec3 ViewOrigin;
uniform float Inter;

attribute vec4 Vert2;
attribute vec3 VertNormal;
attribute vec3 Vert2Normal;
attribute vec2 TexCoord;

varying vec3 Normal;
#ifdef VV_EXPERIMENTAL_FAST_LIGHT
varying vec3 VertToView;
varying vec3 VPos;
varying vec3 VPosL;
#endif
varying vec3 VertToLight;
varying float Dist;
varying float VDist;

varying vec2 TextureCoordinate;
//varying float PlaneDist;


void main () {
  vec4 Vert = mix(gl_Vertex, Vert2, Inter)*ModelToWorldMat;
  gl_Position = gl_ModelViewProjectionMatrix*Vert;

  Normal = NormalToWorldMat*mix(VertNormal, Vert2Normal, Inter);

  /*
  float SurfDist = dot(Normal, Vert.xyz);
  float PlaneDist = SurfDist;
  Dist = dot(LightPos, Normal)-SurfDist;
  VDist = dot(ViewOrigin, Normal)-SurfDist;
  VertToLight = LightPos-Vert.xyz;
  VertToView = ViewOrigin-Vert.xyz;
  VPosL = LightPos-gl_Position.xyz;
  VPos = ViewOrigin-gl_Position.xyz;
  */

  float SurfDist = dot(Normal, Vert.xyz);
  Dist = dot(LightPos, Normal)-SurfDist;
  VDist = dot(ViewOrigin, Normal)-SurfDist;
  VertToLight = LightPos-Vert.xyz;

  TextureCoordinate = TexCoord;
}
