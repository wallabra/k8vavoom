  // calculate common values
  float s = dot(gl_Vertex.xyz, SAxis)+SOffs;
  float t = dot(gl_Vertex.xyz, TAxis)+TOffs;

  // calculate texture coordinates
  TextureCoordinate = vec2(s*TexIW, t*TexIH);

  // calculate lightmap coordinates
  LightmapCoordinate = vec2(
    (s-TexMinS+CacheS*16.0+8.0)/2048.0,
    (t-TexMinT+CacheT*16.0+8.0)/2048.0
  );
