#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform bool AllowTransparency;
uniform float InAlpha;
$include "common/fog_vars.fs"

varying vec3 VertToView;
varying vec3 VPos;
varying vec2 TextureCoordinate;
varying float Dist;


void main () {
  float DistVPos = dot(VPos, VPos); // this is always positive

  if (Dist <= 0.0 && DistVPos > 0.0) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard;

#ifdef VAVOOM_REVERSE_Z
  float z = 1.0/gl_FragCoord.w;
#else
  float z = gl_FragCoord.z/gl_FragCoord.w;
#endif
  float FogFactor = (FogEnd-z)/(FogEnd-FogStart);

  float DistToView = dot(VertToView, VertToView); // this is always positive

  float ClampTrans = clamp(((TexColour.a-0.1)/0.9), 0.0, 1.0);

  // if signs of Dist and DistToView aren't equal, use (1.0-FogFactor)  (-1)
  // if signs of Dist and DistToView are equal, use (0.75-FogFactor)    (0, 1)
  float multr = 1.0-0.25*min(1, 1+sign(Dist));
  FogFactor = clamp(multr-FogFactor, 0.0, multr)*InAlpha;

  vec4 FinalColour_1;
  FinalColour_1.a = (FogFactor*InAlpha)*(ClampTrans*(ClampTrans*(3.0-(2.0*ClampTrans))));

  if (!AllowTransparency) {
    if (InAlpha == 1.0 && FinalColour_1.a < 0.666) discard;
  } else {
    if (FinalColour_1.a < 0.01) discard;
  }

  FinalColour_1.rgb = FogColour.rgb*multr;

  gl_FragColor = FinalColour_1;
}
