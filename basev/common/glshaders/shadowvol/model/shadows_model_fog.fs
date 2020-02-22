#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform bool AllowTransparency;
//uniform float InAlpha;

#define VAVOOM_SIMPLE_ALPHA_FOG
$include "common/fog_vars.fs"

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  if (!AllowTransparency) {
    if (TexColor.a < ALPHA_MASKED) discard;
  } else {
    if (TexColor.a < ALPHA_MIN) discard;
    float newa = TexColor.a*FogColor.a;
    if (newa < ALPHA_MIN) discard;
  }

  vec4 FinalColor;
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColor;
}
