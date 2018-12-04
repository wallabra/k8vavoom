#version 120

attribute vec2 TexCoord;
attribute vec2 TexCoord2;

varying vec2 TextureCoordinate;
varying vec2 Texture2Coordinate;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;

  // pass texture coordinates
  TextureCoordinate = TexCoord;
  Texture2Coordinate = TexCoord2;
}
