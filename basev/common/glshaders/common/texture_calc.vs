  // calculate texture coordinates
  TextureCoordinate = vec2(
    (dot(gl_Vertex.xyz, SAxis)+SOffs)*TexIW,
    (dot(gl_Vertex.xyz, TAxis)+TOffs)*TexIH
  );
