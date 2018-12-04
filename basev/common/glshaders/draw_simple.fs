#version 120

uniform sampler2D Texture;
uniform float Alpha;

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard;

  // we got a non-premultiplied color, convert it
  vec4 FinalColour_1;
  FinalColour_1.a = TexColour.a*clamp(Alpha, 0.0, 1.0);
  FinalColour_1.r = TexColour.r*FinalColour_1.a;
  FinalColour_1.g = TexColour.g*FinalColour_1.a;
  FinalColour_1.b = TexColour.b*FinalColour_1.a;
  if (FinalColour_1.a < 0.01) discard;
  gl_FragColor = FinalColour_1;
}
