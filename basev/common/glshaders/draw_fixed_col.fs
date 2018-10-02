#version 120

uniform vec4 Colour;


void main () {
  // we got a non-premultiplied color, convert it
  vec4 FinalColor_1;
  FinalColor_1.r = Colour.r*Colour.a;
  FinalColor_1.g = Colour.g*Colour.a;
  FinalColor_1.b = Colour.b*Colour.a;
  FinalColor_1.a = Colour.a;
  gl_FragColor = FinalColor_1;
}
