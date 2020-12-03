#ifdef VV_SMAP_TEXTURED
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard; //FIXME
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
#endif
  vec4 fc = vec4(0.0, 0.0, 0.0, 1.0);
  float dist = distance(LightPos, VertWorldPos)+2;
  if (dist >= LightRadius) {
    fc.r = 99999.0;
    fc.b = 1.0;
  } else {
    fc.r = dist/LightRadius;
  }
  gl_FragColor = fc;
