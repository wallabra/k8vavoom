#version 120
$include "common/common.inc"

// fragment shader for simple (non-lightmapped) surfaces

uniform sampler2D Texture;
#ifdef VV_SIMPLE_BRIGHTMAP
$include "common/brightmap_vars.fs"
#endif
uniform vec4 Light;

$include "common/fog_vars.fs"
$include "common/texture_vars.fs"

//#ifdef VV_AMBIENT_GLOW
$include "common/glow_vars.fs"
//#endif


void main () {
  //vec4 TexColour = texture2D(Texture, TextureCoordinate)*Light;
  //if (TexColour.a < 0.01) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard; // for steamlined textures //FIXME

  vec4 lt = calcGlow(Light);
#ifdef VV_SIMPLE_BRIGHTMAP
  $include "common/brightmap_calc.fs"
#endif
  TexColour *= lt;

  vec4 FinalColour_1 = TexColour;
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColour_1;
}
