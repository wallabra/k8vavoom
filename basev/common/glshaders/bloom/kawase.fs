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
uniform sampler2D TextureSource;


void main () {
  // .a should be 1.0 here, otherwise unrendered parts will be grayed
  vec4 clr =
    (texture2D(TextureSource, TexCoord1)+
     texture2D(TextureSource, TexCoord2)+
     texture2D(TextureSource, TexCoord3)+
     texture2D(TextureSource, TexCoord4))/4.0;
  gl_FragColor = vec4(clr.rgb, 1.0);
  //gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
