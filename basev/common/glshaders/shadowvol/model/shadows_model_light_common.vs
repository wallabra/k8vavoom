attribute vec3 Position;

uniform mat4 ModelToWorldMat;
uniform mat3 NormalToWorldMat;
uniform vec3 LightPos;
/*uniform vec3 ViewOrigin;*/
uniform float Inter;

attribute vec4 Vert2;
attribute vec3 VertNormal;
attribute vec3 Vert2Normal;
attribute vec2 TexCoord;

varying vec3 Normal;
varying vec3 VertToLight;
/*
varying float Dist;
varying float VDist;
*/

varying vec2 TextureCoordinate;

#ifdef VV_SHADOWMAPS
$include "shadowvol/smap_light_decl.vs"
#endif


void main () {
  vec4 Vert = mix(vec4(Position, 1.0), Vert2, Inter)*ModelToWorldMat;
  gl_Position = gl_ModelViewProjectionMatrix*Vert;

  //Normal = NormalToWorldMat*mix(VertNormal, Vert2Normal, Inter);
  Normal = mix(VertNormal, Vert2Normal, Inter)*NormalToWorldMat;
  VertToLight = LightPos-Vert.xyz;

  /*
  float SurfDist = dot(Normal, Vert.xyz);
  Dist = dot(LightPos, Normal)-SurfDist;
  VDist = dot(ViewOrigin, Normal)-SurfDist;
  */

  TextureCoordinate = TexCoord;

#ifdef VV_SHADOWMAPS
  VertWorldPos = (LightView*Vert).xyz;
#endif
}
