#version 120

attribute vec2 TexCoord;

varying vec2 TextureCoordinate;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;

  // pass texture coordinates
  TextureCoordinate = TexCoord;
}
