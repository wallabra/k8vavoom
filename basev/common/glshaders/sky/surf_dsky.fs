#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform sampler2D Texture2;
uniform float Brightness;

varying vec2 TextureCoordinate;
varying vec2 Texture2Coordinate;


void main () {
  vec4 Tex2 = texture2D(Texture, Texture2Coordinate);

  float ClampTransp2 = clamp((Tex2.a-0.1)/0.9, 0.0, 1.0);

  vec4 BrightFactor = vec4(Brightness, Brightness, Brightness, 1.0);

  gl_FragColor = mix(texture2D(Texture2, TextureCoordinate), Tex2, ClampTransp2*(ClampTransp2*(3.0-(2.0*ClampTransp2))))*BrightFactor;
}
