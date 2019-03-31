#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform bool AllowTransparency;
uniform float InAlpha;
$include "common/fog_vars.fs"

varying vec2 TextureCoordinate;
varying float Dist;


void main () {
  if (Dist <= 0.0) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (!AllowTransparency) {
    if (TexColour.a < 0.666) discard;
  } else {
    if (TexColour.a < 0.01) discard;
  }

#ifdef VAVOOM_REVERSE_Z
  float z = 1.0/gl_FragCoord.w;
#else
  float z = gl_FragCoord.z/gl_FragCoord.w;
#endif
  float FogFactor = (FogEnd-z)/(FogEnd-FogStart);

  float ClampTrans = clamp(((TexColour.a-0.1)/0.9), 0.0, 1.0);

  float multr = 1.0-0.25*min(1, 1+sign(Dist));
  FogFactor = clamp(multr-FogFactor, 0.0, multr)*InAlpha;

  vec4 FinalColour_1;
  FinalColour_1.a = (FogFactor*InAlpha)*(ClampTrans*(ClampTrans*(3.0-(2.0*ClampTrans))));
  if (FinalColour_1.a < 0.01) discard;

  /*
  if (!AllowTransparency) {
    //if (InAlpha == 1.0 && FinalColour_1.a < 0.666) discard;
    if (TexColour.a < 0.666) discard;
  } else {
    if (FinalColour_1.a < 0.01) discard;
  }
  */

  FinalColour_1.rgb = FogColour.rgb*multr;

  gl_FragColor = FinalColour_1;
}
