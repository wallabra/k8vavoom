#version 120

uniform sampler2D Texture;
uniform vec4 Light;
uniform bool FogEnabled;
uniform int FogType;
uniform vec4 FogColour;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;
uniform float AlphaRef;

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate)*Light;
  if (TexColour.a < AlphaRef) discard;

  //vec4 FinalColour_1 = TexColour;
  // premultiply
  vec4 FinalColour_1;
  FinalColour_1.r = TexColour.r*Light.a;
  FinalColour_1.g = TexColour.g*Light.a;
  FinalColour_1.b = TexColour.b*Light.a;
  FinalColour_1.a = TexColour.a;
  $include "common_fog.fs"

  gl_FragColor = FinalColour_1;
}
