#version 120
$include "common/common.inc"

uniform sampler2D Texture;

uniform vec3 LightPos;
uniform float LightRadius;

varying vec3 VertWorldPos;

varying vec2 TextureCoordinate;


void main () {
  //gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
  vec4 fc = vec4(0.0, 0.0, 0.0, 1.0);
//#ifdef VV_SMAP_TEXTURED
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard; //FIXME
  if (TexColor.a < ALPHA_MASKED) {
    discard; // only normal and masked walls should go thru this
    //fc.r = 99999.0;
    //fc.g = 1.0;
  } else
//#endif
  {
    float dist = distance(LightPos, VertWorldPos)+2;
    if (dist >= LightRadius) {
      fc.r = 99999.0;
      fc.b = 1.0;
    } else {
      fc.r = dist/LightRadius;
    }
  }
  //fc.rgb = vec3(TexColor.rgb);
  gl_FragColor = fc;
}
