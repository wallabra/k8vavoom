#version 120

uniform sampler2D Texture;

$include "common/texture_vars.fs"


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  //if (TexColour.a < 0.01) discard;
  if (TexColour.a < 0.666) discard; //FIXME: only normal and masked walls should go thru this

  vec4 FinalColour_1;
  FinalColour_1.rgb = TexColour.rgb;

  float ClampTransp = clamp((TexColour.a-0.1)/0.9, 0.0, 1.0);
  FinalColour_1.a = TexColour.a*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));
  if (FinalColour_1.a < 0.01) discard;

  gl_FragColor = FinalColour_1;
}
