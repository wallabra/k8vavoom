#version 120

// fragment shader for simple (non-lightmapped) surfaces

uniform sampler2D Texture;
uniform vec4 Light;
uniform vec4 SplatColour;
uniform bool FogEnabled;
uniform int FogType;
uniform vec4 FogColour;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;
uniform bool IsLightmap;

varying vec2 TextureCoordinate;


void main () {
  vec4 FinalColour_1;
  vec4 TexColour;

  TexColour = (texture2D(Texture, TextureCoordinate)*Light);
  if (TexColour.a < 0.1) discard;

  //TexColour = (texture2D(Texture, gl_TexCoord[0].xy)*Light);
  //FinalColour_1 = TexColour;

  float lumi = 0.2126*TexColour.r+0.7152*TexColour.g+0.0722*TexColour.b*SplatColour.a;
  if (lumi < 0.1) discard;

  FinalColour_1.rgb = SplatColour.rgb;
  FinalColour_1.a = lumi;

  /*
  if (FogEnabled) {
    float FogFactor_3;

    float z = gl_FragCoord.z/gl_FragCoord.w;

    if (FogType == 3) {
      FogFactor_3 = exp2(-FogDensity*FogDensity*z*z*1.442695);
    } else if (FogType == 2) {
      FogFactor_3 = exp2(-FogDensity*z*1.442695);
    } else {
      FogFactor_3 = (FogEnd-z)/(FogEnd-FogStart);
    }

    float ClampFactor = clamp(FogFactor_3, 0.0, 1.0);
    FogFactor_3 = ClampFactor;

    float FogFactor = clamp((ClampFactor-0.1)/0.9, 0.0, 1.0);
    FinalColour_1 = mix(FogColour, TexColour, FogFactor*FogFactor*(3.0-(2.0*FogFactor)));
  }
  */

  gl_FragColor = FinalColour_1;
  /*
  //gl_FragColor.r = gl_TexCoord[0].x;
  //gl_FragColor.g = gl_TexCoord[0].y;
  gl_FragColor.r = TextureCoordinate.x;
  gl_FragColor.g = TextureCoordinate.y;
  gl_FragColor.b = 0;
  gl_FragColor.a = 1;
  */
}
