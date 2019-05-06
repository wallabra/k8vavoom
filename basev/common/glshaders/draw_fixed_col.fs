#version 120
$include "common/common.inc"

uniform vec4 Color;


void main () {
  // we got a non-premultiplied color, convert it
  vec4 FinalColor_1;
  FinalColor_1.r = Color.r*Color.a;
  FinalColor_1.g = Color.g*Color.a;
  FinalColor_1.b = Color.b*Color.a;
  FinalColor_1.a = Color.a;
  gl_FragColor = FinalColor_1;
}
