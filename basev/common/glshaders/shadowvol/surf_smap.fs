#version 120
$include "common/common.inc"

uniform vec3 LightPos;
uniform float LightRadius;

varying vec3 VertWorldPos;

#ifdef VV_SMAP_TEXTURED
uniform sampler2D Texture;
$include "common/texture_vars.fs"
#endif


void main () {
  //gl_FragDepth = gl_FragCoord.z;
  //gl_FragColor.r = gl_FragCoord.z;
  //gl_FragColor = vec4(length(VertLightDir), gl_FragCoord.z, gl_FragCoord.w, 1.0);
  //gl_FragColor = vec4(length(VertLightDir), 0.0, 0.0, 1.0);
  //vec4 fc = vec4(length(VertLightDir), 0.0, 0.0, 1.0);
  //if (VertLightDir.z < 0) fc.g = 1.0;
  //if (VertLightDir.y > 0) fc.g = 1.0;
  vec4 fc = vec4(0.0, 0.0, 0.0, 1.0);
#ifdef VV_SMAP_TEXTURED
  vec4 TexColor = GetStdTexelSimpleShade(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard; //FIXME
  if (TexColor.a < ALPHA_MASKED) {
    discard; // only normal and masked walls should go thru this
    //fc.r = 99999.0;
    //fc.g = 1.0;
  } else
#endif
  {
    float dist = distance(LightPos, VertWorldPos);
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
