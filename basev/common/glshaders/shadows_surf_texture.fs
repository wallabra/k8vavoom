#version 120
$include "common/common.inc"

uniform sampler2D Texture;

$include "common/texture_vars.fs"


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
#ifdef VV_TEXTURED_MASKED_WALL
  //if (TexColour.a < 0.01) discard;
  if (TexColour.a < 0.666) discard; //FIXME: only normal and masked walls should go thru this
#endif

  vec4 FinalColour_1;
  FinalColour_1.rgb = TexColour.rgb;

  float ClampTransp = clamp((TexColour.a-0.1)/0.9, 0.0, 1.0);
  FinalColour_1.a = TexColour.a*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));
#ifdef VV_TEXTURED_MASKED_WALL
  //if (FinalColour_1.a < 0.01) discard;
#endif

  gl_FragColor = FinalColour_1;
}
