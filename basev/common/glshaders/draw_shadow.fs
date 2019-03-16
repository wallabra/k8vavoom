#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform float Alpha;

varying vec2 TextureCoordinate;


void main () {
  float Transp = clamp(((texture2D(Texture, TextureCoordinate).a*Alpha)-0.1)/0.9, 0.0, 1.0);

  vec4 FinalColour;
  FinalColour.rgb = vec3(0.0, 0.0, 0.0);
  FinalColour.a = Transp*Transp*(3.0-(2.0*Transp));
  gl_FragColor = FinalColour;
}
