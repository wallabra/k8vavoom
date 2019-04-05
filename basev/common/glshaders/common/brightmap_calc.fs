  vec4 BMColor = texture2D(TextureBM, TextureCoordinate);
  BMColor.rgb *= BMColor.a;
#if 1
  lt.rgb = max(lt.rgb, BMColor.rgb);
  //lt.rgb = BMColor.rgb;
#else
  // additive brightmaps
  lt.rgb = min(lt.rgb+BMColor.rgb, 1.0);
  //lt.rgb = BMColor.rgb;
  //TexColour.rgb = vec3(1.0, 1.0, 1.0);
#endif
