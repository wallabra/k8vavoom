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

  if (TexColor.a*InAlpha < ALPHA_MIN) discard;

  vec4 FinalColor;

#ifdef VV_STENCIL
  FinalColor.rgb = StencilColor.rgb;
#else
  FinalColor.rgb = TexColor.rgb;
#endif

  float ClampTransp = clamp((TexColor.a-0.1)/0.9, 0.0, 1.0);
  FinalColor.a = TexColor.a*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))))*InAlpha;

  gl_FragColor = FinalColor;
}
