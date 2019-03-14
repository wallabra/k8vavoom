#version 120

// fragment shader for simple (non-lightmapped) surfaces

uniform sampler2D Texture;
uniform vec4 Light;

$include "common/fog_vars.fs"
$include "common/texture_vars.fs"


void main () {
  //vec4 TexColour = texture2D(Texture, TextureCoordinate)*Light;
  //if (TexColour.a < 0.01) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard; // for steamlined textures //FIXME
  TexColour *= Light;

  vec4 FinalColour_1 = TexColour;
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColour_1;
}
