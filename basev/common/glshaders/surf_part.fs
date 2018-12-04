#version 120

varying vec4 Light;
varying vec2 TextureCoordinate;
uniform int SmoothParticle;


void main () {
  float Transp;

  if (SmoothParticle == 0) {
    if (TextureCoordinate.x <= -0.5 || TextureCoordinate.x >= 0.5) discard;
    if (TextureCoordinate.y <= -0.5 || TextureCoordinate.y >= 0.5) discard;
    Transp = 0.9;
  } else {
    float a = clamp(((1.0-sqrt(dot(TextureCoordinate, TextureCoordinate)))*2.0), 0.0, 1.0);
    if (a < 0.1) discard;
    Transp = clamp(((a-0.1)/0.9), 0.0, 1.0);
  }

  gl_FragColor = Light*Transp*Transp*(3.0-2.0*Transp);
}
