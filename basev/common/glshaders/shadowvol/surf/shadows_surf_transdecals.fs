#version 120
$include "common/common.inc"

uniform sampler2D Texture;
$include "common/texture_vars.fs"


void main () {
  vec4 TexColor = GetStdTexelSimpleShade(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard;
  if (TexColor.a < ALPHA_MASKED) discard; //FIXME: only normal and masked walls should go thru this
  // this doesn't matter
  gl_FragColor = TexColor;
}
