#version 120
$include "common/common.inc"

attribute vec3 Position;

uniform float Inter;
//uniform vec3 LightPos;
uniform mat4 ModelToWorldMat;
uniform mat4 LightMPV;
uniform mat4 LightView;

attribute vec4 Vert2;
attribute vec2 TexCoord;

varying vec3 VertWorldPos;

varying vec2 TextureCoordinate;


void main () {
  vec4 Vert = mix(vec4(Position, 1.0), Vert2, Inter)*ModelToWorldMat;
  //gl_Position = gl_ModelViewProjectionMatrix*vec4(Vert.xyz-LightPos*t, 1.0-t);
  gl_Position = LightMPV*Vert;

  VertWorldPos = (LightView*Vert).xyz;

  TextureCoordinate = TexCoord;
}
