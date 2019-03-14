#version 120

uniform vec4 Light;
uniform sampler2D Texture;

$include "common/texture_vars.fs"


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  //if (TexColour.a <= 0.01) discard;
  if (TexColour.a < 0.666) discard;
  gl_FragColor = Light;
}
