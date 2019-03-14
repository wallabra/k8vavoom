#version 120

uniform sampler2D Texture;
uniform vec4 Light;
uniform float AlphaRef;

$include "common/fog_vars.fs"

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate)*Light;
  if (TexColour.a < AlphaRef) discard;

  // convert to premultiplied
  vec4 FinalColour_1;
  FinalColour_1.r = TexColour.r*TexColour.a*Light.a;
  FinalColour_1.g = TexColour.g*TexColour.a*Light.a;
  FinalColour_1.b = TexColour.b*TexColour.a*Light.a;
  FinalColour_1.a = TexColour.a;
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColour_1;
}
