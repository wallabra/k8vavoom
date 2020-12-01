#version 120
$include "common/common.inc"

uniform vec3 ViewOrigin;
uniform vec3 LightPos;
$include "common/texture_vars.vs"

/*attribute*/uniform vec3 SurfNormal;
/*attribute*/uniform float SurfDist;

varying vec3 Normal;
varying vec3 VertToLight;
varying float Dist;
varying float VDist;

//uniform mat4 LightMPV;
//uniform mat4 LightView;
//varying vec4 VertLightDir;
varying vec3 VertWorldPos;
uniform mat4 LightView;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;

  $include "common/texture_calc.vs"

  //Normal = normalize(vec4(SurfNormal, 1)*gl_ModelViewMatrix).xyz;
  Normal = SurfNormal;

  float LightDist = dot(LightPos, SurfNormal);
  float ViewDist = dot(ViewOrigin, SurfNormal);
  Dist = LightDist-SurfDist;
  VDist = ViewDist-SurfDist;

  VertToLight = LightPos-gl_Vertex.xyz;

  //VertLightDir = LightView*gl_Vertex;
  //VertWorldPos = (gl_ModelViewMatrix*gl_Vertex).xyz;
  //VertWorldPos = gl_Vertex.xyz;
  VertWorldPos = (LightView*gl_Vertex).xyz;
}
