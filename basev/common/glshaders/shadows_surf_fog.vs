#version 120


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
}
