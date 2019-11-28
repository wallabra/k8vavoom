#version 120
$include "common/common.inc"
// color exponentiation shader
// for contrast boosting

uniform vec4 Exponent;
uniform sampler2D TextureSource;

varying vec2 TextureCoordinate;


void main () {
  gl_FragColor = pow(texture2D(TextureSource, TextureCoordinate.st), Exponent);
}
