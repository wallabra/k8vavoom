#version 120
$include "common/common.inc"

// fragment shader for simple (non-lightmapped) surfaces

uniform sampler2D Texture;
#ifdef VV_SIMPLE_BRIGHTMAP
uniform sampler2D TextureBM;
#endif
uniform vec4 Light;

$include "common/fog_vars.fs"
$include "common/texture_vars.fs"


void main () {
  //vec4 TexColour = texture2D(Texture, TextureCoordinate)*Light;
  //if (TexColour.a < 0.01) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard; // for steamlined textures //FIXME

  vec4 lt = Light;
#ifdef VV_SIMPLE_BRIGHTMAP
  vec4 BMColor = texture2D(TextureBM, TextureCoordinate);
  lt.rgb = max(lt.rgb, BMColor.rgb);
  //lt.rgb = BMColor.rgb;
#endif
  TexColour *= lt;

  vec4 FinalColour_1 = TexColour;
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColour_1;
}
