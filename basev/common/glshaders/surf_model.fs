#version 120
$include "common/common.inc"

uniform sampler2D Texture;
$include "common/fog_vars.fs"
uniform float InAlpha;
uniform bool AllowTransparency;

varying vec4 Light;
varying vec2 TextureCoordinate;


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  //if (TexColor.a < 0.01) discard;
  if (!AllowTransparency) {
    if (TexColor.a < 0.666) discard;
  } else {
    if (TexColor.a < 0.01) discard;
  }
  TexColor *= Light;

  vec4 FinalColor = TexColor;

  // do fog before premultiply, otherwise it is wrong
  $include "common/fog_calc.fs"

  // convert to premultiplied
  FinalColor.rgb *= FinalColor.a;
  FinalColor.a *= InAlpha;
  if (FinalColor.a < 0.01) discard;
  FinalColor = min(FinalColor, 1.0);

  gl_FragColor = FinalColor;
}
