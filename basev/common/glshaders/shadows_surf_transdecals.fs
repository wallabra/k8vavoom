#version 120
$include "common/common.inc"

uniform sampler2D Texture;
$include "common/texture_vars.fs"


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  //if (TexColor.a < 0.01) discard;
  if (TexColor.a < 0.666) discard; //FIXME: only normal and masked walls should go thru this
  // this doesn't matter
  gl_FragColor = TexColor;
}
