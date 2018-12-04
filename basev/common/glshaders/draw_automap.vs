#version 120

varying vec4 Colour;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;

  // pass color
  Colour = gl_Color;
}
