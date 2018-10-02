#version 120

// fragment shader for simple (non-lightmapped) surfaces

uniform sampler2D Texture;
uniform vec4 Light;
uniform bool FogEnabled;
uniform int FogType;
uniform vec4 FogColour;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate)*Light;
  if (TexColour.a < 0.01) discard;

  vec4 FinalColour_1 = TexColour;
  $include "common_fog.fs"

  gl_FragColor = FinalColour_1;
}
