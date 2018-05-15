#version 120

// vertex shader for simple (non-lightmapped) surfaces

uniform vec3 SAxis;
uniform vec3 TAxis;
uniform float SOffs;
uniform float TOffs;
uniform float TexIW;
uniform float TexIH;
uniform bool IsLightmap;

varying vec2 TextureCoordinate;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;

  TextureCoordinate = gl_MultiTexCoord0.xy;
}
