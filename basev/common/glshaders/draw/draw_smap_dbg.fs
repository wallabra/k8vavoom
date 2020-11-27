#version 130
$include "common/common.inc"

//130: temp

uniform samplerCube Texture;
//uniform samplerCubeShadow Texture;
uniform float CubeZ;
varying vec2 TextureCoordinate;


void main () {
  vec4 depth = texture(Texture, vec3(TextureCoordinate.x, TextureCoordinate.x, CubeZ));
  //float depth = texture(Texture, vec4(TextureCoordinate.x, TextureCoordinate.x, CubeZ, 0));

  vec4 FinalColor;
  FinalColor.a = 1.0;
  FinalColor.rgb = vec3(depth.r, depth.g, depth.b);

  gl_FragColor = FinalColor;
}
