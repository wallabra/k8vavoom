#version 120
$include "common/common.inc"

uniform sampler2D Texture;
//uniform sampler2D AmbLightTexture;
uniform float InAlpha;
uniform bool AllowTransparency;
//uniform vec2 ScreenSize;
#ifdef VV_STENCIL
uniform vec3 StencilColor;
#endif

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard;
  if (!AllowTransparency) {
    if (TexColor.a < ALPHA_MASKED) discard;
  } else {
    if (TexColor.a < ALPHA_MIN) discard;
  }

  float alpha = clamp(TexColor.a*InAlpha, 0.0, 1.0);
  if (alpha < ALPHA_MIN) discard;

  vec4 FinalColor;
  FinalColor.a = alpha;
#ifdef VV_STENCIL
  FinalColor.rgb = StencilColor.rgb; // *alpha
  //FIXME: for now, skip ambient light for translucent texels
  /*
  if (alpha >= 0.9) {
    // sample color from ambient light texture
    vec2 tc2 = gl_FragCoord.xy/ScreenSize.xy;
    vec3 ambColor = texture2D(AmbLightTexture, tc2).rgb;

    // Light.a == 1: fullbright
    // k8: oops, no way to do it yet (why?)
    FinalColor.rgb = clamp(FinalColor.rgb*ambColor.rgb, 0.0, 1.0);
  }
  */
#else
  /*
  FinalColor.rgb = TexColor.rgb*alpha;

  // sample color from ambient light texture
  vec2 tc2 = gl_FragCoord.xy/ScreenSize.xy;
  vec3 ambColor = texture2D(AmbLightTexture, tc2).rgb;

  // Light.a == 1: fullbright
  // k8: oops, no way to do it yet (why?)
  FinalColor.rgb = clamp(FinalColor.rgb*ambColor.rgb, 0.0, 1.0);
  */

  FinalColor.rgb = TexColor.rgb;

  float ClampTransp = clamp((TexColor.a-0.1)/0.9, 0.0, 1.0);
  FinalColor.a = TexColor.a*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))))*InAlpha;
#endif

  gl_FragColor = FinalColor;
}
