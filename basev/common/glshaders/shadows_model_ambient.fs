#version 120

uniform vec4 Light;
uniform sampler2D Texture;
uniform float InAlpha;
uniform bool AllowTransparency;

varying vec2 TextureCoordinate;
varying vec3 VertToView;
varying vec3 VPos;
varying float Dist;


void main () {
  float DistVPos = dot(VPos, VPos);
  if (Dist > 0.0 && DistVPos < 0.0) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard;

  float ClampTransp = clamp(((Light.a*TexColour.a)-0.1)/0.9, 0.0, 1.0);

  if (!AllowTransparency) {
    if (InAlpha == 1.0 && ClampTransp < 0.666) discard;
  } else {
    if (ClampTransp < 0.01) discard;
  }

  float DistToView = dot(VertToView, VertToView);

  vec4 FinalColour_1;

  // sign(1)*sign(-1): -1  OK
  // sign(-1)*sign(1): -1  OK
  // sign(1)*sign(1): 1    OK
  // sign(-1)*sign(-1): 1  OK
  // sign(0)*sign(1): 0    ahem...
  //
  // -1: mul by 0.75
  // other: mul by 1
  //
  // negate: OK is 1, <= 0: not ok
  // take max of (0, negated)

  FinalColour_1.rgb = Light.rgb*(1.0-0.25*max(0, -(sign(Dist)*sign(DistToView))));

  /*
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
