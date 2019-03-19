#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform sampler2D AmbLightTexture;
uniform float InAlpha;
uniform bool AllowTransparency;

varying vec2 TextureCoordinate;
varying vec3 VertToView;
varying vec3 VPos;
//!varying float PlaneDist;
//!varying float Dist;
uniform vec2 ScreenSize;


void main () {
  float DistVPos = dot(VPos, VPos);
  if (DistVPos < 0.0) discard;

  float DistToView = dot(VertToView, VertToView);
  if (DistToView < 0.0) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard;

  float alpha = clamp(TexColour.a*InAlpha, 0, 1);
  //float ClampTransp = clamp((TexColour.a-0.1)/0.9, 0.0, 1.0);
  //float alpha = InAlpha*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));

  /*if (!AllowTransparency) {
    if (InAlpha == 1.0 && alpha < 0.666) discard;
  } else*/ {
    if (alpha < 0.01) discard;
  }

  vec4 FinalColour_1;
  FinalColour_1.r = TexColour.r*alpha;
  FinalColour_1.g = TexColour.g*alpha;
  FinalColour_1.b = TexColour.b*alpha;
  FinalColour_1.a = alpha;

  // sample color from ambient light texture
  vec2 tc2 = gl_FragCoord.xy/ScreenSize.xy;
  vec4 ambColor = texture2D(AmbLightTexture, tc2);

  // Light.a == 1: fullbright
  // k8: oops, no way to do it yet (why?)
  /*if (Light.a == 0)*/ {
    FinalColour_1.r = clamp(FinalColour_1.r*ambColor.r, 0.0, 1.0);
    FinalColour_1.g = clamp(FinalColour_1.g*ambColor.g, 0.0, 1.0);
    FinalColour_1.b = clamp(FinalColour_1.b*ambColor.b, 0.0, 1.0);
    //FinalColour_1 = ambColor;
    //FinalColour_1.a = 1;
  }

  gl_FragColor = FinalColour_1;
}
