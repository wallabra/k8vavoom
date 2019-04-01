#version 120
$include "common/common.inc"

uniform vec4 Light;

#ifdef VV_AMBIENT_MASKED_WALL
uniform sampler2D Texture;
$include "common/texture_vars.fs"
#endif

void main () {
#ifdef VV_AMBIENT_MASKED_WALL
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  //if (TexColour.a <= 0.01) discard;
  if (TexColour.a < 0.666) discard; //FIXME: only normal and masked walls should go thru this
#endif
  gl_FragColor = Light;
}
