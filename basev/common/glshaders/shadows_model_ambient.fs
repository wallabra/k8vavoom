#version 120
$include "common/common.inc"

uniform vec4 Light;
uniform sampler2D Texture;
uniform float InAlpha;
uniform bool AllowTransparency;

varying vec2 TextureCoordinate;
varying float Dist;


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  if (TexColor.a < ALPHA_MIN) discard;

  float ClampTransp = clamp(((Light.a*TexColor.a)-0.1)/0.9, 0.0, 1.0);

  if (!AllowTransparency) {
    //if (InAlpha == 1.0 && ClampTransp < ALPHA_MASKED) discard;
    if (TexColor.a < ALPHA_MASKED) discard;
  } else {
    if (ClampTransp < ALPHA_MIN) discard;
  }

  vec4 FinalColor;
  FinalColor.rgb = Light.rgb*(1.0-0.25*max(0, -sign(Dist)));

  /*
  // `DistToView` is always positive (or zero)
  float DistToView = dot(VertToView, VertToView);
  if ((Dist >= 0.0 && DistToView < 0.0) || (Dist < 0.0 && DistToView > 0.0)) {
    FinalColor.xyz = Light.xyz*0.75;
  } else {
    FinalColor.xyz = Light.xyz;
  }
  */

  FinalColor.a = InAlpha*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));
  if (FinalColor.a < ALPHA_MIN) discard;

  gl_FragColor = FinalColor;
}
