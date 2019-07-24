#version 120
$include "common/common.inc"

uniform vec4 Light;

uniform sampler2D Texture;
$include "common/texture_vars.fs"

#ifdef VV_AMBIENT_BRIGHTMAP_WALL
$include "common/brightmap_vars.fs"
#endif

$include "common/glow_vars.fs"


void main () {
  vec4 lt = calcGlow(Light);
  vec4 TexColor = GetStdTexel(Texture, TextureCoordinate);
  if (TexColor.a <= ALPHA_MIN) discard;
#ifdef VV_AMBIENT_BRIGHTMAP_WALL
  $include "common/brightmap_calc.fs"
#endif

  TexColor.rgb *= lt.rgb;
  // convert to premultiplied
  vec4 FinalColor;
  FinalColor.rgb = (TexColor.rgb*TexColor.a)*lt.a;
  FinalColor.a = TexColor.a*lt.a;

  gl_FragColor = clamp(FinalColor, 0.0, 1.0);
}
