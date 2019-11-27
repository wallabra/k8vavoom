#version 120
$include "common/common.inc"

uniform sampler2D Texture;
$include "common/texture_vars.fs"


void main () {
  vec4 TexColor = GetStdTexel(Texture, TextureCoordinate);
#ifdef VV_TEXTURED_MASKED_WALL
  //if (TexColor.a < ALPHA_MIN) discard;
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
#endif

  vec4 FinalColor;
  FinalColor.rgb = TexColor.rgb;

  float ClampTransp = clamp((TexColor.a-0.1)/0.9, 0.0, 1.0);
  FinalColor.a = TexColor.a*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));
#ifdef VV_TEXTURED_MASKED_WALL
  //if (FinalColor.a < ALPHA_MIN) discard;
#endif

  gl_FragColor = FinalColor;
}
