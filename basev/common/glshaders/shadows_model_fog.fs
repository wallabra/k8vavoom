#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform bool AllowTransparency;
uniform float InAlpha;
$include "common/fog_vars.fs"

varying vec2 TextureCoordinate;
varying float Dist;


void main () {
  //if (Dist <= 0.0) discard; // wtf?!

  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  if (!AllowTransparency) {
    if (TexColor.a < 0.666) discard;
  } else {
    if (TexColor.a < 0.01) discard;
  }

  vec4 FinalColor = TexColor;
#ifdef VAVOOM_UNUSED
#ifdef VAVOOM_REVERSE_Z
  float z = 1.0/gl_FragCoord.w;
#else
  float z = gl_FragCoord.z/gl_FragCoord.w;
#endif
  float FogFactor = (FogEnd-z)/(FogEnd-FogStart);

  float ClampTrans = clamp(((TexColor.a-0.1)/0.9), 0.0, 1.0);

  float multr = 1.0-0.25*min(1, 1+sign(Dist));
  FogFactor = clamp(multr-FogFactor, 0.0, multr)*InAlpha;

  FinalColor.a = (FogFactor*InAlpha)*(ClampTrans*(ClampTrans*(3.0-(2.0*ClampTrans))));
  if (FinalColor.a < 0.01) discard;

  /*
  if (!AllowTransparency) {
    //if (InAlpha == 1.0 && FinalColor.a < 0.666) discard;
    if (TexColor.a < 0.666) discard;
  } else {
    if (FinalColor.a < 0.01) discard;
  }
  */

  FinalColor.rgb = FogColor.rgb*multr;
  //FinalColor.rgb = vec3(1, 0, 0);
#endif

  $include "common/fog_calc.fs"

  gl_FragColor = FinalColor;
}
