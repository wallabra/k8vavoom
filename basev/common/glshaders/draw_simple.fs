#version 120

uniform sampler2D Texture;
uniform float Alpha;

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard;

  //vec4 FinalColour = TexColour;
  // premultiply
  vec4 FinalColour_1;
  FinalColour_1.r = TexColour.r*Alpha;
  FinalColour_1.g = TexColour.g*Alpha;
  FinalColour_1.b = TexColour.b*Alpha;
  FinalColour_1.a = TexColour.a*Alpha;

  //!float Transp = clamp(((Alpha-0.4)/0.6), 0.0, 1.0);
  //!FinalColour_1.a = TexColour.a*(Transp*(Transp*(3.0-(2.0*Transp))));

  gl_FragColor = FinalColour_1;
}
