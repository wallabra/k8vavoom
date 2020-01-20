#version 120
$include "common/common.inc"

uniform sampler2D Texture;
$include "common/texture_vars.fs"


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  if (TexColor.a < 0.666) discard;
  gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
}
