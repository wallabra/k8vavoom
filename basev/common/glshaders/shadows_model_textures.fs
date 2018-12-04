#version 120

uniform sampler2D Texture;
uniform sampler2D AmbLightTexture;
uniform float InAlpha;
uniform bool AllowTransparency;

varying vec2 TextureCoordinate;
varying vec3 VertToView;
varying vec3 VPos;
varying float PlaneDist;
varying float Dist;
uniform vec2 ScreenSize;


void main () {
  float DistVPos = /*sqrt*/(dot(VPos, VPos));
  if (DistVPos < 0.0) discard;

  float DistToView = /*sqrt*/(dot(VertToView, VertToView));
  if (DistToView < 0.0) discard;

#if 0
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.w < 0.1) discard;

  float ClampTransp = clamp((TexColour.w-0.1)/0.9, 0.0, 1.0);

  if (!AllowTransparency) {
    if (InAlpha == 1.0 && ClampTransp < 0.666) discard;
  } else {
    if (ClampTransp < 0.1) discard;
  }

  vec4 FinalColour_1;
  FinalColour_1.xyz = TexColour.xyz;
  FinalColour_1.w = InAlpha*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));

#else
# if 0
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  float ClampTransp = TexColour.a; //clamp((TexColour.a-0.1)/0.9, 0.0, 1.0);
  TexColour.r *= InAlpha;
  TexColour.g *= InAlpha;
  TexColour.b *= InAlpha;
  TexColour.a *= InAlpha;
  //if (TexColour.a < 0.1) discard;

  if (!AllowTransparency) {
    if (InAlpha == 1.0 && TexColour.a < 0.666) discard;
  } else {
    if (TexColour.a < 0.1) discard;
  }

  vec4 FinalColour_1;
  FinalColour_1.r = TexColour.r*TexColour.a;
  FinalColour_1.g = TexColour.g*TexColour.a;
  FinalColour_1.b = TexColour.b*TexColour.a;
  FinalColour_1.a = TexColour.a;
  FinalColour_1 = TexColour;

  float alpha = InAlpha*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));
  FinalColour_1.a = alpha;

  // sample color from ambient light texture
  vec2 tc2 = gl_FragCoord.xy/ScreenSize.xy;
  vec4 ambColor = texture2D(AmbLightTexture, tc2);
  // Light.a == 1: fullbright
  /*if (Light.a == 0)*/ {
    FinalColour_1.r = clamp(FinalColour_1.r*ambColor.r, 0.0, 1.0);
    FinalColour_1.g = clamp(FinalColour_1.g*ambColor.g, 0.0, 1.0);
    FinalColour_1.b = clamp(FinalColour_1.b*ambColor.b, 0.0, 1.0);
    //FinalColour_1 = ambColor;
    //FinalColour_1.a = 1;
  }

  /*
  float alpha = InAlpha*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));
  alpha = clamp(TexColour.a*alpha, 0, 1);
  if (alpha < 0.01) discard;

  //FinalColour_1.r = TexColour.r*TexColour.a*alpha;
  //FinalColour_1.g = TexColour.g*TexColour.a*alpha;
  //FinalColour_1.b = TexColour.b*TexColour.a*alpha;
  FinalColour_1.r = TexColour.r*alpha;
  FinalColour_1.g = TexColour.g*alpha;
  FinalColour_1.b = TexColour.b*alpha;
  FinalColour_1.a = alpha;
  */
# else
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  float alpha = clamp(TexColour.a*InAlpha, 0, 1);
  //float ClampTransp = clamp((TexColour.w-0.1)/0.9, 0.0, 1.0);
  //float alpha = InAlpha*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));

  /*if (!AllowTransparency) {
    if (InAlpha == 1.0 && alpha < 0.666) discard;
  } else*/ {
    if (alpha < 0.1) discard;
  }

  vec4 FinalColour_1;
  FinalColour_1.r = TexColour.r*alpha;
  FinalColour_1.g = TexColour.g*alpha;
  FinalColour_1.b = TexColour.b*alpha;
  //FinalColour_1 = TexColour;
  FinalColour_1.a = alpha;

  // sample color from ambient light texture
  vec2 tc2 = gl_FragCoord.xy/ScreenSize.xy;
  vec4 ambColor = texture2D(AmbLightTexture, tc2);
  // Light.a == 1: fullbright
  //FinalColour_1 = ambColor;
  //FinalColour_1.a = 0.5;
#if 1
  /*if (Light.a == 0)*/ {
    FinalColour_1.r = clamp(FinalColour_1.r*ambColor.r, 0.0, 1.0);
    FinalColour_1.g = clamp(FinalColour_1.g*ambColor.g, 0.0, 1.0);
    FinalColour_1.b = clamp(FinalColour_1.b*ambColor.b, 0.0, 1.0);
    //FinalColour_1 = ambColor;
    //FinalColour_1.a = 1;
  }
#endif
  //FinalColour_1.a = 0.1;
# endif
#endif

  gl_FragColor = FinalColour_1;
}
