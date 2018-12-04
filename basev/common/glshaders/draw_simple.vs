#version 120

varying vec2 TextureCoordinate;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;

  // pass texture coordinates
  TextureCoordinate = gl_MultiTexCoord0.xy;
}
