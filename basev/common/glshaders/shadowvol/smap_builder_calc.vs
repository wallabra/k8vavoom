// `Vert` should contain vertex in *world* coordinates (unmodified by view matrix)

  gl_Position = LightMPV*Vert;
  //VertWorldPos = Vert.xyz;
  VertToLight = Vert.xyz-LightPos;
