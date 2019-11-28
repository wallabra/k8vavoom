#version 120
$include "common/common.inc"
// Kawase blur filter
// for an explanation of how this works, see these references:
// https://software.intel.com/en-us/blogs/2014/07/15/an-investigation-of-fast-real-time-gpu-based-image-blur-algorithms
// http://www.daionet.gr.jp/~masa/archives/GDC2003_DSTEAL.ppt

varying vec2 TexCoord1;
varying vec2 TexCoord2;
varying vec2 TexCoord3;
varying vec2 TexCoord4;

// scale should be desired blur size (i.e. 9 for 9x9 blur)/2-1.
// since the desired blur size is always an odd number, scale always has a
// fractional part of 0.5. blurs are created by running successive Kawase
// filters at increasing scales until the desired size is reached.
// divide scale value by resolution of the input texture to get this scale vector.
uniform vec2 ScaleU;


void main () {
  //gl_Position = ftransform(); // this is the same as the code below
  //gl_Position = gl_ProjectionMatrix*gl_ModelViewMatrix*gl_Vertex;

  // transforming the vertex (k8vavoom does it this way)
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;

  // if we do all this math here, and let GLSL do its built-in interpolation
  // of varying variables, the math still comes out right, but it is faster
  TexCoord1 = gl_MultiTexCoord0.xy+ScaleU*vec2(-1, -1);
  TexCoord2 = gl_MultiTexCoord0.xy+ScaleU*vec2(-1, 1);
  TexCoord3 = gl_MultiTexCoord0.xy+ScaleU*vec2(1, -1);
  TexCoord4 = gl_MultiTexCoord0.xy+ScaleU*vec2(1, 1);
}
