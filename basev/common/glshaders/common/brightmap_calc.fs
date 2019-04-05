  vec4 BMColor = texture2D(TextureBM, TextureCoordinate);
  BMColor.rgb *= BMColor.a;
#if 0
  lt.rgb = max(lt.rgb, BMColor.rgb);
  //lt.rgb = BMColor.rgb;
#else
  // additive brightmaps
  //lt.rgb = min(lt.rgb+BMColor.rgb, 1.0);

  // for additive, this will be added light, otherwise this is 0
  vec3 ltA = (lt.rgb+BMColor.rgb)*BrightMapAdditive;

  // for additive, this is 0, otherwise this is max of both
  vec3 ltM = max(lt.rgb, BMColor.rgb)*(1.0-BrightMapAdditive);

  // choose one of them
  lt.rgb = max(ltA, ltM);

  // sanitise
  lt.rgb = min(lt.rgb, 1.0);

  //lt.rgb = BMColor.rgb;
  //TexColour.rgb = vec3(1.0, 1.0, 1.0);
#endif
