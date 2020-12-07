#version 120
$include "common/common.inc"

//130: temp

uniform samplerCube Texture;
//uniform samplerCubeShadow Texture;
uniform float CubeFace;
varying vec2 TextureCoordinate;


void main () {
  //vec4 depth = textureCube(Texture, vec3(TextureCoordinate.x, TextureCoordinate.y, CubeZ));

  // front, back, left, right, top, bottom
  float zofs = 0.5;
  vec4 depth;
       if (CubeFace < 1.0) depth = textureCube(Texture, vec3( zofs, 0.5-TextureCoordinate.y, 0.5-TextureCoordinate.x));
  else if (CubeFace < 2.0) depth = textureCube(Texture, vec3(-zofs, 0.5-TextureCoordinate.y, TextureCoordinate.x-0.5));
  else if (CubeFace < 3.0) depth = textureCube(Texture, vec3(TextureCoordinate.x-0.5,  zofs, TextureCoordinate.y-0.5));
  else if (CubeFace < 4.0) depth = textureCube(Texture, vec3(TextureCoordinate.x-0.5, -zofs, 0.5-TextureCoordinate.y));
  else if (CubeFace < 5.0) depth = textureCube(Texture, vec3(TextureCoordinate.x-0.5, 0.5-TextureCoordinate.y,  zofs));
  else if (CubeFace < 6.0) depth = textureCube(Texture, vec3(0.5-TextureCoordinate.x, 0.5-TextureCoordinate.y, -zofs));
  else depth = textureCube(Texture, vec3(TextureCoordinate.x-0.5, TextureCoordinate.y-0.5, 0.0));
  //float depth = textureCube(Texture, vec4(TextureCoordinate.x, TextureCoordinate.y, CubeZ, 0));

  vec4 FinalColor;
  FinalColor.a = 1.0;
  FinalColor.rgb = vec3(depth.r, depth.g, depth.b);
  //FinalColor.rgb = vec3(TextureCoordinate.x, TextureCoordinate.y, CubeZ);

  gl_FragColor = FinalColor;
}
