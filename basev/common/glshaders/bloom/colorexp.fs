#version 120
$include "common/common.inc"
// color exponentiation shader
// for contrast boosting

uniform vec4 Exponent;
uniform sampler2D TextureSource;

varying vec2 TextureCoordinate;


void main () {
  // .a should be 1.0 here, otherwise unrendered parts will be grayed
  vec4 clr = pow(texture2D(TextureSource, TextureCoordinate.st), Exponent);
  gl_FragColor = vec4(clr.rgb, 1.0);
}
