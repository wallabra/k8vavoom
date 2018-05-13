#version 120

// vertex shader for simple (non-lightmapped) surfaces

uniform vec3 SAxis;
uniform vec3 TAxis;
uniform float SOffs;
uniform float TOffs;
uniform float TexIW;
uniform float TexIH;

varying vec2 TextureCoordinate;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;

  // calculate texture coordinates
  vec2 st = vec2(
    (dot(gl_Vertex.xyz, SAxis)+SOffs)*TexIW,
    (dot(gl_Vertex.xyz, TAxis)+TOffs)*TexIH
  );

  TextureCoordinate = st;
}
