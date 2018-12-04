#version 120

uniform vec4 FogColour;
uniform sampler2D Texture;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;
uniform int FogType;
uniform bool AllowTransparency;

varying vec3 VertToView;
varying vec3 VPos;
varying vec2 TextureCoordinate;
varying float Dist;
uniform float InAlpha;


void main () {
  float DistVPos = /*sqrt*/(dot(VPos, VPos));

  if (sign(Dist)*sign(DistVPos) < 0) discard;
  /*
  if (Dist > 0.0) {
    if (DistVPos < 0.0) discard;
  } else {
    if (DistVPos > 0.0) discard;
  }
  */

  vec4 TexColour = texture2D (Texture, TextureCoordinate);
  if (TexColour.w < 0.1) discard;

#ifdef VAVOOM_REVERSE_Z
  float z = 1.0/gl_FragCoord.w;
#else
  float z = gl_FragCoord.z/gl_FragCoord.w;
#endif

  float FogFactor;
  if (FogType == 3) {
    FogFactor = exp2((((-FogDensity*FogDensity)*z)*z)*1.442695);
  } else if (FogType == 2) {
    FogFactor = exp2((-FogDensity*z)*1.442695);
  } else {
    FogFactor = (FogEnd-z)/(FogEnd-FogStart);
  }

  float DistToView = /*sqrt*/(dot(VertToView, VertToView));

  float ClampTrans = clamp(((TexColour.w-0.1)/0.9), 0.0, 1.0);

  // if signs of Dist and DistToView aren't equal, use (1.0-FogFactor)  (-1)
  // if signs of Dist and DistToView are equal, use (0.75-FogFactor)    (0, 1)
  float multr = 1.0-0.25*min(1, 1+(sign(Dist)*sign(DistToView)));
  FogFactor = clamp(multr-FogFactor, 0.0, multr)*InAlpha;

  vec4 FinalColour;
  FinalColour.w = (FogFactor*InAlpha)*(ClampTrans*(ClampTrans*(3.0-(2.0*ClampTrans))));

  if (!AllowTransparency) {
    if (InAlpha == 1.0 && FinalColour.w < 0.666) discard;
  } else {
    if (FinalColour.w < 0.1) discard;
  }

  FinalColour.xyz = FogColour.xyz*multr;

  /*
  if ((Dist >= 0.0))
  {
    if ((DistToView <= 0.0))
    {
      vec4 DarkColour;

      FogFactor = (clamp ((1.0 - FogFactor), 0.0, 1.0) * InAlpha);
      float ClampTrans;

      ClampTrans = clamp (((TexColour.w - 0.1) / 0.9), 0.0, 1.0);
      DarkColour.xyz = FogColour.xyz;
      DarkColour.w = ((FogFactor * InAlpha) * (ClampTrans * (ClampTrans * (3.0 - (2.0 * ClampTrans))
      )));
      FinalColour = DarkColour;
    }
    else
    {
      vec4 BrightColour;

      FogFactor = (clamp ((0.75 - FogFactor), 0.0, 0.75) * InAlpha);
      float ClampTrans;

      ClampTrans = clamp (((TexColour.w - 0.1) / 0.9), 0.0, 1.0);
      BrightColour.xyz = (FogColour.xyz * 0.75);
      BrightColour.w = ((FogFactor * InAlpha) * (ClampTrans * (ClampTrans * (3.0 - (2.0 * ClampTrans))
      )));
      FinalColour = BrightColour;
    };
  }
  else
  {
    if ((DistToView >= 0.0))
    {
      vec4 DarkColour;

      FogFactor = (clamp ((1.0 - FogFactor), 0.0, 1.0) * InAlpha);
      float ClampTrans;

      ClampTrans = clamp (((TexColour.w - 0.1) / 0.9), 0.0, 1.0);
      DarkColour.xyz = FogColour.xyz;
      DarkColour.w = ((FogFactor * InAlpha) * (ClampTrans * (ClampTrans *
      (3.0 - (2.0 * ClampTrans))
      )));
      FinalColour = DarkColour;
    }
    else
    {
      vec4 BrighColour;

      FogFactor = (clamp ((0.75 - FogFactor), 0.0, 0.75) * InAlpha);
      float ClampTrans;

      ClampTrans = clamp (((TexColour.w - 0.1) / 0.9), 0.0, 1.0);
      BrighColour.xyz = (FogColour.xyz * 0.75);
      BrighColour.w = ((FogFactor * InAlpha) * (ClampTrans * (ClampTrans *
      (3.0 - (2.0 * ClampTrans))
      )));
      FinalColour = BrighColour;
    };
  };

  if ((AllowTransparency == bool(0)))
  {
    if (((InAlpha == 1.0) && (FinalColour.w < 0.666)))
    {
      discard;
    };
  }
  else
  {
    if ((FinalColour.w < 0.1))
    {
      discard;
    };
  };
  */

  gl_FragColor = FinalColour;
}
