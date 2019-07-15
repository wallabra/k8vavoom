varying vec4 Light;
varying vec2 TextureCoordinate;
//uniform int SmoothParticle;


void main () {
#ifdef PARTICLES_SMOOTH
  float a = clamp(((1.0-sqrt(dot(TextureCoordinate, TextureCoordinate)))*2.0), 0.0, 1.0);
  float Transp = clamp(((a-0.1)/0.9), 0.0, 1.0);
  if (Transp <= 0.0) discard;
#else
  if (TextureCoordinate.x <= -0.5 || TextureCoordinate.x >= 0.5) discard;
  if (TextureCoordinate.y <= -0.5 || TextureCoordinate.y >= 0.5) discard;
  const float Transp = 0.9;
#endif

  gl_FragColor = Light*Transp*Transp*(3.0-2.0*Transp);
}
