#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform float Brightness;

varying vec2 TextureCoordinate;


void main () {
  vec4 BrightFactor = vec4(Brightness, Brightness, Brightness, 1.0);
  gl_FragColor = texture2D(Texture, TextureCoordinate)*BrightFactor;
}
