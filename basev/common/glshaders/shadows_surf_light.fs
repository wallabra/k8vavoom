#version 120

uniform vec3 LightColour;
uniform float LightRadius;
uniform sampler2D Texture;

varying vec3 Normal;
varying vec3 VertToLight;
varying vec3 VertToView;
varying float Dist;
varying float VDist;
varying vec2 TextureCoordinate;


void main () {
  if (VDist <= 0.0 || Dist <= 0.0) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  //if (TexColour.a < 0.1) discard; //FIXME
  if (TexColour.a < 0.666) discard; //FIXME: only normal and masked walls should go thru this

  float DistToView = dot(VertToView, VertToView);
  if (DistToView <= 0.0) discard;

  float DistToLight = dot(VertToLight, VertToLight);
  if (DistToLight <= 0.0) discard;

  DistToLight = sqrt(DistToLight);

  if (DistToLight > LightRadius) discard;

  float Add_1 = (LightRadius-DistToLight)*(0.5+(0.5*dot(normalize(VertToLight), Normal)));
  if (Add_1 <= 0.0) discard;

  float ClampAdd = clamp(Add_1/255.0, 0.0, 1.0);
  Add_1 = ClampAdd;

  float Transp = clamp((TexColour.a-0.1)/0.9, 0.0, 1.0);

  vec4 FinalColour_1;
  FinalColour_1.rgb = LightColour;
  FinalColour_1.a = ClampAdd*(Transp*(Transp*(3.0-(2.0*Transp))));
  if (FinalColour_1.a < 0.01) discard;

  gl_FragColor = FinalColour_1;
}
