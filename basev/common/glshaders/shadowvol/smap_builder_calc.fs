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
  dist += sign(floor(LightRadius/800.0))*1.5; // big light offset
  //dist += 0.18;
  //dist += CubeSize/1024.0+0.055;
  //dist += CubeSize/2048.0+0.058*2;
  dist += 0.18+(CubeSize-128.0)/4096.0;
  //dist += 0.18;
  #endif
  if (dist >= LightRadius) discard;
  gl_FragColor.r = dist/LightRadius;
