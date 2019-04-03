#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform vec3 LightColour;
uniform float LightRadius;
uniform float LightMin;
uniform float InAlpha;
uniform bool AllowTransparency;
#ifdef VV_SPOTLIGHT
uniform vec3 ConeDirection;
uniform float ConeAngle; // degrees
#endif

varying vec3 Normal;
varying vec3 VertToLight;
varying vec3 VertToView;
varying vec3 VPos;
varying vec3 VPosL;
varying vec2 TextureCoordinate;
varying float Dist;
varying float VDist;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  //if (TexColour.a < 0.01) discard;
  TexColour.a *= InAlpha;
  if (!AllowTransparency) {
    if (TexColour.a < 0.666) discard;
  } else {
    if (TexColour.a < 0.01) discard;
  }

  float DistVPosL = dot(VPosL, VPosL);
  float DistVPos = dot(VPos, VPos);

  //-2, -1, 0
  float distSign1 = min(-1.0, sign(Dist)-1.0);
  // =0: DistVPosL < -LightRadius
  // <0: DistVPosL > LightRadius, or -DistVPosL < -LightRadius
  //
  // =0: DistVPosL < -LightRadius, or -DistVPosL > LightRadius
  // <0: DistVPosL > LightRadius
  //
  //  0: -1
  // -1:  1
  float dsmul = -1.0-distSign1*2.0;

  //3 < -5  -3 > 5
  //-5 < -3  5 > 3

  if (sign(Dist)*sign(DistVPos) < 0.0 || dsmul*DistVPosL > LightRadius*LightRadius) discard;
  /*
  if (Dist > 0.0) {
    if (DistVPos < 0.0) discard;
    if (DistVPosL < -LightRadius) discard;
  } else {
    if (DistVPos > 0.0) discard;
    if (DistVPosL > LightRadius) discard;
  }
  */


  float DistToView = dot(VertToView, VertToView);
  if (sign(Dist)*sign(VDist) < 0.0 || sign(Dist)*sign(DistToView) < 0.0) discard;
  /*
  if (Dist > 0.0) {
    if (VDist < 0.0) discard;
    if (DistToView < 0.0) discard;
  } else {
    if (VDist > 0.0) discard;
    if (DistToView > 0.0) discard;
  }
  */

  float DistToLight = dot(VertToLight, VertToLight);

  //  1:  1
  //  0:  1
  // -1: -1

  //  1:  1
  //  0: -1

  float distSign2 = min(1.0, sign(Dist)+1.0);
  float dsmul2 = -1.0-distSign2*2.0;
  if (dsmul2*DistToLight > LightRadius) discard;

  /*
  if (Dist > 0.0) {
    if (DistToLight > LightRadius) discard;
  } else {
    if (DistToLight < -LightRadius) discard;
  }
  */


  float attenuation = (LightRadius-DistToLight-LightMin)*(0.5+(0.5*dot(normalize(VertToLight), Normal)));

#ifdef VV_SPOTLIGHT
  // cone restrictions (affects attenuation)
  vec3 surfaceToLight = normalize(VertToLight);
  float distanceToLight = length(VertToLight);

  //vec3 ConeDirection = vec3(1.0, 0.0, 0.0);
  //float ConeAngle = 50.0;

  //float lightToSurfaceAngle = degrees(acos(dot(-surfaceToLight, normalize(ConeDirection))));
  float lightToSurfaceAngle = degrees(acos(dot(-surfaceToLight, ConeDirection)));
  if (lightToSurfaceAngle > ConeAngle) attenuation = 0.0;
#endif

  if (attenuation <= 0.0) discard;

  float ClampAdd = min(attenuation/255.0, 1.0);
  attenuation = ClampAdd;

  float ClampTrans = clamp((TexColour.a-0.1)/0.9, 0.0, 1.0);
  if (ClampTrans < 0.01) discard;

  /*
  if (!AllowTransparency) {
    if (InAlpha == 1.0 && ClampTrans < 0.666) discard;
  } else {
    if (ClampTrans < 0.01) discard;
  }
  */

  vec4 FinalColour_1;
  FinalColour_1.rgb = LightColour;
  FinalColour_1.a = (ClampAdd*TexColour.a)*(ClampTrans*(ClampTrans*(3.0-(2.0*ClampTrans))));
  if (FinalColour_1.a < 0.01) discard;

  gl_FragColor = FinalColour_1;
}
