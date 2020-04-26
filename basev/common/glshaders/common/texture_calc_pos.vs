  // calculate texture coordinates
  TextureCoordinate = vec2(
    (dot(Position, SAxis)+SOffs)*TexIW,
    (dot(Position, TAxis)+TOffs)*TexIH
  );
