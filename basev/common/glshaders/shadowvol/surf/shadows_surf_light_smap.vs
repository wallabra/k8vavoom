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
uniform mat4 LightView;
varying vec4 VertLightDir;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  //VertLightPos = (LightMPV*vec4(gl_Vertex.xyz, 1)).xyz;
  //VertLightDir = gl_Vertex.xyz-LightPos;
  //VertLightDir = LightPos-gl_Vertex.xyz;

  $include "common/texture_calc.vs"

  Normal = SurfNormal;

  float LightDist = dot(LightPos, SurfNormal);
  float ViewDist = dot(ViewOrigin, SurfNormal);
  Dist = LightDist-SurfDist;
  VDist = ViewDist-SurfDist;

  VertToLight = LightPos-gl_Vertex.xyz;

  VertLightDir = LightView*gl_Vertex;
}
