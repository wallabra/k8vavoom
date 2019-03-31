#version 120
$include "common/common.inc"

uniform vec4 Light;
uniform sampler2D Texture;
uniform float InAlpha;
uniform bool AllowTransparency;

varying vec2 TextureCoordinate;
varying float Dist;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard;

  float ClampTransp = clamp(((Light.a*TexColour.a)-0.1)/0.9, 0.0, 1.0);

  if (!AllowTransparency) {
    //if (InAlpha == 1.0 && ClampTransp < 0.666) discard;
    if (TexColour.a < 0.666) discard;
  } else {
    if (ClampTransp < 0.01) discard;
  }

  vec4 FinalColour_1;
  FinalColour_1.rgb = Light.rgb*(1.0-0.25*max(0, -sign(Dist)));

  /*
  // `DistToView` is always positive (or zero)
  float DistToView = dot(VertToView, VertToView);
  if ((Dist >= 0.0 && DistToView < 0.0) || (Dist < 0.0 && DistToView > 0.0)) {
    FinalColour.xyz = Light.xyz*0.75;
  } else {
    FinalColour.xyz = Light.xyz;
  }
  */

  FinalColour_1.a = InAlpha*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));
  if (FinalColour_1.a < 0.01) discard;

  gl_FragColor = FinalColour_1;
}
