#version 120
$include "common/common.inc"

uniform vec4 Light;

uniform sampler2D Texture;
$include "common/texture_vars.fs"

#ifdef VV_AMBIENT_BRIGHTMAP_WALL
$include "common/brightmap_vars.fs"
#endif

$include "common/glow_vars.fs"


void main () {
  vec4 lt = calcGlow(Light);
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  if (TexColor.a <= 0.01) discard;
#ifdef VV_AMBIENT_BRIGHTMAP_WALL
  $include "common/brightmap_calc.fs"
#endif
  float lta = lt.a;
  //lt.rgb = TexColor.rgb*lt.a;
  /*
  lt.r = clamp(lt.r*TexColor.r*lta, 0.0, 1.0);
  lt.g = clamp(lt.g*TexColor.g*lta, 0.0, 1.0);
  lt.b = clamp(lt.b*TexColor.b*lta, 0.0, 1.0);
  */
  //lt.rgb = clamp(lt.rgb*TexColor.rgb*lta, 0.0, 1.0);
  lt.rgb = clamp(lt.rgb*TexColor.rgb*lta, 0.0, 1.0);
  lt.a = lta;
  //lt.rgb = vec3(1, 0, 0);
  //lt.a = 1.0;
  //lt.a = clamp(lta+0.7, 0.0, 1.0);
  //lt.a = 1.0;

  gl_FragColor = lt;
}
