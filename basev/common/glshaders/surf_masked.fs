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

  // convert to premultiplied
  vec4 FinalColour_1;
  FinalColour_1.r = TexColour.r*TexColour.a*Light.a;
  FinalColour_1.g = TexColour.g*TexColour.a*Light.a;
  FinalColour_1.b = TexColour.b*TexColour.a*Light.a;
  FinalColour_1.a = TexColour.a;
  $include "common/fog.fs"

  gl_FragColor = FinalColour_1;
}
