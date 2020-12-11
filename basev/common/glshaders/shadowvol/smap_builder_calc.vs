// `Vert` should contain vertex in *world* coordinates (unmodified by view matrix)

  gl_Position = LightMPV*Vert;
  VertToLight = Vert.xyz-LightPos;
