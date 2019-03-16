#version 120
$include "common/common.inc"

uniform vec3 ViewOrigin;
uniform vec3 LightPos;
$include "common/texture_vars.vs"

attribute vec3 SurfNormal;
attribute float SurfDist;

varying vec3 Normal;
varying vec3 VertToLight;
varying vec3 VertToView;
varying float Dist;
varying float VDist;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;

  $include "common/texture_calc.vs"

  Normal = SurfNormal;

  float LightDist = dot(LightPos, SurfNormal);
  float ViewDist = dot(ViewOrigin, SurfNormal);
  Dist = LightDist-SurfDist;
  VDist = ViewDist-SurfDist;

  VertToLight = LightPos-gl_Vertex.xyz;
  VertToView = ViewOrigin-gl_Vertex.xyz;
}
