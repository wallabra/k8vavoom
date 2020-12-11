$include "shadowvol/smap_common_defines.inc"

#ifdef VV_SMAP_TEXTURED
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard; //FIXME
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
#endif
  float dist = length(VertToLight);
  #ifdef VV_SMAP_BLUR4
  dist += 1.2;
  #else
  //dist += 0.8;
  #endif
  if (dist >= LightRadius) discard;
  gl_FragColor.r = dist/LightRadius;
