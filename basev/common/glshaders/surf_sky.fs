#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform float Brightness;

varying vec2 TextureCoordinate;


void main () {
  vec4 BrightFactor;
  BrightFactor.a = 1.0;
  BrightFactor.r = Brightness;
  BrightFactor.g = Brightness;
  BrightFactor.b = Brightness;

  gl_FragColor = texture2D(Texture, TextureCoordinate)*BrightFactor;
}
