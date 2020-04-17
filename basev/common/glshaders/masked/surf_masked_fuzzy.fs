#version 120
$include "common/common.inc"

uniform sampler2D Texture;
//uniform vec4 Light;
uniform float AlphaRef;
uniform float Time;

$include "common/fog_vars.fs"

varying vec2 TextureCoordinate;


// based on GZDoom shaders
void main () {
  // no need to calculate shading here
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  if (TexColor.a < AlphaRef) discard;

  //vec4 lt = Light;
  //TexColor.rgb *= lt.rgb;

  // black-stencil it
  /*
  vec4 FinalColor;
  FinalColor.a = clamp(TexColor.a*lt.a, 0.0, 1.0);
  FinalColor.rgb = vec3(0.0, 0.0, 0.0);
  */
  vec4 FinalColor;
  float xtime = Time/2.0;

  #if 0
  /* smoothnoise */
  float texX = sin(mod(TextureCoordinate.x*100.0+xtime*5.0, 3.489))+TextureCoordinate.x/4.0;
  float texY = cos(mod(TextureCoordinate.y*100.0+xtime*5.0, 3.489))+TextureCoordinate.y/4.0;
  float vX = (texX/texY)*21.0;
  float vY = (texY/texX)*13.0;
  float test = mod(xtime*2.0+(vX+vY), 0.5);
  #endif

  #if 1
  /* smooth */
  float texX = TextureCoordinate.x/3.0+0.66;
  float texY = 0.34-TextureCoordinate.y/3.0;
  float vX = (texX/texY)*21.0;
  float vY = (texY/texX)*13.0;
  float test = mod(xtime*2.0+(vX+vY), 0.5);
  #endif

  #if 0
  /* jagged */
  vec2 texSplat;
  const float pi = 3.14159265358979323846;
  texSplat.x = TextureCoordinate.x+mod(sin(pi*2.0*(TextureCoordinate.y+xtime*2.0)),0.1)*0.1;
  texSplat.y = TextureCoordinate.y+mod(cos(pi*2.0*(TextureCoordinate.x+xtime*2.0)),0.1)*0.1;

  float texX = sin(TextureCoordinate.x*100.0+xtime*5.0);
  float texY = cos(TextureCoordinate.x*100.0+xtime*5.0);
  float vX = (texX/texY)*21.0;
  float vY = (texY/texX)*13.0;
  float test = mod(xtime*2.0+(vX+vY), 0.5);
  #endif

  FinalColor.a = TexColor.a*(test*1.6);
  FinalColor.rgb = vec3(0.0,0.0,0.0);
  $include "common/fog_calc.fs"

  gl_FragColor = clamp(FinalColor, 0.0, 1.0);
}
