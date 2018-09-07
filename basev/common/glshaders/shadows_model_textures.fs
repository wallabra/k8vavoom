#version 120

uniform sampler2D Texture;
uniform float InAlpha;
uniform bool AllowTransparency;

varying vec2 TextureCoordinate;
varying vec3 VertToView;
varying vec3 VPos;
varying float PlaneDist;
varying float Dist;

void main () {
  float DistVPos = /*sqrt*/(dot(VPos, VPos));
  if (DistVPos < 0.0) discard;

  float DistToView = /*sqrt*/(dot(VertToView, VertToView));
  if (DistToView < 0.0) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.w < 0.1) discard;

  float ClampTransp = clamp((TexColour.w-0.1)/0.9, 0.0, 1.0);

  if (!AllowTransparency) {
    if (InAlpha == 1.0 && ClampTransp < 0.666) discard;
  } else {
    if (ClampTransp < 0.1) discard;
  }

  vec4 FinalColour;
  FinalColour.xyz = TexColour.xyz;
  FinalColour.w = InAlpha*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));

  gl_FragColor = FinalColour;
}
