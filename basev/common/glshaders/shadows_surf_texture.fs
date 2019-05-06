#version 120
$include "common/common.inc"

uniform sampler2D Texture;

$include "common/texture_vars.fs"


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
#ifdef VV_TEXTURED_MASKED_WALL
  //if (TexColor.a < 0.01) discard;
  if (TexColor.a < 0.666) discard; //FIXME: only normal and masked walls should go thru this
#endif

  vec4 FinalColor;
  FinalColor.rgb = TexColor.rgb;

  float ClampTransp = clamp((TexColor.a-0.1)/0.9, 0.0, 1.0);
  FinalColor.a = TexColor.a*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));
#ifdef VV_TEXTURED_MASKED_WALL
  //if (FinalColor.a < 0.01) discard;
#endif

  gl_FragColor = FinalColor;
}
